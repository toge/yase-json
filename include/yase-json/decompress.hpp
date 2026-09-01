#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/glaze.hpp>

#include "yase-json/detail/compress_json_compat.hpp"
#include "yase-json/detail/error.hpp"

namespace yase_json::detail {

template <typename T>
auto make_node(T&& v) -> glz::generic {
  auto node = glz::generic{};
  node = std::forward<T>(v);
  return node;
}

inline auto decode(std::string_view key, glz::generic::array_t const& values, size_t depth) -> result<glz::generic>;

inline auto decode_array(std::string_view encoded, glz::generic::array_t const& values, size_t depth)
  -> result<glz::generic> {
  if (encoded == "a|") {
    return make_node(glz::generic::array_t{});
  }

  auto const parts = split_preserving_empty(encoded);
  auto array = glz::generic::array_t{};
  array.reserve(parts.size() - 1);
  for (auto const index : std::views::iota(size_t{1}, parts.size())) {
    auto node = decode(parts[index], values, depth);
    if (!node) {
      return node;
    }
    array.emplace_back(std::move(*node));
  }

  return make_node(std::move(array));
}

inline auto decode_bool(std::string_view encoded) -> result<glz::generic> {
  if (encoded == "b|T") {
    return make_node(true);
  }
  if (encoded == "b|F") {
    return make_node(false);
  }
  return err("Unknown boolean encoding");
}

inline auto decode_number(std::string_view encoded) -> result<glz::generic> {
  return s_to_num(encoded.substr(2)).transform([](double const v) { return make_node(v); });
}

inline auto decode_special_number(std::string_view encoded) -> result<glz::generic> {
  switch (encoded.size() > 2 ? encoded[2] : '\0') {
    case '+':
      return make_node(std::numeric_limits<double>::infinity());
    case '-':
      return make_node(-std::numeric_limits<double>::infinity());
    case '0':
      return make_node(std::numeric_limits<double>::quiet_NaN());
    default:
      return err("Unknown special number encoding");
  }
}

inline auto decode_object(std::string_view encoded, glz::generic::array_t const& values, size_t depth)
  -> result<glz::generic> {
  if (encoded == "o|") {
    return make_node(glz::generic::object_t{});
  }

  auto const parts = split_preserving_empty(encoded);
  if (parts.size() < 2) {
    return err("Object encoding is malformed");
  }

  auto decoded_keys = decode(parts[1], values, depth);
  if (!decoded_keys) {
    return decoded_keys;
  }
  auto keys = std::vector<std::string>{};
  if (decoded_keys->is_array()) {
    auto const& key_array = decoded_keys->get<glz::generic::array_t>();
    keys.reserve(key_array.size());
    for (auto const& key : key_array) {
      if (!key.is_string()) {
        return err("Decoded object key must be a string");
      }
      keys.emplace_back(key.get<std::string>());
    }
  } else if (decoded_keys->is_string()) {
    keys.emplace_back(decoded_keys->get<std::string>());
  } else {
    return err("Decoded object key schema must be a string or string array");
  }

  if (parts.size() - 2 != keys.size()) {
    return err("Object key/value count mismatch");
  }

  auto object = glz::generic::object_t{};
  for (auto const index : std::views::iota(size_t{0}, keys.size())) {
    auto node = decode(parts[index + 2], values, depth);
    if (!node) {
      return node;
    }
    object.emplace(keys[index], std::move(*node));
  }

  return make_node(std::move(object));
}

inline auto decode(std::string_view key, glz::generic::array_t const& values, size_t depth) -> result<glz::generic> {
  if (depth > kMaxDepth) {
    return err("Decompression depth limit exceeded");
  }
  if (key.empty() || key == "_") {
    return make_node(nullptr);
  }

  auto index = from_base62(key);
  if (!index) {
    return std::unexpected(std::move(index).error());
  }
  if (*index >= values.size()) {
    return err("Value key out of range", error::kind_t::out_of_range);
  }

  auto const& encoded_node = values[*index];
  if (!encoded_node.is_string()) {
    return err("Encoded value must be a string");
  }
  auto const& encoded = encoded_node.get<std::string>();

  if (encoded.size() > 1 && encoded[1] == '|') {
    switch (encoded[0]) {
      case 'a':
        return decode_array(encoded, values, depth + 1);
      case 'b':
        return decode_bool(encoded);
      case 'n':
        return decode_number(encoded);
      case 'N':
        return decode_special_number(encoded);
      case 'o':
        return decode_object(encoded, values, depth + 1);
      case 's':
        return make_node(encoded.substr(2));
      default:
        break;
    }
  }

  return make_node(encoded);
}

} // namespace yase_json::detail

namespace yase_json {

inline auto try_decompress(std::string_view compressed_json_str) -> detail::result<std::string> {
  auto compressed = glz::generic{};
  if (auto const ec = glz::read_json(compressed, compressed_json_str)) {
    return detail::err("Failed to parse compressed JSON: " + glz::format_error(ec, compressed_json_str));
  }

  if (!compressed.is_array()) {
    return detail::err("Root must be array");
  }

  auto const& root = compressed.get<glz::generic::array_t>();
  if (root.size() != 2) {
    return detail::err("Root must have exactly 2 elements");
  }
  if (!root[0].is_array()) {
    return detail::err("Values must be an array");
  }
  if (!root[1].is_string()) {
    return detail::err("Root key must be a string");
  }

  auto const& values = root[0].get<glz::generic::array_t>();
  auto result = detail::decode(root[1].get<std::string>(), values, 0);
  if (!result) {
    return std::unexpected(std::move(result).error());
  }

  auto out = std::string{};
  if (auto const ec = glz::write_json(*result, out)) {
    return detail::err("Failed to generate decompressed JSON");
  }
  return out;
}

#if __cpp_exceptions

class Decompressor {
public:
  auto decompress(std::string_view compressed_json_str) -> std::string {
    auto result = try_decompress(compressed_json_str);
    if (!result) {
      detail::throw_error(result.error());
    }
    return std::move(*result);
  }
};

// 自由関数版 — クラスと等価だが状態を持たない
[[nodiscard]] inline auto decompress(std::string_view compressed_json_str) -> std::string {
  return Decompressor{}.decompress(compressed_json_str);
}

#endif

} // namespace yase_json
