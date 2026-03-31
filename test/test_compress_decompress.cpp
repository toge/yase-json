#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"

TEST_CASE("Base62 encoding/decoding", "[base62]") {
  SECTION("Encode/Decode symmetry") {
    for (uint64_t i = 0; i < 1000; ++i) {
      auto encoded = yase_json::detail::to_base62(i);
      auto decoded = yase_json::detail::from_base62(encoded);
      REQUIRE(i == decoded);
    }
  }
}

TEST_CASE("Compressor and Decompressor symmetry", "[compression]") {
  yase_json::Compressor compressor;
  yase_json::Decompressor decompressor;

  auto test_symmetry = [&](std::string_view json_str) {
    auto compressed = compressor.compress(json_str);
    auto decompressed = decompressor.decompress(compressed);

    glz::generic original_parsed, decompressed_parsed;
    auto ec_read_orig = glz::read_json(original_parsed, json_str);
    REQUIRE(ec_read_orig == 0); // Assert success
    auto ec_read_decomp = glz::read_json(decompressed_parsed, decompressed);
    REQUIRE(ec_read_decomp == 0); // Assert success

    std::string original_out, decompressed_out;
    auto ec_write_orig = glz::write_json(original_parsed, original_out);
    REQUIRE(ec_write_orig == 0); // Assert success
    auto ec_write_decomp = glz::write_json(decompressed_parsed, decompressed_out);
    REQUIRE(ec_write_decomp == 0); // Assert success

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

    auto compressed = compressor.compress(redundant_str);

    glz::generic compressed_parsed;
    auto ec_read_compressed = glz::read_json(compressed_parsed, compressed);
    REQUIRE(ec_read_compressed == 0);

    auto decompressed = decompressor.decompress(compressed);

    glz::generic original_parsed, decompressed_parsed;
    auto ec_read_orig_redundant = glz::read_json(original_parsed, redundant_str);
    REQUIRE(ec_read_orig_redundant == 0);
    auto ec_read_decomp_redundant = glz::read_json(decompressed_parsed, decompressed);
    REQUIRE(ec_read_decomp_redundant == 0);
    std::string original_out, decompressed_out;
    if (auto const ec = glz::write_json(original_parsed, original_out)) {
      throw std::runtime_error("Failed to write original JSON");
    }
    if (auto const ec = glz::write_json(decompressed_parsed, decompressed_out)) {
      throw std::runtime_error("Failed to write decompressed JSON");
    }
    REQUIRE(original_out == decompressed_out);
  }
}
