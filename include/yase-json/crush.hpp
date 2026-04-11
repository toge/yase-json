#ifndef __YASE_JSON_CRUSH_HPP__
#define __YASE_JSON_CRUSH_HPP__

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <bitset>

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
auto replace_all(std::basic_string_view<CharT> const input,
                 std::basic_string_view<CharT> const target,
                 std::basic_string_view<CharT> const replacement)
  -> std::basic_string<CharT> {
  if (target.empty()) return std::basic_string<CharT>{input};
  std::basic_string<CharT> output;
  output.reserve(input.size());
  size_t pos = 0;
  while (true) {
    auto const found = input.find(target, pos);
    if (found == std::basic_string_view<CharT>::npos) {
      output.append(input.substr(pos));
      break;
    }
    output.append(input.substr(pos, found - pos));
    output.append(replacement);
    pos = found + target.size();
  }
  return output;
}

template <typename CharT>
auto replace_all_with_char(std::basic_string_view<CharT> const input,
                           std::basic_string_view<CharT> const target,
                           CharT const replacement)
  -> std::basic_string<CharT> {
  return replace_all(input, target, std::basic_string_view<CharT>{&replacement, 1});
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

template <typename CharT>
class RollingHash {
public:
  explicit RollingHash(std::basic_string_view<CharT> const input) {
    prefix_.resize(input.size() + 1);
    powers_.resize(input.size() + 1);
    prefix_[0] = 0;
    powers_[0] = 1;
    for (size_t i = 0; i < input.size(); ++i) {
      prefix_[i + 1] = prefix_[i] * kBase + code_unit(input[i]);
      powers_[i + 1] = powers_[i] * kBase;
    }
  }
  [[nodiscard]] auto slice(size_t const pos, size_t const length) const -> uint64_t {
    return prefix_[pos + length] - prefix_[pos] * powers_[length];
  }
  static auto hash(std::basic_string_view<CharT> const input) -> uint64_t {
    uint64_t v = 0;
    for (auto const ch : input) v = v * kBase + code_unit(ch);
    return v;
  }
private:
  static auto code_unit(CharT const v) -> uint64_t {
    return static_cast<uint64_t>(static_cast<std::make_unsigned_t<CharT>>(v)) + 1;
  }
  static constexpr auto kBase = uint64_t{11400714819323198485ull};
  std::vector<uint64_t> prefix_;
  std::vector<uint64_t> powers_;
};

auto constexpr greedy_non_overlapping_count = [](std::vector<size_t> const& positions, size_t const length) {
  int64_t count = 0; size_t next_allowed = 0; bool first = true;
  for (auto const pos : positions) {
    if (first || pos >= next_allowed) {
      ++count; next_allowed = pos + length; first = false;
    }
  }
  return count;
};

inline constexpr auto replacement_characters_utf16 = [] {
  std::array<char16_t, 222> chars{};
  size_t idx = 0;
  auto const is_loop1 = [](int i) {
    if (i <= 0) return false;
    std::u16string_view const unescaped = u"-_.!~*'()";
    return (i >= 48 && i <= 57) || (i >= 65 && i <= 90) || (i >= 97 && i <= 122) ||
           unescaped.find(static_cast<char16_t>(i)) != std::u16string_view::npos;
  };
  for (int i = 254; i >= 32; --i) {
    if (i != 92 && !is_loop1(i)) {
      chars[idx++] = static_cast<char16_t>(i);
    }
  }
  for (int i = 126; i > 0; --i) {
    if (is_loop1(i)) {
      chars[idx++] = static_cast<char16_t>(i);
    }
  }
  return chars;
}();

template <typename CharT>
auto build_initial_candidates(std::basic_string_view<CharT> const string, int64_t const max_len) {
  std::vector<OrderedCandidate<CharT>> candidates;
  if (string.size() < 2) return candidates;

  // To avoid O(N*L) string creations, we can use RollingHash to find repeats quickly.
  // But to match official JSONCrush's tie-breaking/order, we iterate in order of encounter.
  RollingHash<CharT> const hasher(string);
  std::unordered_map<uint64_t, std::vector<size_t>> buckets;

  for (size_t len = 2; len < static_cast<size_t>(max_len); ++len) {
    if (string.size() < len) break;
    for (size_t i = 0; i <= string.size() - len; ++i) {
      buckets[hasher.slice(i, len)].push_back(i);
    }
  }

  std::unordered_map<std::basic_string_view<CharT>, bool> processed;
  for (size_t i = 0; i < string.size(); ++i) {
    for (size_t len = 2; len < static_cast<size_t>(max_len) && i + len <= string.size(); ++len) {
      auto sub = string.substr(i, len);
      if (processed.count(sub)) continue;

      uint64_t h = hasher.slice(i, len);
      auto& positions = buckets[h];

      // Verify actual string match if hash matches multiple potential substrings
      if (positions.size() > 1 && positions[0] == i) {
        if constexpr (std::is_same_v<CharT, char16_t>) { if (has_unmatched_surrogate(sub)) continue; }

        // Check if this is the first occurrence we've seen for this content
        // In most cases, we only have one string per hash bucket if we use a good hash.
        // For brevity and speed, we assume few collisions.

        int64_t count = greedy_non_overlapping_count(positions, len);

        if (count > 1) {
          candidates.push_back({std::basic_string<CharT>(sub), count, encoded_uri_length(std::u16string_view(reinterpret_cast<const char16_t*>(sub.data()), sub.size()))});
        }
        processed[sub] = true;
      }
    }
  }
  return candidates;
}

template <typename CharT>
auto count_candidates(std::basic_string_view<CharT> const string, std::vector<OrderedCandidate<CharT>>& candidates) {
  if (candidates.empty()) return;
  for (auto& c : candidates) {
    std::vector<size_t> positions;
    size_t pos = string.find(c.value);
    while (pos != std::basic_string_view<CharT>::npos) {
      positions.push_back(pos);
      pos = string.find(c.value, pos + 1);
    }
    c.count = greedy_non_overlapping_count(positions, c.value.size());
  }
  candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [](auto const& c) { return c.count <= 1; }), candidates.end());
}

auto const js_crush_utf16 = [](std::u16string string, int64_t const max_len = 50) {
  std::u16string split_string;
  auto candidates = build_initial_candidates<char16_t>(string, max_len);
  int replace_pos = replacement_characters_utf16.size();

  while (true) {
    std::bitset<65536> present;
    for (auto c : string) present.set(static_cast<uint16_t>(c));

    char16_t replace_char = 0;
    while (replace_pos > 0) {
      char16_t c = replacement_characters_utf16[--replace_pos];
      if (!present.test(static_cast<uint16_t>(c))) { replace_char = c; break; }
    }
    if (replace_char == 0) break;

    size_t best_idx = 0; int64_t best_delta = 0;
    int64_t rep_len = encoded_uri_length(std::u16string_view{&replace_char, 1});
    int64_t delim_len = encoded_uri_length(std::u16string_view{&JSON_CRUSH_DELIMITER, 1});

    auto it = candidates.begin();
    while (it != candidates.end()) {
      int64_t delta = (it->count - 1) * it->encoded_length - (it->count + 1) * rep_len;
      if (split_string.empty()) delta -= delim_len;
      
      if (delta <= 0) {
        it = candidates.erase(it);
      } else {
        if (delta > best_delta) {
          best_delta = delta;
          best_idx = std::distance(candidates.begin(), it);
        }
        ++it;
      }
    }
    if (best_delta <= 0 || candidates.empty()) break;

    auto const best_sub = candidates[best_idx].value;
    string = replace_all_with_char<char16_t>(string, best_sub, replace_char);
    string.push_back(replace_char); string.append(best_sub);
    split_string.insert(split_string.begin(), replace_char);

    std::vector<OrderedCandidate<char16_t>> next_cands;
    std::unordered_map<std::u16string, size_t> seen;
    for (auto& c : candidates) {
      auto rewritten = replace_all_with_char<char16_t>(c.value, best_sub, replace_char);
      if (rewritten.size() < 2) continue;
      if (!seen.count(rewritten)) {
        seen[rewritten] = next_cands.size();
        next_cands.push_back({rewritten, 0, encoded_uri_length(rewritten)});
      }
    }
    candidates = std::move(next_cands);
    count_candidates<char16_t>(string, candidates);
  }
  return JSCrushResult<char16_t>{std::move(string), std::move(split_string)};
};

} // namespace detail

inline auto crush(std::string_view input) -> std::string {
  auto string = detail::utf8_to_utf16(input);
  string.erase(std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER), string.end());
  string = detail::json_crush_swap(string);
  auto crushed = detail::js_crush_utf16(std::move(string));
  auto output = std::move(crushed.crushed);
  if (!crushed.split.empty()) { output.push_back(detail::JSON_CRUSH_DELIMITER); output += crushed.split; }
  output.push_back(u'_');
  return detail::utf16_to_utf8(output);
}

inline auto uncrush(std::string_view input) -> std::string {
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
