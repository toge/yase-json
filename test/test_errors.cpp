#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"

TEST_CASE("Decompressor rejects malformed inputs", "[error][decompress]") {
  yase_json::Decompressor decompressor;

  SECTION("invalid JSON") {
    REQUIRE_THROWS_AS(decompressor.decompress("not json"), std::runtime_error);
  }

  SECTION("root not array") {
    REQUIRE_THROWS_AS(decompressor.decompress(R"({"a":1})"), std::runtime_error);
  }

  SECTION("root array wrong size") {
    REQUIRE_THROWS_AS(decompressor.decompress(R"([[]])"), std::runtime_error);
    REQUIRE_THROWS_AS(decompressor.decompress(R"([[],"a","extra"])"), std::runtime_error);
  }

  SECTION("values not array") {
    REQUIRE_THROWS_AS(decompressor.decompress(R"(["not_array","0"])"), std::runtime_error);
  }

  SECTION("root key not string") {
    REQUIRE_THROWS_AS(decompressor.decompress(R"([[],123])"), std::runtime_error);
  }

  SECTION("out-of-range key") {
    // values has 1 entry at index 0, root key "1" is out of range
    REQUIRE_THROWS_AS(decompressor.decompress(R"([["a|"],"1"])"), std::out_of_range);
  }

  SECTION("unknown boolean encoding") {
    // craft compressed payload where one value is "b|X"
    auto const json = R"({"flag":true})";
    yase_json::Compressor c;
    auto compressed = c.compress(json);
    // replace "b|T" entry with "b|X" in values array
    // compressed is like [["b|T",...],"0"] — mutate
    compressed = std::string(R"([["b|X"],"0"])");
    REQUIRE_THROWS_AS(decompressor.decompress(compressed), std::runtime_error);
  }

  SECTION("unknown special number encoding") {
    auto compressed = std::string(R"([["N|x"],"0"])");
    REQUIRE_THROWS_AS(decompressor.decompress(compressed), std::runtime_error);
  }

  SECTION("depth limit exceeded") {
    // deeply nested array a|a|... chain exceeding 512
    // Build a payload manually with nested a| depth > 600
    std::string inner = "a|";
    for (int i = 0; i < 600; ++i) {
      std::string encoded = "a|" + std::string("0"); // index 0 self-reference pattern
      // Instead build actual nested structure via compressor with deep JSON
      // Use real deep JSON input
      break;
    }
    // Deep JSON via actual compression — compression itself should throw
    std::string deep_json = "";
    for (int i = 0; i < 520; ++i) deep_json += "[";
    deep_json += "1";
    for (int i = 0; i < 520; ++i) deep_json += "]";
    yase_json::Compressor comp;
    REQUIRE_THROWS_AS(comp.compress(deep_json), std::runtime_error);
  }
}

TEST_CASE("Compressor rejects invalid input", "[error][compress]") {
  yase_json::Compressor c;
  SECTION("invalid JSON throws") {
    REQUIRE_THROWS_AS(c.compress("not json"), std::runtime_error);
    REQUIRE_THROWS_AS(c.compress("{bad}"), std::runtime_error);
  }
}

TEST_CASE("crush/uncrush edge cases", "[error][crush]") {
  SECTION("empty string crush round-trip") {
    auto const crushed = yase_json::crush("");
    auto const uncrushed = yase_json::uncrush(crushed);
    REQUIRE(uncrushed == "");
  }

  SECTION("U+0001 is removed by crush (documented behavior)") {
    std::string with_delim = std::string("a") + char(0x01) + "b";
    auto const crushed = yase_json::crush(with_delim);
    auto const uncrushed = yase_json::uncrush(crushed);
    // delimiter is stripped, so round-trip loses it
    REQUIRE(uncrushed == "ab");
  }
}
