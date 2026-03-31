#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"

TEST_CASE("Base62 encoding/decoding", "[base62]") {
  SECTION("Encode/Decode symmetry") {
    for (uint64_t i = 0; i < 1000; ++i) {
      auto encoded = yase_json::to_base62(i);
      auto decoded = yase_json::detail::from_base62(encoded);
      REQUIRE(i == decoded);
    }
  }
}

TEST_CASE("Compressor and Decompressor symmetry", "[compression]") {
  yase_json::Compressor compressor;
  yase_json::Decompressor decompressor;

  auto test_symmetry = [&](std::string_view json_str) {
    glz::generic original;
    glz::read_json(original, json_str);

    auto compressed = compressor.compress(original);
    auto decompressed = decompressor.decompress(compressed);

    std::string original_out, decompressed_out;
    glz::write_json(original, original_out);
    glz::write_json(decompressed, decompressed_out);

    REQUIRE(original_out == decompressed_out);
  };

  SECTION("Simple string") {
    test_symmetry(R"("hello world")");
  }

  SECTION("Simple object") {
    test_symmetry(R"({"key1":"value1","key2":123.0})");
  }

  SECTION("Simple array") {
    test_symmetry(R"(["a","b","c",1.0,2.0])");
  }

  SECTION("Nested structure") {
    test_symmetry(R"({
      "obj": {"inner": "val"},
      "arr": [1.0, 2.0, {"x": "y"}],
      "str": "redundant",
      "str2": "redundant"
    })");
  }

  SECTION("Redundant data compression") {
    std::string redundant_str = R"([
      {"name": "item", "val": 1.0},
      {"name": "item", "val": 1.0},
      {"name": "item", "val": 1.0}
    ])";

    glz::generic original;
    glz::read_json(original, redundant_str);
    auto compressed = compressor.compress(original);

    auto const& arr = compressed.get<glz::generic::array_t>();
    REQUIRE(arr.size() == 2);

    auto decompressed = decompressor.decompress(compressed);
    std::string original_out, decompressed_out;
    glz::write_json(original, original_out);
    glz::write_json(decompressed, decompressed_out);
    REQUIRE(original_out == decompressed_out);
  }
}
