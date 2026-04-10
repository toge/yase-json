#ifndef __YASE_JSON_CRUSH_HPP__
#define __YASE_JSON_CRUSH_HPP__

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yase_json {

namespace detail {

auto constexpr JSON_CRUSH_DELIMITER = char16_t{u'\x0001'};

template <typename CharT>
struct JSCrushResult {
  std::basic_string<CharT> crushed;
  std::basic_string<CharT> split;
};

template <typename CharT>
struct OrderedCandidate {
  std::basic_string<CharT> value;
  int64_t count = 0;
  int64_t encoded_length = 0;
};

struct SubstringMatches {
  size_t first_pos = 0;
  int64_t count = 0;
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
  if (value <= 0x7F) return 1;
  if (value <= 0x7FF) return 2;
  if (value <= 0xFFFF) return 3;
  return 4;
};

auto const append_utf16 = [](std::u16string& output, char32_t const value) {
  if (value <= 0xFFFF) {
    output.push_back(static_cast<char16_t>(value));
  } else {
    auto const shifted = value - 0x10000;
    output.push_back(static_cast<char16_t>(0xD800 + (shifted >> 10)));
    output.push_back(static_cast<char16_t>(0xDC00 + (shifted & 0x3FF)));
  }
};

auto const append_utf8 = [](std::string& output, char32_t const value) {
  if (value <= 0x7F) {
    output.push_back(static_cast<char>(value));
  } else if (value <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (value >> 6)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  } else if (value <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (value >> 12)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  } else {
    output.push_back(static_cast<char>(0xF0 | (value >> 18)));
    output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  }
};

auto const decode_utf8_code_point = [](std::string_view const input, size_t& index) -> char32_t {
  auto const lead = static_cast<unsigned char>(input[index]);
  if (lead <= 0x7F) {
    ++index;
    return static_cast<char32_t>(lead);
  }
  auto read_continuation = [&](size_t const offset) -> unsigned char {
    if (index + offset >= input.size()) throw std::runtime_error("Invalid UTF-8");
    auto const byte = static_cast<unsigned char>(input[index + offset]);
    if ((byte & 0xC0) != 0x80) throw std::runtime_error("Invalid UTF-8");
    return byte;
  };
  if ((lead & 0xE0) == 0xC0) {
    auto const b1 = read_continuation(1);
    auto const v = static_cast<char32_t>(((lead & 0x1F) << 6) | (b1 & 0x3F));
    index += 2; return v;
  }
  if ((lead & 0xF0) == 0xE0) {
    auto const b1 = read_continuation(1); auto const b2 = read_continuation(2);
    auto const v = static_cast<char32_t>(((lead & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
    index += 3; return v;
  }
  if ((lead & 0xF8) == 0xF0) {
    auto const b1 = read_continuation(1); auto const b2 = read_continuation(2); auto const b3 = read_continuation(3);
    auto const v = static_cast<char32_t>(((lead & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));
    index += 4; return v;
  }
  throw std::runtime_error("Invalid UTF-8");
};

auto const utf8_to_utf16 = [](std::string_view const input) -> std::u16string {
  auto output = std::u16string{};
  output.reserve(input.size());
  size_t index = 0;
  while (index < input.size()) append_utf16(output, decode_utf8_code_point(input, index));
  return output;
};

auto const utf16_to_utf8 = [](std::u16string_view const input) -> std::string {
  auto output = std::string{};
  output.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    auto const v = input[i];
    if (is_high_surrogate(v)) {
      if (i + 1 >= input.size() || !is_low_surrogate(input[i + 1])) throw std::runtime_error("Invalid UTF-16");
      append_utf8(output, 0x10000 + ((static_cast<char32_t>(v - 0xD800) << 10) | (input[i + 1] - 0xDC00)));
      ++i;
    } else {
      append_utf8(output, static_cast<char32_t>(v));
    }
  }
  return output;
};

auto const encoded_uri_length = [](std::u16string_view const input) -> int64_t {
  int64_t length = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    char32_t cp = input[i];
    if (is_high_surrogate(input[i])) {
      if (i + 1 >= input.size() || !is_low_surrogate(input[i + 1])) throw std::runtime_error("Invalid UTF-16");
      cp = 0x10000 + ((static_cast<char32_t>(input[i] - 0xD800) << 10) | (input[i + 1] - 0xDC00));
      ++i;
    }
    if (is_uri_unescaped(cp)) ++length;
    else length += 3 * utf8_length(cp);
  }
  return length;
};

auto constexpr has_unmatched_surrogate = [](std::u16string_view const input) {
  if (input.empty()) return false;
  return is_low_surrogate(input.front()) || is_high_surrogate(input.back());
};

template <typename CharT>
auto split_on_char(std::basic_string_view<CharT> const input, CharT const delimiter)
  -> std::vector<std::basic_string<CharT>> {
  auto parts = std::vector<std::basic_string<CharT>>{};
  size_t start = 0;
  while (true) {
    auto const pos = input.find(delimiter, start);
    if (pos == std::basic_string_view<CharT>::npos) {
      parts.emplace_back(input.substr(start));
      return parts;
    }
    parts.emplace_back(input.substr(start, pos - start));
    start = pos + 1;
  }
}

template <typename CharT>
auto join_strings(std::vector<std::basic_string<CharT>> const& parts,
                  std::basic_string_view<CharT> const separator)
  -> std::basic_string<CharT> {
  if (parts.empty()) return {};
  size_t total_size = (parts.size() > 1 ? (parts.size() - 1) * separator.size() : 0);
  for (auto const& s : parts) total_size += s.size();
  std::basic_string<CharT> output;
  output.reserve(total_size);
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) output.append(separator);
    output.append(parts[i]);
  }
  return output;
}

template <typename CharT>
auto swap_internal(std::basic_string_view<CharT> const input,
                   std::basic_string_view<CharT> const left,
                   std::basic_string_view<CharT> const right)
  -> std::basic_string<CharT> {
  std::basic_string<CharT> output;
  output.reserve(input.size());
  size_t pos = 0;
  while (pos < input.size()) {
    if (pos + left.size() <= input.size() && input.substr(pos, left.size()) == left) {
      output.append(right); pos += left.size();
    } else if (pos + right.size() <= input.size() && input.substr(pos, right.size()) == right) {
      output.append(left); pos += right.size();
    } else {
      output.push_back(input[pos++]);
    }
  }
  return output;
}

auto const json_crush_swap = [](std::u16string_view const input, bool const forward = true) {
  auto string = std::u16string{input};
  auto const groups = std::array{
    std::pair{std::u16string_view{u"\""}, std::u16string_view{u"'"}},
    std::pair{std::u16string_view{u"':"}, std::u16string_view{u"!"}},
    std::pair{std::u16string_view{u",'"}, std::u16string_view{u"~"}},
    std::pair{std::u16string_view{u"}"}, std::u16string_view{u")"}},
    std::pair{std::u16string_view{u"{"}, std::u16string_view{u"("}},
  };
  if (forward) {
    for (auto const& [l, r] : groups) string = swap_internal(std::u16string_view{string}, l, r);
  } else {
    for (int i = 4; i >= 0; --i) string = swap_internal(std::u16string_view{string}, groups[i].first, groups[i].second);
  }
  return string;
};

auto const replacement_characters_utf16 = [] {
  std::u16string chars;
  std::u16string_view unescaped = u"-_.!~*'()";
  for (int i = 126; i > 0; --i) {
    char16_t c = static_cast<char16_t>(i);
    if ((i >= 48 && i <= 57) || (i >= 65 && i <= 90) || (i >= 97 && i <= 122) || unescaped.find(c) != std::u16string_view::npos)
      chars.push_back(c);
  }
  for (int i = 32; i < 255; ++i) {
    char16_t c = static_cast<char16_t>(i);
    if (c != u'\\' && chars.find(c) == std::u16string::npos) chars.insert(chars.begin(), c);
  }
  return chars;
}();

auto const count_non_overlapping_substrings = [](std::u16string_view const string,
                                                 std::u16string_view const substring,
                                                 size_t const start_pos,
                                                 int64_t const initial_count) {
  auto count = initial_count;
  for (auto pos = string.find(substring, start_pos);
       pos != std::u16string_view::npos;
       pos = string.find(substring, pos + substring.size())) {
    ++count;
  }
  return count;
};

auto const find_non_overlapping_substrings = [](std::u16string_view const string,
                                                std::u16string_view const substring) {
  auto matches = SubstringMatches{};
  if (substring.empty()) {
    return matches;
  }

  for (auto pos = string.find(substring, 0);
       pos != std::u16string_view::npos;
       pos = string.find(substring, pos + substring.size())) {
    if (matches.count == 0) {
      matches.first_pos = pos;
    }
    ++matches.count;
  }
  return matches;
};

auto const collect_non_overlapping_match_positions = [](
  std::u16string_view const input,
  std::u16string_view const target,
  std::vector<size_t>& positions) {
  positions.clear();
  if (target.empty()) {
    return;
  }

  for (auto pos = input.find(target, 0);
       pos != std::u16string_view::npos;
       pos = input.find(target, pos + target.size())) {
    positions.push_back(pos);
  }
};

auto const write_replaced_with_char = [](
  std::u16string_view const input,
  std::u16string_view const target,
  char16_t const replacement,
  std::vector<size_t> const& positions,
  std::u16string& output) {
  output.clear();
  if (positions.empty()) {
    output.assign(input);
    return;
  }

  output.reserve(input.size() - positions.size() * (target.size() - 1));
  auto last = size_t{0};
  for (auto const pos : positions) {
    output.append(input.substr(last, pos - last));
    output.push_back(replacement);
    last = pos + target.size();
  }
  output.append(input.substr(last));
};

auto const build_initial_candidates = [](std::u16string_view const string, int64_t const max_len) {
  auto candidates = std::vector<OrderedCandidate<char16_t>>{};
  // Keys are views into `string`, which stays alive for the lifetime of this map.
  auto seen = std::unordered_map<std::u16string_view, size_t>{};

  if (string.size() < 2 || max_len <= 2) {
    return candidates;
  }

  auto const upper_length = std::min<size_t>(string.size(), static_cast<size_t>(max_len));
  for (auto const substring_length : std::views::iota(size_t{2}, upper_length)) {
    auto const start_limit = string.size() - substring_length;
    for (auto const i : std::views::iota(size_t{0}, start_limit)) {
      auto const substring = string.substr(i, substring_length);
      if (seen.contains(substring) || has_unmatched_surrogate(substring)) {
        continue;
      }

      auto const count = count_non_overlapping_substrings(
        string, substring, i + substring_length, 1);
      seen.emplace(substring, candidates.size());

      if (count > 1) {
        auto const encoded_length = encoded_uri_length(substring);
        candidates.push_back({
          std::u16string{substring},
          count,
          encoded_length
        });
      }
    }
  }

  return candidates;
};

auto const rebuild_candidates = [](std::u16string_view const string,
                                   std::vector<OrderedCandidate<char16_t>> const& previous,
                                   std::u16string_view const replaced,
                                   char16_t const replacement) {
  auto candidates = std::vector<OrderedCandidate<char16_t>>{};
  // Keys are views into `string`, which stays alive for the lifetime of this map.
  auto seen = std::unordered_map<std::u16string_view, size_t>{};
  auto rewritten = std::u16string{};
  auto positions = std::vector<size_t>{};

  for (auto const& candidate : previous) {
    auto rewritten_view = std::u16string_view{candidate.value};
    collect_non_overlapping_match_positions(candidate.value, replaced, positions);
    if (!positions.empty()) {
      write_replaced_with_char(candidate.value, replaced, replacement, positions, rewritten);
      rewritten_view = rewritten;
    }

    auto const matches = find_non_overlapping_substrings(string, rewritten_view);
    if (matches.count <= 1) {
      continue;
    }

    auto const accepted = string.substr(matches.first_pos, rewritten_view.size());
    if (auto const it = seen.find(accepted); it != seen.end()) {
      candidates[it->second].count = matches.count;
      continue;
    }

    seen.emplace(accepted, candidates.size());
    auto const encoded_length = encoded_uri_length(accepted);
    candidates.push_back({
      std::u16string{accepted},
      matches.count,
      encoded_length
    });
  }

  return candidates;
};

auto const js_crush_utf16 = [](std::u16string string, int64_t const max_len = 50) {
  std::u16string split_string;
  auto candidates = build_initial_candidates(string, max_len);
  auto replace_pos = static_cast<int64_t>(replacement_characters_utf16.size());
  auto replacement_positions = std::vector<size_t>{};
  auto rewritten_string = std::u16string{};

  while (true) {
    char16_t replace_char = 0;
    while (true) {
      if (replace_pos == 0) {
        replace_pos = -1;
        break;
      }
      auto const candidate = replacement_characters_utf16[--replace_pos];
      if (string.find(candidate) == std::u16string::npos) {
        replace_char = candidate;
        break;
      }
    }
    if (replace_pos < 0) {
      break;
    }

    auto const replace_length = encoded_uri_length(std::u16string_view{&replace_char, 1});
    auto const delimiter_length = encoded_uri_length(std::u16string_view{&JSON_CRUSH_DELIMITER, 1});
    auto best_delta = int64_t{0};
    auto best_index = size_t{0};
    auto filtered = std::vector<OrderedCandidate<char16_t>>{};
    filtered.reserve(candidates.size());

    for (auto const& candidate : candidates) {
      auto delta = (candidate.count - 1) * candidate.encoded_length - (candidate.count + 1) * replace_length;
      if (split_string.empty()) {
        delta -= delimiter_length;
      }
      if (delta <= 0) {
        continue;
      }
      if (delta > best_delta) {
        best_delta = delta;
        best_index = filtered.size();
      }
      filtered.push_back(candidate);
    }

    if (filtered.empty()) {
      break;
    }

    auto const& best_sub = filtered[best_index].value;
    collect_non_overlapping_match_positions(string, best_sub, replacement_positions);
    write_replaced_with_char(string, best_sub, replace_char, replacement_positions, rewritten_string);
    string.swap(rewritten_string);
    string.push_back(replace_char);
    string.append(best_sub);
    split_string.insert(split_string.begin(), replace_char);
    candidates = rebuild_candidates(string, filtered, best_sub, replace_char);
  }
  return JSCrushResult<char16_t>{std::move(string), std::move(split_string)};
};

} // namespace detail

auto crush(std::string_view input) -> std::string {
  auto string = detail::utf8_to_utf16(input);
  string.erase(std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER), string.end());
  string = detail::json_crush_swap(string);
  auto crushed = detail::js_crush_utf16(std::move(string));
  auto output = std::move(crushed.crushed);
  if (!crushed.split.empty()) { output.push_back(detail::JSON_CRUSH_DELIMITER); output += crushed.split; }
  output.push_back(u'_');
  return detail::utf16_to_utf8(output);
}

auto uncrush(std::string_view input) -> std::string {
  auto string = detail::utf8_to_utf16(input);
  if (!string.empty()) string.pop_back();
  auto parts = detail::split_on_char(std::u16string_view{string}, detail::JSON_CRUSH_DELIMITER);
  auto uncrushed = parts.empty() ? std::u16string{} : parts.front();
  if (parts.size() > 1) {
    for (auto const replacement : parts[1]) {
      auto split_array = detail::split_on_char(std::u16string_view{uncrushed}, replacement);
      auto const original = split_array.empty() ? std::u16string{} : split_array.back();
      if (!split_array.empty()) split_array.pop_back();
      uncrushed = detail::join_strings(split_array, std::u16string_view{original});
    }
  }
  return detail::utf16_to_utf8(detail::json_crush_swap(uncrushed, false));
}

} // namespace yase_json

#endif // __YASE_JSON_CRUSH_HPP__
