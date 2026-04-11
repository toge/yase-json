#include <catch2/catch_test_macros.hpp>
#include "yase-json/pipeline.hpp"
#include <glaze/glaze.hpp>

TEST_CASE("Pipeline round-trip", "[pipeline]") {
  std::string const input = R"({
    "name": "Gemini CLI",
    "version": "1.0.0",
    "features": ["compression", "decompression", "pipeline"],
    "nested": {
      "key": "value",
      "number": 123.456,
      "boolean": true,
      "null": null
    }
  })";

  SECTION("Full compression and decompression") {
    auto const compressed = yase_json::pipeline::compress_and_crush(input);
    REQUIRE(!compressed.empty());
    
    // Check if it's actually crushed (JSONCrush output usually ends with '_')
    REQUIRE(compressed.back() == '_');

    auto const decompressed = yase_json::pipeline::uncrush_and_decompress(compressed);
    
    // Use glaze to compare JSON structures instead of raw string comparison (due to spacing)
    auto input_obj = glz::generic{};
    auto decompressed_obj = glz::generic{};
    
    REQUIRE(glz::read_json(input_obj, input) == glz::error_code::none);
    REQUIRE(glz::read_json(decompressed_obj, decompressed) == glz::error_code::none);
    
    std::string input_min, decompressed_min;
    REQUIRE(glz::write_json(input_obj, input_min) == glz::error_code::none);
    REQUIRE(glz::write_json(decompressed_obj, decompressed_min) == glz::error_code::none);
    
    REQUIRE(input_min == decompressed_min);
  }
}
