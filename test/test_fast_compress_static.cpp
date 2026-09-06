
#include <catch2/catch_all.hpp>
#include "yase-json/fast_compress.hpp"

TEST_CASE("StaticFastCompressor のフィールド選択", "[fast_compress]") {
  yase_json::StaticFastCompressor<"name", "age"> compressor;

  std::string json = R"({"name": "toge", "age": 25, "gender": "male"})";
  auto compressed_result = compressor.try_compress(json);
  REQUIRE(compressed_result);
  auto const& compressed = *compressed_result;

  // The compressed output should only contain "name" and "age"

  REQUIRE(compressed.find("gender") == std::string::npos);
  REQUIRE(compressed.find("name") != std::string::npos);
  REQUIRE(compressed.find("age") != std::string::npos);
}
