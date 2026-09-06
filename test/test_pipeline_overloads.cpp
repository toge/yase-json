#include <catch2/catch_all.hpp>
#include <string>

#include "yase-json/pipeline.hpp"

TEST_CASE("pipeline::try_compress_and_crush basic", "[pipeline_overloads]") {
  std::string const input = R"({"name":"test","data":[1,2,3,1,2,3]})";

  SECTION("compress_and_crush returns expected result") {
    auto result = yase_json::pipeline::try_compress_and_crush(input);
    REQUIRE(result);
    REQUIRE(!result->empty());
    // JSONCrush output ends with '_'
    REQUIRE(result->back() == '_');
  }

  SECTION("round-trip is symmetric") {
    auto compressed = yase_json::pipeline::try_compress_and_crush(input);
    REQUIRE(compressed);
    auto decompressed = yase_json::pipeline::try_uncrush_and_decompress(*compressed);
    REQUIRE(decompressed);
    // Content should match (though formatting may differ)
    auto recompressed = yase_json::pipeline::try_compress_and_crush(*decompressed);
    REQUIRE(recompressed);
    REQUIRE(*recompressed == *compressed);
  }
}

TEST_CASE("pipeline::try_uncrush_and_decompress basic", "[pipeline_overloads]") {
  std::string const input = R"({"name":"test","data":[1,2,3,1,2,3]})";
  auto const compressed = yase_json::pipeline::try_compress_and_crush(input);
  REQUIRE(compressed);

  SECTION("uncrush_and_decompress returns expected result") {
    auto result = yase_json::pipeline::try_uncrush_and_decompress(*compressed);
    REQUIRE(result);
    REQUIRE(!result->empty());
  }
}
