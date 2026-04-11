#ifndef YASE_JSON_PIPELINE_HPP
#define YASE_JSON_PIPELINE_HPP

#include <string>
#include <string_view>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"

namespace yase_json::pipeline {

/**
 * @brief Performs structural compression followed by substitution compression (JSONCrush).
 * @param input The raw JSON string view.
 * @return The compressed and crushed string.
 */
[[nodiscard]] inline auto compress_and_crush(std::string_view input) -> std::string {
  return crush(Compressor{}.compress(input));
}

/**
 * @brief Reverses the substitution compression (JSONCrush) followed by structural decompression.
 * @param input The crushed and compressed string view.
 * @return The original JSON string.
 */
[[nodiscard]] inline auto uncrush_and_decompress(std::string_view input) -> std::string {
  return Decompressor{}.decompress(uncrush(input));
}

} // namespace yase_json::pipeline

#endif // YASE_JSON_PIPELINE_HPP
