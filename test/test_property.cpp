#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/fast_compress.hpp"
#include "yase-json/pipeline.hpp"

namespace {

auto json_equal(std::string_view a, std::string_view b) -> bool {
  glz::generic pa, pb;
  if (glz::read_json(pa, a) != 0) return false;
  if (glz::read_json(pb, b) != 0) return false;
  std::string oa, ob;
  if (glz::write_json(pa, oa) != 0) return false;
  if (glz::write_json(pb, ob) != 0) return false;
  return oa == ob;
}

} // namespace

TEST_CASE("Property: random JSON round-trips via compress/decompress", "[property][compression]") {
  // Deterministic pseudo-random strings covering edge characters
  auto const samples = std::vector<std::string>{
      R"("N|+")",
      R"("N|x")",
      R"("a|foo")",
      R"("s|bar")",
      R"({"k":"N|0","k2":"a|b"})",
      R"({"x":null,"y":[null,1,null]})",
      R"({"empty_obj":{},"empty_arr":[]})",
      R"({"num":1.5,"val":12345})",
      R"({"s":"hello \"world\""})",
      R"([1,2,3,"N|+"])",
  };

  for (auto const& sample : samples) {
    auto compressed_result = yase_json::try_compress(sample);
    REQUIRE(compressed_result);
    auto decompressed_result = yase_json::try_decompress(*compressed_result);
    REQUIRE(decompressed_result);
    INFO("sample: " << sample);
    REQUIRE(json_equal(sample, *decompressed_result));
  }

  SECTION("pipeline round-trip") {
    for (auto const& sample : samples) {
      auto crushed_result = yase_json::pipeline::try_compress_and_crush(sample);
      REQUIRE(crushed_result);
      auto restored_result = yase_json::pipeline::try_uncrush_and_decompress(*crushed_result);
      REQUIRE(restored_result);
      INFO("sample: " << sample);
      REQUIRE(json_equal(sample, *restored_result));
    }
  }
}

TEST_CASE("Property: crush/uncrush is identity", "[property][crush]") {
  auto const samples = std::vector<std::string>{
      R"({"a":"hello","b":"hello","c":"hello"})",
      R"({"x":123.456,"y":123.456})",
      std::string(200, 'a'),
      R"({"k":"N|+","k2":"s|foo"})",
      "",
      "simple ascii string with repetition repetition repetition",
  };

  for (auto const& s : samples) {
    auto crushed_result = yase_json::try_crush(s);
    REQUIRE(crushed_result);
    auto uncrushed_result = yase_json::try_uncrush(*crushed_result);
    REQUIRE(uncrushed_result);
    INFO("sample size: " << s.size());
    REQUIRE(*uncrushed_result == s);
  }
}

TEST_CASE("Property: FastCrusher round-trip after warm_up with varied inputs", "[property][fast_crush]") {
  yase_json::FastCrusher crusher;
  crusher.warm_up(R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})");

  auto const inputs = std::vector<std::string>{
      R"({"key_0":9.9,"key_1":8.8,"key_2":7.7})",
      R"({"key_0":"N|+","key_1":"a|b","key_2":"hello"})",
      std::string(R"({"key_0":1.0,"key_1":")") + std::string(50, 'x') + "\"}",
  };

  for (auto const& input : inputs) {
    auto crushed_result = crusher.try_crush(input);
    REQUIRE(crushed_result);
    auto uncrushed_result = yase_json::try_uncrush(*crushed_result);
    REQUIRE(uncrushed_result);
    INFO("input: " << input.substr(0, 40));
    REQUIRE(json_equal(input, *uncrushed_result));
  }
}
