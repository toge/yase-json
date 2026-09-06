#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include <glaze/glaze.hpp>

#include "yase-json/detail/compress_json_compat.hpp"
#include "yase-json/detail/error.hpp"

namespace yase_json {

inline auto try_compress(std::string_view json_str) -> detail::result<std::string> {
  auto data = glz::generic{};
  if (auto const ec = glz::read_json(data, json_str)) {
    return detail::err("Failed to parse JSON: " + glz::format_error(ec, json_str));
  }

  auto memory = detail::CompressionMemory{};
  auto root_key = memory.add_value(data);
  if (!root_key) {
    return std::unexpected(std::move(root_key).error());
  }
  return detail::write_compressed(memory.values, *root_key);
}

} // namespace yase_json
