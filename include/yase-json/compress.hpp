#pragma once

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
    return detail::write_compressed(memory.values, root_key);
  }
};

[[nodiscard]] inline auto compress(std::string_view json_str) -> std::string {
  return Compressor{}.compress(json_str);
}

} // namespace yase_json
