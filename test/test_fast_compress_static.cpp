
#include <catch2/catch_all.hpp>
#include "yase-json/fast_compress.hpp"

TEST_CASE("StaticFastCompressor のフィールド選択", "[fast_compress]") {
  yase_json::StaticFastCompressor<"name", "age"> compressor;
  
  std::string json = R"({"name": "toge", "age": 25, "gender": "male"})";
  std::string compressed = compressor.compress(json);
  
  // The compressed output should only contain "name" and "age"
  
  REQUIRE(compressed.find("gender") == std::string::npos);
  REQUIRE(compressed.find("name") != std::string::npos);
  REQUIRE(compressed.find("age") != std::string::npos);
}
