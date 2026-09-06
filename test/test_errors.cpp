#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"

TEST_CASE("Decompressor rejects malformed inputs", "[error][decompress]") {
  SECTION("invalid JSON") {
    auto result = yase_json::try_decompress("not json");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message.find("Failed to parse") != std::string::npos);
  }

  SECTION("root not array") {
    auto result = yase_json::try_decompress(R"({"a":1})");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "Root must be array");
  }

  SECTION("root array wrong size") {
    REQUIRE_FALSE(yase_json::try_decompress(R"([[]])"));
    REQUIRE_FALSE(yase_json::try_decompress(R"([[],"a","extra"])"));
  }

  SECTION("values not array") {
    auto result = yase_json::try_decompress(R"(["not_array","0"])");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "Values must be an array");
  }

  SECTION("root key not string") {
    auto result = yase_json::try_decompress(R"([[],123])");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message == "Root key must be a string");
  }

  SECTION("out-of-range key") {
    auto result = yase_json::try_decompress(R"([["a|"],"1"])");
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message.find("out of range") != std::string::npos);
  }

  SECTION("unknown boolean encoding") {
    auto compressed = std::string(R"([["b|X"],"0"])");
    auto result = yase_json::try_decompress(compressed);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message.find("Unknown boolean") != std::string::npos);
  }

  SECTION("unknown special number encoding") {
    auto compressed = std::string(R"([["N|x"],"0"])");
    auto result = yase_json::try_decompress(compressed);
    REQUIRE_FALSE(result);
    REQUIRE(result.error().message.find("Unknown special number") != std::string::npos);
  }

}

TEST_CASE("Compressor rejects invalid input", "[error][compress]") {
  SECTION("invalid JSON returns error") {
    auto result1 = yase_json::try_compress("not json");
    REQUIRE_FALSE(result1);
    REQUIRE(result1.error().message.find("Failed to parse") != std::string::npos);

    auto result2 = yase_json::try_compress("{bad}");
    REQUIRE_FALSE(result2);
    REQUIRE(result2.error().message.find("Failed to parse") != std::string::npos);
  }
}

TEST_CASE("crush/uncrush edge cases", "[error][crush]") {
  SECTION("empty string crush round-trip") {
    auto crushed = yase_json::try_crush("");
    REQUIRE(crushed);
    auto uncrushed = yase_json::try_uncrush(*crushed);
    REQUIRE(uncrushed);
    REQUIRE(*uncrushed == "");
  }

  SECTION("U+0001 is removed by crush (documented behavior)") {
    std::string with_delim = std::string("a") + char(0x01) + "b";
    auto crushed = yase_json::try_crush(with_delim);
    REQUIRE(crushed);
    auto uncrushed = yase_json::try_uncrush(*crushed);
    REQUIRE(uncrushed);
    REQUIRE(*uncrushed == "ab");
  }
}
