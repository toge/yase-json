#ifndef __YASE_JSON_CRUSH_HPP__
#define __YASE_JSON_CRUSH_HPP__

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yase_json {

namespace detail {

auto constexpr JSON_CRUSH_DELIMITER = char16_t{u'\x0001'};

struct JSCrushResult {
  std::u16string crushed;
  std::u16string split;
};

auto constexpr is_high_surrogate = [](char16_t const value) {
  return value >= 0xD800 && value <= 0xDBFF;
};

auto constexpr is_low_surrogate = [](char16_t const value) {
  return value >= 0xDC00 && value <= 0xDFFF;
};

auto constexpr is_uri_unescaped = [](char32_t const value) {
  return (value >= U'0' && value <= U'9') ||
         (value >= U'A' && value <= U'Z') ||
         (value >= U'a' && value <= U'z') ||
         value == U'-' || value == U'_' || value == U'.' || value == U'!' ||
         value == U'~' || value == U'*' || value == U'\'' || value == U'(' ||
         value == U')';
};

auto constexpr utf8_length = [](char32_t const value) -> int64_t {
  if (value <= 0x7F) {
    return 1;
  }
  if (value <= 0x7FF) {
    return 2;
  }
  if (value <= 0xFFFF) {
    return 3;
  }
  return 4;
};

auto const append_utf16 = [](std::u16string& output, char32_t const value) {
  if (value <= 0xFFFF) {
    output.push_back(static_cast<char16_t>(value));
    return;
  }

  auto const shifted = value - 0x10000;
  output.push_back(static_cast<char16_t>(0xD800 + (shifted >> 10)));
  output.push_back(static_cast<char16_t>(0xDC00 + (shifted & 0x3FF)));
};

auto const append_utf8 = [](std::string& output, char32_t const value) {
  if (value <= 0x7F) {
    output.push_back(static_cast<char>(value));
    return;
  }
  if (value <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (value >> 6)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    return;
  }
  if (value <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (value >> 12)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    return;
  }

  output.push_back(static_cast<char>(0xF0 | (value >> 18)));
  output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
};

auto const decode_utf8_code_point = [](std::string_view const input, size_t& index) -> char32_t {
  auto const fail = [] {
    throw std::runtime_error("Invalid UTF-8 input for JSONCrush");
  };

  auto const lead = static_cast<unsigned char>(input[index]);
  if (lead <= 0x7F) {
    ++index;
    return static_cast<char32_t>(lead);
  }

  auto read_continuation = [&](size_t const offset) -> unsigned char {
    if (index + offset >= input.size()) {
      fail();
    }
    auto const byte = static_cast<unsigned char>(input[index + offset]);
    if ((byte & 0xC0) != 0x80) {
      fail();
    }
    return byte;
  };

  if ((lead & 0xE0) == 0xC0) {
    auto const byte1 = read_continuation(1);
    auto const value = static_cast<char32_t>(((lead & 0x1F) << 6) | (byte1 & 0x3F));
    if (value < 0x80) {
      fail();
    }
    index += 2;
    return value;
  }

  if ((lead & 0xF0) == 0xE0) {
    auto const byte1 = read_continuation(1);
    auto const byte2 = read_continuation(2);
    auto const value = static_cast<char32_t>(((lead & 0x0F) << 12) |
                                             ((byte1 & 0x3F) << 6) |
                                             (byte2 & 0x3F));
    if (value < 0x800 || (value >= 0xD800 && value <= 0xDFFF)) {
      fail();
    }
    index += 3;
    return value;
  }

  if ((lead & 0xF8) == 0xF0) {
    auto const byte1 = read_continuation(1);
    auto const byte2 = read_continuation(2);
    auto const byte3 = read_continuation(3);
    auto const value = static_cast<char32_t>(((lead & 0x07) << 18) |
                                             ((byte1 & 0x3F) << 12) |
                                             ((byte2 & 0x3F) << 6) |
                                             (byte3 & 0x3F));
    if (value < 0x10000 || value > 0x10FFFF) {
      fail();
    }
    index += 4;
    return value;
  }

  fail();
  return char32_t{};
};

auto const utf8_to_utf16 = [](std::string_view const input) -> std::u16string {
  auto output = std::u16string{};
  output.reserve(input.size());

  auto index = size_t{0};
  while (index < input.size()) {
    append_utf16(output, decode_utf8_code_point(input, index));
  }
  return output;
};

auto const utf16_to_utf8 = [](std::u16string_view const input) -> std::string {
  auto output = std::string{};
  output.reserve(input.size());

  for (auto index = size_t{0}; index < input.size(); ++index) {
    auto const value = input[index];
    if (is_high_surrogate(value)) {
      if (index + 1 >= input.size() || !is_low_surrogate(input[index + 1])) {
        throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
      }
      auto const code_point = static_cast<char32_t>(
        0x10000 +
        ((static_cast<char32_t>(value - 0xD800) << 10) |
         static_cast<char32_t>(input[index + 1] - 0xDC00)));
      append_utf8(output, code_point);
      ++index;
      continue;
    }
    if (is_low_surrogate(value)) {
      throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
    }
    append_utf8(output, static_cast<char32_t>(value));
  }

  return output;
};

auto const encoded_uri_length = [](std::u16string_view const input) -> int64_t {
  auto length = int64_t{0};

  for (auto index = size_t{0}; index < input.size(); ++index) {
    auto code_point = char32_t{input[index]};
    if (is_high_surrogate(input[index])) {
      if (index + 1 >= input.size() || !is_low_surrogate(input[index + 1])) {
        throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
      }
      code_point = static_cast<char32_t>(
        0x10000 +
        ((static_cast<char32_t>(input[index] - 0xD800) << 10) |
         static_cast<char32_t>(input[index + 1] - 0xDC00)));
      ++index;
    } else if (is_low_surrogate(input[index])) {
      throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
    }

    if (is_uri_unescaped(code_point)) {
      ++length;
    } else {
      length += 3 * utf8_length(code_point);
    }
  }

  return length;
};

auto constexpr has_unmatched_surrogate = [](std::u16string_view const input) {
  if (input.empty()) {
    return false;
  }
  return is_low_surrogate(input.front()) || is_high_surrogate(input.back());
};

auto constexpr contains_char = [](std::u16string_view const input, char16_t const value) {
  return input.find(value) != std::u16string_view::npos;
};

auto const split_on_char = [](std::u16string_view const input, char16_t const delimiter) {
  auto parts = std::vector<std::u16string>{};
  auto start = size_t{0};

  while (true) {
    auto const pos = input.find(delimiter, start);
    if (pos == std::u16string_view::npos) {
      parts.emplace_back(input.substr(start));
      return parts;
    }
    parts.emplace_back(input.substr(start, pos - start));
    start = pos + 1;
  }
};

auto const join_strings = [](std::vector<std::u16string> const& parts, std::u16string_view const separator) {
  auto output = std::u16string{};
  for (auto const index : std::views::iota(size_t{0}, parts.size())) {
    if (index != 0) {
      output += separator;
    }
    output += parts[index];
  }
  return output;
};

auto const replace_all = [](std::u16string_view const input,
                            std::u16string_view const target,
                            std::u16string_view const replacement) {
  if (target.empty()) {
    return std::u16string{input};
  }

  auto output = std::u16string{};
  auto position = size_t{0};
  while (position < input.size()) {
    auto const found = input.find(target, position);
    if (found == std::u16string_view::npos) {
      output.append(input.substr(position));
      return output;
    }

    output.append(input.substr(position, found - position));
    output.append(replacement);
    position = found + target.size();
  }
  return output;
};

auto const count_occurrences = [](std::u16string_view const input, std::u16string_view const substring) {
  if (substring.empty()) {
    return int64_t{0};
  }

  auto count = int64_t{0};
  for (auto position = input.find(substring);
       position != std::u16string_view::npos;
       position = input.find(substring, position + substring.size())) {
    ++count;
  }
  return count;
};

auto const swap_internal = [](std::u16string_view const input,
                              std::u16string_view const left,
                              std::u16string_view const right) {
  auto output = std::u16string{};
  auto position = size_t{0};

  while (position < input.size()) {
    if (position + left.size() <= input.size() &&
        input.substr(position, left.size()) == left) {
      output += right;
      position += left.size();
      continue;
    }
    if (position + right.size() <= input.size() &&
        input.substr(position, right.size()) == right) {
      output += left;
      position += right.size();
      continue;
    }

    output.push_back(input[position]);
    ++position;
  }

  return output;
};

auto const json_crush_swap = [](std::u16string_view const input, bool const forward = true) {
  auto string = std::u16string{input};
  auto const swap_groups = std::array{
    std::pair{std::u16string_view{u"\""}, std::u16string_view{u"'"}},
    std::pair{std::u16string_view{u"':"}, std::u16string_view{u"!"}},
    std::pair{std::u16string_view{u",'"}, std::u16string_view{u"~"}},
    std::pair{std::u16string_view{u"}"}, std::u16string_view{u")"}},
    std::pair{std::u16string_view{u"{"}, std::u16string_view{u"("}},
  };

  if (forward) {
    for (auto const& [left, right] : swap_groups) {
      string = swap_internal(string, left, right);
    }
    return string;
  }

  for (auto const index : std::views::iota(size_t{0}, swap_groups.size()) | std::views::reverse) {
    auto const& [left, right] = swap_groups[index];
    string = swap_internal(string, left, right);
  }
  return string;
};

auto const replacement_characters = [] {
  auto characters = std::u16string{};

  auto const unescaped = std::u16string_view{u"-_.!~*'()"};
  for (auto i = int{127}; --i;) {
    auto const c = static_cast<char16_t>(i);
    if ((i >= 48 && i <= 57) ||
        (i >= 65 && i <= 90) ||
        (i >= 97 && i <= 122) ||
        unescaped.find(c) != std::u16string_view::npos) {
      characters.push_back(c);
    }
  }

  for (auto const i : std::views::iota(32, 255)) {
    auto const c = static_cast<char16_t>(i);
    if (c != u'\\' && !contains_char(characters, c)) {
      characters.insert(characters.begin(), c);
    }
  }

  return characters;
}();

auto const js_crush = [](std::u16string string, int64_t const max_substring_length = 50) {
  auto replace_character_pos = replacement_characters.size();
  auto split_string = std::u16string{};

  auto substring_count = std::unordered_map<std::u16string, int64_t>{};
  for (auto substring_length = int64_t{2}; substring_length < max_substring_length; ++substring_length) {
    if (string.size() <= static_cast<size_t>(substring_length)) {
      break;
    }

    auto const max_start = string.size() - static_cast<size_t>(substring_length);
    for (auto const start : std::views::iota(size_t{0}, max_start)) {
      auto const substring = std::u16string{string.substr(start, static_cast<size_t>(substring_length))};
      if (substring_count.contains(substring) || has_unmatched_surrogate(substring)) {
        continue;
      }

      auto count = int64_t{1};
      for (auto position = string.find(substring, start + static_cast<size_t>(substring_length));
           position != std::u16string::npos;
           position = string.find(substring, position + static_cast<size_t>(substring_length))) {
        ++count;
      }

      if (count > 1) {
        substring_count.emplace(substring, count);
      }
    }
  }

  while (true) {
    auto has_replace_character = false;
    auto replace_character = char16_t{};
    while (replace_character_pos > 0) {
      --replace_character_pos;
      if (!contains_char(string, replacement_characters[replace_character_pos])) {
        replace_character = replacement_characters[replace_character_pos];
        has_replace_character = true;
        break;
      }
    }
    if (!has_replace_character) {
      break;
    }

    auto best_substring = std::u16string{};
    auto best_length_delta = int64_t{0};
    auto const replace_length = encoded_uri_length(std::u16string_view{&replace_character, 1});

    auto invalid_substrings = std::vector<std::u16string>{};
    invalid_substrings.reserve(substring_count.size());
    for (auto const& [substring, count] : substring_count) {
      auto length_delta = (count - 1) * encoded_uri_length(substring) - (count + 1) * replace_length;
      if (split_string.empty()) {
        length_delta -= encoded_uri_length(std::u16string_view{&JSON_CRUSH_DELIMITER, 1});
      }

      if (length_delta <= 0) {
        invalid_substrings.push_back(substring);
        continue;
      }
      if (length_delta > best_length_delta) {
        best_substring = substring;
        best_length_delta = length_delta;
      }
    }

    for (auto const& substring : invalid_substrings) {
      substring_count.erase(substring);
    }

    if (best_substring.empty()) {
      break;
    }

    auto const replacement = std::u16string_view{&replace_character, 1};
    string = replace_all(string, best_substring, replacement);
    string.push_back(replace_character);
    string += best_substring;
    split_string.insert(split_string.begin(), replace_character);

    auto new_substring_count = std::unordered_map<std::u16string, int64_t>{};
    new_substring_count.reserve(substring_count.size());
    for (auto const& [substring, _] : substring_count) {
      std::ignore = _;
      auto const rewritten = replace_all(substring, best_substring, replacement);
      auto const count = count_occurrences(string, rewritten);
      if (count > 1) {
        new_substring_count.emplace(std::move(rewritten), count);
      }
    }
    substring_count = std::move(new_substring_count);
  }

  return JSCrushResult{std::move(string), std::move(split_string)};
};

} // namespace detail

/**
 * @brief JSONCrush互換の文字列圧縮を行います。
 * @param input 圧縮対象の文字列。
 * @return JSONCrush.crush() 互換の圧縮文字列。
 * @throw std::runtime_error 入力が不正なUTF-8の場合。
 */
auto crush(std::string_view input) -> std::string {
  auto string = detail::utf8_to_utf16(input);
  string.erase(std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER), string.end());
  string = detail::json_crush_swap(string);

  auto crushed = detail::js_crush(std::move(string));
  auto output = std::move(crushed.crushed);
  if (!crushed.split.empty()) {
    output.push_back(detail::JSON_CRUSH_DELIMITER);
    output += crushed.split;
  }
  output.push_back(u'_');
  return detail::utf16_to_utf8(output);
}

/**
 * @brief JSONCrush互換の圧縮文字列を元に戻します。
 * @param input JSONCrush.crush() 互換の圧縮文字列。
 * @return 復元された文字列。
 * @throw std::runtime_error 入力が不正なUTF-8の場合。
 */
auto uncrush(std::string_view input) -> std::string {
  auto string = detail::utf8_to_utf16(input);
  if (!string.empty()) {
    string.pop_back();
  }

  auto parts = detail::split_on_char(string, detail::JSON_CRUSH_DELIMITER);
  auto uncrushed = parts.empty() ? std::u16string{} : parts.front();
  if (parts.size() > 1) {
    auto const split = parts[1];
    for (auto const replacement : split) {
      auto split_array = detail::split_on_char(uncrushed, replacement);
      auto const original = split_array.empty() ? std::u16string{} : split_array.back();
      if (!split_array.empty()) {
        split_array.pop_back();
      }
      uncrushed = detail::join_strings(split_array, original);
    }
  }

  return detail::utf16_to_utf8(detail::json_crush_swap(uncrushed, false));
}

} // namespace yase_json

#endif // __YASE_JSON_CRUSH_HPP__
