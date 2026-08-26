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

TEST_CASE("Compressor matches compress-json format", "[compression][compatibility]") {
  yase_json::Compressor compressor;
  yase_json::Decompressor decompressor;

  auto constexpr sample_json = R"({
  "key_0": 83.65503238356673,
  "key_1": 89.95409841521338,
  "key_2": 47.44338696958149,
  "key_3": 94.43725005738578,
  "key_4": 0.36392421970924826,
  "key_5": 68.47659554361542,
  "key_6": 14.436310626403683,
  "key_7": 31.42182555594355,
  "key_8": 40.578482117782904,
  "key_9": 4.781149012078
})";

  auto constexpr expected_compressed = R"([["key_0","key_1","key_2","key_3","key_4","key_5","key_6","key_7","key_8","key_9","a|0|1|2|3|4|5|6|7|8|9","n|1L.Ah7S1YY0","n|1R.NelL46Sh","n|l.QkCCYIhk","n|1W.OrxaDYwo","n|0.:4doudMnW31","n|16.6xcghesU","n|E.1lh6nCSTS","n|V.FiSW6K6e","n|e.1sDjrK3D9","n|4.FJsCQPj","o|A|B|C|D|E|F|G|H|I|J|K"],"L"])";

  SECTION("compress uses the same encoded payload as compress-json") {
    REQUIRE(compressor.compress(sample_json) == expected_compressed);
  }

  SECTION("decompress accepts compress-json encoded payloads") {
    auto const decompressed = decompressor.decompress(expected_compressed);

    glz::generic original_parsed, decompressed_parsed;
    REQUIRE(glz::read_json(original_parsed, sample_json) == 0);
    REQUIRE(glz::read_json(decompressed_parsed, decompressed) == 0);

    auto original_out = std::string{};
    auto decompressed_out = std::string{};
    REQUIRE(glz::write_json(original_parsed, original_out) == 0);
    REQUIRE(glz::write_json(decompressed_parsed, decompressed_out) == 0);
    REQUIRE(original_out == decompressed_out);
  }
}

TEST_CASE("Strings with special prefix survive round-trip (Bug A regression)", "[compression][regression]") {
  yase_json::Compressor compressor;
  yase_json::Decompressor decompressor;

  auto test_string_value = [&](std::string_view payload) {
    // payload is a JSON string value that looks like internal encoding
    auto json = std::string{"\""} + std::string{payload} + "\"";
    auto compressed = compressor.compress(json);
    auto decompressed = decompressor.decompress(compressed);
    glz::generic orig, decomp;
    REQUIRE(glz::read_json(orig, json) == 0);
    REQUIRE(glz::read_json(decomp, decompressed) == 0);
    std::string o, d;
    REQUIRE(glz::write_json(orig, o) == 0);
    REQUIRE(glz::write_json(decomp, d) == 0);
    REQUIRE(o == d);
  };

  SECTION("N| prefix strings") {
    test_string_value("N|+");
    test_string_value("N|-");
    test_string_value("N|0");
    test_string_value("N|x");
    test_string_value("N|hello");
  }

  SECTION("other encoded-like prefixes") {
    test_string_value("a|foo");
    test_string_value("b|T");
    test_string_value("n|123");
    test_string_value("o|bar");
    test_string_value("s|baz");
  }

  SECTION("object containing N| string values") {
    auto json = R"({"k":"N|+","k2":"N|x"})";
    auto compressed = compressor.compress(json);
    auto decompressed = decompressor.decompress(compressed);
    glz::generic o, d;
    REQUIRE(glz::read_json(o, json) == 0);
    REQUIRE(glz::read_json(d, decompressed) == 0);
    std::string os, ds;
    REQUIRE(glz::write_json(o, os) == 0);
    REQUIRE(glz::write_json(d, ds) == 0);
    REQUIRE(os == ds);
  }

  SECTION("free function API round-trip") {
    auto json = R"({"x":"N|+"})";
    auto compressed = yase_json::compress(json);
    auto decompressed = yase_json::decompress(compressed);
    glz::generic o, d;
    REQUIRE(glz::read_json(o, json) == 0);
    REQUIRE(glz::read_json(d, decompressed) == 0);
    std::string os, ds;
    REQUIRE(glz::write_json(o, os) == 0);
    REQUIRE(glz::write_json(d, ds) == 0);
    REQUIRE(os == ds);
  }
}
