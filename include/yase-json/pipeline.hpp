#ifndef YASE_JSON_PIPELINE_HPP
#define YASE_JSON_PIPELINE_HPP

#include <iterator>
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
 * @brief Performs structural compression followed by substitution compression (JSONCrush).
 * @param input The raw JSON string view.
 * @param out The output string to append to.
 * @return The number of characters written.
 */
inline auto compress_and_crush(std::string_view input, std::string& out) -> std::size_t {
  return crush(Compressor{}.compress(input), out);
}

/**
 * @brief Performs structural compression followed by substitution compression (JSONCrush).
 * @param input The raw JSON string view.
 * @param out The output iterator to write to.
 * @return The output iterator after writing.
 */
template <std::output_iterator<char> OutputIt>
inline auto compress_and_crush(std::string_view input, OutputIt out) -> OutputIt {
  return crush(Compressor{}.compress(input), out);
}

/**
 * @brief Reverses the substitution compression (JSONCrush) followed by structural decompression.
 * @param input The crushed and compressed string view.
 * @return The original JSON string.
 */
[[nodiscard]] inline auto uncrush_and_decompress(std::string_view input) -> std::string {
  return Decompressor{}.decompress(uncrush(input));
}

/**
 * @brief Reverses the substitution compression (JSONCrush) followed by structural decompression.
 * @param input The crushed and compressed string view.
 * @param out The output string to append to.
 * @return The number of characters written.
 */
inline auto uncrush_and_decompress(std::string_view input, std::string& out) -> std::size_t {
  auto const result = uncrush_and_decompress(input);
  out.append(result);
  return result.size();
}

/**
 * @brief Reverses the substitution compression (JSONCrush) followed by structural decompression.
 * @param input The crushed and compressed string view.
 * @param out The output iterator to write to.
 * @return The output iterator after writing.
 */
template <std::output_iterator<char> OutputIt>
inline auto uncrush_and_decompress(std::string_view input, OutputIt out) -> OutputIt {
  auto const result = uncrush_and_decompress(input);
  return std::copy(result.begin(), result.end(), out);
}

} // namespace yase_json::pipeline

#endif // YASE_JSON_PIPELINE_HPP
