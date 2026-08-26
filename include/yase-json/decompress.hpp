#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include "yase-json/detail/compress_json_compat.hpp"

namespace yase_json {

class Decompressor {
public:
  auto decompress(std::string_view compressed_json_str) -> std::string {
    auto compressed = glz::generic{};
    if (auto const ec = glz::read_json(compressed, compressed_json_str)) {
      throw std::runtime_error("Failed to parse compressed JSON: " + glz::format_error(ec, compressed_json_str));
    }

    if (!compressed.is_array()) {
      throw std::runtime_error("Root must be array");
    }

    auto const& root = compressed.get<glz::generic::array_t>();
    if (root.size() != 2) {
      throw std::runtime_error("Root must have exactly 2 elements");
    }
    if (!root[0].is_array()) {
      throw std::runtime_error("Values must be an array");
    }
    if (!root[1].is_string()) {
      throw std::runtime_error("Root key must be a string");
    }

    auto const& values = root[0].get<glz::generic::array_t>();
    auto const result = decode(root[1].get<std::string>(), values, 0);

    auto out = std::string{};
    if (auto const ec = glz::write_json(result, out)) {
      throw std::runtime_error("Failed to generate decompressed JSON");
    }
    return out;
  }

private:
  static constexpr size_t kMaxDepth = 512;

  template <typename T>
  static auto make_node(T&& v) -> glz::generic {
    auto node = glz::generic{};
    node = std::forward<T>(v);
    return node;
  }

  auto decode(std::string_view key, glz::generic::array_t const& values, size_t depth) const -> glz::generic {
    if (depth > kMaxDepth) {
      throw std::runtime_error("Decompression depth limit exceeded");
    }
    if (key.empty() || key == "_") {
      return make_node(nullptr);
    }

    auto const index = detail::from_base62(key);
    if (index >= values.size()) {
      throw std::out_of_range("Value key out of range");
    }

    auto const& encoded_node = values.at(index);
    if (!encoded_node.is_string()) {
      throw std::runtime_error("Encoded value must be a string");
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

  auto decode_array(std::string_view encoded, glz::generic::array_t const& values, size_t depth) const -> glz::generic {
    if (depth > kMaxDepth) {
      throw std::runtime_error("Decompression depth limit exceeded");
    }
    if (encoded == "a|") {
      return make_node(glz::generic::array_t{});
    }

    auto const parts = detail::split_preserving_empty(encoded);
    auto array = glz::generic::array_t{};
    array.reserve(parts.size() - 1);
    for (auto const index : std::views::iota(size_t{1}, parts.size())) {
      array.emplace_back(decode(parts[index], values, depth));
    }

    return make_node(std::move(array));
  }

  auto decode_bool(std::string_view encoded) const -> glz::generic {
    if (encoded == "b|T") {
      return make_node(true);
    }
    if (encoded == "b|F") {
      return make_node(false);
    }
    throw std::runtime_error("Unknown boolean encoding");
  }

  auto decode_number(std::string_view encoded) const -> glz::generic {
    return make_node(detail::s_to_num(encoded.substr(2)));
  }

  auto decode_special_number(std::string_view encoded) const -> glz::generic {
    switch (encoded.size() > 2 ? encoded[2] : '\0') {
      case '+':
        return make_node(std::numeric_limits<double>::infinity());
      case '-':
        return make_node(-std::numeric_limits<double>::infinity());
      case '0':
        return make_node(std::numeric_limits<double>::quiet_NaN());
      default:
        throw std::runtime_error("Unknown special number encoding");
    }
  }

  auto decode_object(std::string_view encoded, glz::generic::array_t const& values, size_t depth) const -> glz::generic {
    if (depth > kMaxDepth) {
      throw std::runtime_error("Decompression depth limit exceeded");
    }
    if (encoded == "o|") {
      return make_node(glz::generic::object_t{});
    }

    auto const parts = detail::split_preserving_empty(encoded);
    if (parts.size() < 2) {
      throw std::runtime_error("Object encoding is malformed");
    }

    auto const decoded_keys = decode(parts[1], values, depth);
    auto keys = std::vector<std::string>{};
    if (decoded_keys.is_array()) {
      auto const& key_array = decoded_keys.get<glz::generic::array_t>();
      keys.reserve(key_array.size());
      for (auto const& key : key_array) {
        if (!key.is_string()) {
          throw std::runtime_error("Decoded object key must be a string");
        }
        keys.emplace_back(key.get<std::string>());
      }
    } else if (decoded_keys.is_string()) {
      keys.emplace_back(decoded_keys.get<std::string>());
    } else {
      throw std::runtime_error("Decoded object key schema must be a string or string array");
    }

    if (parts.size() - 2 != keys.size()) {
      throw std::runtime_error("Object key/value count mismatch");
    }

    auto object = glz::generic::object_t{};
    for (auto const index : std::views::iota(size_t{0}, keys.size())) {
      object.emplace(keys[index], decode(parts[index + 2], values, depth));
    }

    return make_node(std::move(object));
  }
};

// 自由関数版 — クラスと等価だが状態を持たない
[[nodiscard]] inline auto decompress(std::string_view compressed_json_str) -> std::string {
  return Decompressor{}.decompress(compressed_json_str);
}

} // namespace yase_json
