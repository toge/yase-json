#ifndef __YASE_JSON_COMPRESS_JSON_COMPAT_HPP__
#define __YASE_JSON_COMPRESS_JSON_COMPAT_HPP__

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

namespace yase_json::detail {

struct string_hash {
  using is_transparent = void;
  auto operator()(std::string_view sv) const noexcept -> std::size_t {
    return std::hash<std::string_view>{}(sv);
  }
};

inline auto constexpr BASE62_CHARS = std::string_view{"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
inline auto constexpr MAX_SAFE_INTEGER = std::string_view{"9007199254740991"};

inline auto constexpr decode_table = [] {
  auto table = std::array<uint8_t, 256>{};
  table.fill(0);
  for (auto const i : std::views::iota(size_t{0}, BASE62_CHARS.size())) {
    table[static_cast<uint8_t>(BASE62_CHARS[i])] = static_cast<uint8_t>(i);
  }
  return table;
}();

inline auto strip_leading_zeroes(std::string_view digits) -> std::string_view {
  while (digits.size() > 1 && digits.front() == '0') {
    digits.remove_prefix(1);
  }
  return digits;
}

inline auto to_base62(uint64_t value) -> std::string {
  if (value == 0) {
    return "0";
  }

  auto encoded = std::string{};
  while (value > 0) {
    encoded.push_back(BASE62_CHARS[value % 62]);
    value /= 62;
  }
  std::ranges::reverse(encoded);
  return encoded;
}

inline auto from_base62(std::string_view encoded) -> uint64_t {
  auto value = uint64_t{0};
  for (auto const ch : encoded) {
    value = value * 62 + decode_table[static_cast<uint8_t>(ch)];
  }
  return value;
}

inline auto split_preserving_empty(std::string_view value) -> std::vector<std::string_view> {
  auto parts = std::vector<std::string_view>{};
  auto begin = size_t{0};
  while (begin <= value.size()) {
    auto const end = value.find('|', begin);
    if (end == std::string_view::npos) {
      parts.emplace_back(value.substr(begin));
      break;
    }
    parts.emplace_back(value.substr(begin, end - begin));
    begin = end + 1;
  }
  return parts;
}

inline auto is_safe_integer_string(std::string_view digits) -> bool {
  digits = strip_leading_zeroes(digits);
  if (digits.size() != MAX_SAFE_INTEGER.size()) {
    return digits.size() < MAX_SAFE_INTEGER.size();
  }
  return digits <= MAX_SAFE_INTEGER;
}

inline auto decimal_to_base62(std::string_view digits) -> std::string {
  digits = strip_leading_zeroes(digits);
  if (digits == "0") {
    return "0";
  }
  if (is_safe_integer_string(digits)) {
    return to_base62(std::stoull(std::string{digits}));
  }

  auto current = std::string{digits};
  auto encoded = std::string{};
  while (current != "0") {
    auto quotient = std::string{};
    auto remainder = uint32_t{0};
    for (auto const ch : current) {
      auto const value = remainder * 10 + static_cast<uint32_t>(ch - '0');
      auto const digit = value / 62;
      remainder = value % 62;
      if (!quotient.empty() || digit != 0) {
        quotient.push_back(static_cast<char>('0' + digit));
      }
    }
    if (quotient.empty()) {
      quotient = "0";
    }
    encoded.push_back(BASE62_CHARS[remainder]);
    current = std::move(quotient);
  }

  std::ranges::reverse(encoded);
  return encoded;
}

inline auto multiply_decimal(std::string& digits, uint32_t factor) -> void {
  auto carry = uint32_t{0};
  for (auto const index : std::views::reverse(std::views::iota(size_t{0}, digits.size()))) {
    auto const value = static_cast<uint32_t>(digits[index] - '0') * factor + carry;
    digits[index] = static_cast<char>('0' + (value % 10));
    carry = value / 10;
  }
  while (carry > 0) {
    digits.insert(digits.begin(), static_cast<char>('0' + (carry % 10)));
    carry /= 10;
  }
}

inline auto add_decimal(std::string& digits, uint32_t addend) -> void {
  auto carry = addend;
  for (auto const index : std::views::reverse(std::views::iota(size_t{0}, digits.size()))) {
    if (carry == 0) {
      break;
    }
    auto const value = static_cast<uint32_t>(digits[index] - '0') + carry;
    digits[index] = static_cast<char>('0' + (value % 10));
    carry = value / 10;
  }
  while (carry > 0) {
    digits.insert(digits.begin(), static_cast<char>('0' + (carry % 10)));
    carry /= 10;
  }
}

inline auto base62_to_decimal(std::string_view encoded) -> std::string {
  auto digits = std::string{"0"};
  for (auto const ch : encoded) {
    multiply_decimal(digits, 62);
    add_decimal(digits, decode_table[static_cast<uint8_t>(ch)]);
  }
  return std::string{strip_leading_zeroes(digits)};
}

inline auto int_str_to_s(std::string_view digits) -> std::string {
  digits = strip_leading_zeroes(digits);
  if (digits.empty()) {
    digits = "0";
  }

  auto const encoded = decimal_to_base62(digits);
  if (is_safe_integer_string(digits)) {
    return encoded;
  }
  return ":" + encoded;
}

inline auto s_to_int_str(std::string_view encoded) -> std::string {
  if (!encoded.empty() && encoded.front() == ':') {
    encoded.remove_prefix(1);
  }
  return base62_to_decimal(encoded);
}

inline auto encode_string(std::string_view value) -> std::string {
  if (value.size() > 1 && value[1] == '|') {
    switch (value.front()) {
      case 'a':
      case 'b':
      case 'n':
      case 'o':
      case 's':
        return "s|" + std::string{value};
      default:
        break;
    }
  }
  return std::string{value};
}

inline auto reverse_string(std::string_view value) -> std::string {
  auto reversed = std::string{value};
  std::ranges::reverse(reversed);
  return reversed;
}

inline auto number_to_json_string(double value) -> std::string {
  if (value == 0.0) {
    return "0";
  }

  auto node = glz::generic{};
  node = value;

  auto out = std::string{};
  if (auto const ec = glz::write_json(node, out)) {
    throw std::runtime_error("Failed to encode number");
  }
  if (out == "-0") {
    return "0";
  }
  return out;
}

inline auto num_to_s(double value) -> std::string {
  if (value < 0) {
    return "-" + num_to_s(-value);
  }

  auto number = number_to_json_string(value);
  auto dot = number.find('.');
  if (dot == std::string::npos) {
    auto const exp = number.find('e');
    if (exp == std::string::npos) {
      return int_str_to_s(number);
    }
    number.insert(exp, ".0");
    dot = exp;
  }

  auto a = std::string_view{number}.substr(0, dot);
  auto remainder = std::string_view{number}.substr(dot + 1);
  auto exp = remainder.find('e');
  auto b = remainder.substr(0, exp);
  auto c = exp == std::string_view::npos ? std::string_view{} : remainder.substr(exp + 1);

  auto encoded = int_str_to_s(a) + "." + int_str_to_s(reverse_string(b));
  if (!c.empty()) {
    encoded.push_back('.');
    if (c.front() == '-') {
      encoded.push_back('-');
      c.remove_prefix(1);
    } else if (c.front() == '+') {
      c.remove_prefix(1);
    }
    encoded += int_str_to_s(c);
  }
  return encoded;
}

inline auto s_to_num(std::string_view encoded) -> double {
  if (!encoded.empty() && encoded.front() == '-') {
    return -s_to_num(encoded.substr(1));
  }

  auto const first_dot = encoded.find('.');
  if (first_dot == std::string_view::npos) {
    return std::stod(s_to_int_str(encoded));
  }

  auto a = s_to_int_str(encoded.substr(0, first_dot));
  auto remainder = encoded.substr(first_dot + 1);
  auto const second_dot = remainder.find('.');
  auto b = reverse_string(s_to_int_str(remainder.substr(0, second_dot)));

  auto number = a + "." + b;
  if (second_dot != std::string_view::npos) {
    auto c = remainder.substr(second_dot + 1);
    number.push_back('e');
    if (!c.empty() && c.front() == '-') {
      number.push_back('-');
      c.remove_prefix(1);
    }
    number += s_to_int_str(c);
  }
  return std::stod(number);
}

inline auto encode_number(double value) -> std::string {
  if (value == std::numeric_limits<double>::infinity()) {
    return "N|+";
  }
  if (value == -std::numeric_limits<double>::infinity()) {
    return "N|-";
  }
  if (std::isnan(value)) {
    return "N|0";
  }
  return "n|" + num_to_s(value);
}

struct CompressionMemory {
  std::vector<std::string> values{};
  std::unordered_map<std::string, std::string,
                     string_hash, std::equal_to<>> value_cache{};
  std::unordered_map<std::string, std::string,
                     string_hash, std::equal_to<>> schema_cache{};

  auto get_value_key(std::string value) -> std::string {
    if (auto const it = value_cache.find(std::string_view{value}); it != value_cache.end()) {
      return it->second;
    }

    auto const key = to_base62(values.size());
    values.emplace_back(std::move(value));
    value_cache.emplace(values.back(), key);
    return key;
  }

  auto get_schema(std::vector<std::string> const& keys) -> std::string {
    auto signature = std::string{};
    for (auto const index : std::views::iota(size_t{0}, keys.size())) {
      if (index != 0) {
        signature.push_back(',');
      }
      signature += keys[index];
    }

    if (auto const it = schema_cache.find(std::string_view{signature}); it != schema_cache.end()) {
      return it->second;
    }

    auto encoded = std::string{"a"};
    for (auto const& key : keys) {
      encoded.push_back('|');
      encoded += get_value_key(encode_string(key));
    }
    if (encoded == "a") {
      encoded = "a|";
    }

    auto const schema_key = get_value_key(std::move(encoded));
    schema_cache.emplace(std::move(signature), schema_key);
    return schema_key;
  }

  auto add_value(glz::generic const& value, bool array_element = false) -> std::string {
    if (value.is_null()) {
      return array_element ? "_" : "";
    }

    if (value.is_object()) {
      auto const& object = value.get<glz::generic::object_t>();
      if (object.empty()) {
        return get_value_key("o|");
      }

      auto keys = std::vector<std::string>{};
      keys.reserve(object.size());
      for (auto const& [key, ignored] : object) {
        std::ignore = ignored;
        keys.emplace_back(key);
      }

      auto encoded = std::string{"o|"};
      encoded += get_schema(keys);
      for (auto const& [key, child] : object) {
        std::ignore = key;
        encoded.push_back('|');
        encoded += add_value(child);
      }
      return get_value_key(std::move(encoded));
    }

    if (value.is_array()) {
      auto encoded = std::string{"a"};
      for (auto const& child : value.get<glz::generic::array_t>()) {
        encoded.push_back('|');
        encoded += child.is_null() ? "_" : add_value(child, true);
      }
      if (encoded == "a") {
        encoded = "a|";
      }
      return get_value_key(std::move(encoded));
    }

    if (value.is_boolean()) {
      return get_value_key(value.get<bool>() ? "b|T" : "b|F");
    }

    if (value.is_number()) {
      return get_value_key(encode_number(value.get<double>()));
    }

    if (value.is_string()) {
      return get_value_key(encode_string(value.get<std::string>()));
    }

    throw std::runtime_error("Unsupported JSON value");
  }
};

} // namespace yase_json::detail

#endif // __YASE_JSON_COMPRESS_JSON_COMPAT_HPP__
