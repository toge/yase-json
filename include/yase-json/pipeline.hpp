#pragma once

#include <expected>
#include <iterator>
#include <string>
#include <string_view>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/detail/error.hpp"

namespace yase_json::pipeline {

/**
 * @brief Performs structural compression followed by substitution compression (JSONCrush).
 * @param input The raw JSON string view.
 * @return The compressed and crushed string, or error.
 */
[[nodiscard]] inline auto try_compress_and_crush(std::string_view input) -> detail::result<std::string> {
  auto const compressed_result = try_compress(input);
  if (!compressed_result) {
    return std::unexpected(std::move(compressed_result).error());
  }
  return try_crush(*compressed_result);
}

/**
 * @brief Reverses the substitution compression (JSONCrush) followed by structural decompression.
 * @param input The crushed and compressed string view.
 * @return The original JSON string, or error.
 */
[[nodiscard]] inline auto try_uncrush_and_decompress(std::string_view input) -> detail::result<std::string> {
  auto const uncrushed_result = try_uncrush(input);
  if (!uncrushed_result) {
    return std::unexpected(std::move(uncrushed_result).error());
  }
  return try_decompress(*uncrushed_result);
}

} // namespace yase_json::pipeline
