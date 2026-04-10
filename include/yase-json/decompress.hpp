#ifndef __YASE_JSON_DECOMPRESS_HPP__
#define __YASE_JSON_DECOMPRESS_HPP__

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

    values_ = &root[0].get<glz::generic::array_t>();
    auto const result = decode(root[1].get<std::string>());

    auto out = std::string{};
    if (auto const ec = glz::write_json(result, out)) {
      throw std::runtime_error("Failed to generate decompressed JSON");
    }
    return out;
  }

private:
  glz::generic::array_t const* values_ = nullptr;

  auto decode(std::string_view key) const -> glz::generic {
    if (key.empty() || key == "_") {
      auto node = glz::generic{};
      node = nullptr;
      return node;
    }

    auto const index = detail::from_base62(key);
    if (index >= values_->size()) {
      throw std::out_of_range("Value key out of range");
    }

    auto const& encoded_node = values_->at(index);
    if (!encoded_node.is_string()) {
      throw std::runtime_error("Encoded value must be a string");
    }
    auto const& encoded = encoded_node.get<std::string>();

    if (encoded.size() > 1 && encoded[1] == '|') {
      switch (encoded[0]) {
        case 'a':
          return decode_array(encoded);
        case 'b':
          return decode_bool(encoded);
        case 'n':
          return decode_number(encoded);
        case 'N':
          return decode_special_number(encoded);
        case 'o':
          return decode_object(encoded);
        case 's': {
          auto node = glz::generic{};
          node = encoded.substr(2);
          return node;
        }
        default:
          break;
      }
    }

    auto node = glz::generic{};
    node = encoded;
    return node;
  }

  auto decode_array(std::string_view encoded) const -> glz::generic {
    auto array = glz::generic::array_t{};
    if (encoded == "a|") {
      auto node = glz::generic{};
      node = std::move(array);
      return node;
    }

    auto const parts = detail::split_preserving_empty(encoded);
    array.reserve(parts.size() - 1);
    for (auto const index : std::views::iota(size_t{1}, parts.size())) {
      array.emplace_back(decode(parts[index]));
    }

    auto node = glz::generic{};
    node = std::move(array);
    return node;
  }

  auto decode_bool(std::string_view encoded) const -> glz::generic {
    if (encoded == "b|T") {
      auto node = glz::generic{};
      node = true;
      return node;
    }
    if (encoded == "b|F") {
      auto node = glz::generic{};
      node = false;
      return node;
    }
    throw std::runtime_error("Unknown boolean encoding");
  }

  auto decode_number(std::string_view encoded) const -> glz::generic {
    auto node = glz::generic{};
    node = detail::s_to_num(encoded.substr(2));
    return node;
  }

  auto decode_special_number(std::string_view encoded) const -> glz::generic {
    auto node = glz::generic{};
    switch (encoded.size() > 2 ? encoded[2] : '\0') {
      case '+':
        node = std::numeric_limits<double>::infinity();
        return node;
      case '-':
        node = -std::numeric_limits<double>::infinity();
        return node;
      case '0':
        node = std::numeric_limits<double>::quiet_NaN();
        return node;
      default:
        throw std::runtime_error("Unknown special number encoding");
    }
  }

  auto decode_object(std::string_view encoded) const -> glz::generic {
    auto object = glz::generic::object_t{};
    if (encoded == "o|") {
      auto node = glz::generic{};
      node = std::move(object);
      return node;
    }

    auto const parts = detail::split_preserving_empty(encoded);
    if (parts.size() < 2) {
      throw std::runtime_error("Object encoding is malformed");
    }

    auto const decoded_keys = decode(parts[1]);
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

    for (auto const index : std::views::iota(size_t{0}, keys.size())) {
      object.emplace(keys[index], decode(parts[index + 2]));
    }

    auto node = glz::generic{};
    node = std::move(object);
    return node;
  }
};

} // namespace yase_json

#endif // __YASE_JSON_DECOMPRESS_HPP__
