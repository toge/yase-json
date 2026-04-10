#ifndef __YASE_JSON_COMPRESS_HPP__
#define __YASE_JSON_COMPRESS_HPP__

#include <stdexcept>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>

#include "yase-json/detail/compress_json_compat.hpp"

namespace yase_json {

class Compressor {
public:
  auto compress(std::string_view json_str) -> std::string {
    auto data = glz::generic{};
    if (auto const ec = glz::read_json(data, json_str)) {
      throw std::runtime_error("Failed to parse JSON: " + glz::format_error(ec, json_str));
    }

    auto memory = detail::CompressionMemory{};
    auto const root_key = memory.add_value(data);

    auto values = glz::generic::array_t{};
    values.reserve(memory.values.size());
    for (auto const& value : memory.values) {
      auto node = glz::generic{};
      node = value;
      values.emplace_back(std::move(node));
    }

    auto result = glz::generic::array_t{};
    auto values_node = glz::generic{};
    values_node = std::move(values);
    result.emplace_back(std::move(values_node));

    auto root_node = glz::generic{};
    root_node = root_key;
    result.emplace_back(std::move(root_node));

    auto final_node = glz::generic{};
    final_node = std::move(result);

    auto out = std::string{};
    if (auto const ec = glz::write_json(final_node, out)) {
      throw std::runtime_error("Failed to generate compressed JSON");
    }
    return out;
  }
};

} // namespace yase_json

#endif // __YASE_JSON_COMPRESS_HPP__
