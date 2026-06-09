#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/fast_compress_crusher.hpp"

namespace {

auto verify_json_equal(std::string_view original, std::string_view result) -> void {
  glz::generic original_parsed, result_parsed;
  REQUIRE(glz::read_json(original_parsed, original) == 0);
  REQUIRE(glz::read_json(result_parsed, result) == 0);

  auto original_out = std::string{};
  auto result_out = std::string{};
  REQUIRE(glz::write_json(original_parsed, original_out) == 0);
  REQUIRE(glz::write_json(result_parsed, result_out) == 0);
  REQUIRE(original_out == result_out);
}

} // namespace

TEST_CASE("FastCompressCrusher の基本動作", "[fast_compress_crusher]") {
  SECTION("warmup前のフォールバック出力が uncrush_decompress で復元できる") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})";
    auto const crushed = crusher.compress_crush(input);
    auto const result = crusher.uncrush_decompress(crushed);
    verify_json_equal(input, result);
  }

  SECTION("warmup後の compress_crush が uncrush_decompress で復元できる") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto const crushed1 = crusher.compress_crush(input1);
    auto const crushed2 = crusher.compress_crush(input2);
    auto const crushed3 = crusher.compress_crush(input3);

    verify_json_equal(input1, crusher.uncrush_decompress(crushed1));
    verify_json_equal(input2, crusher.uncrush_decompress(crushed2));
    verify_json_equal(input3, crusher.uncrush_decompress(crushed3));
  }

  SECTION("threshold=1 で初回から辞書が構築される") {
    auto crusher = yase_json::FastCompressCrusher{1};
    auto const input1 = R"({"a":1.0,"b":2.0})";
    auto const input2 = R"({"a":10.0,"b":20.0})";
    auto const input3 = R"({"a":100.0,"b":200.0})";

    // threshold=1: 1回目はフォールバック、2回目以降は辞書使用
    auto const crushed1 = crusher.compress_crush(input1);
    auto const crushed2 = crusher.compress_crush(input2);
    auto const crushed3 = crusher.compress_crush(input3);

    verify_json_equal(input1, crusher.uncrush_decompress(crushed1));
    verify_json_equal(input2, crusher.uncrush_decompress(crushed2));
    verify_json_equal(input3, crusher.uncrush_decompress(crushed3));
  }

  SECTION("reset() 後に再学習できる") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input1 = R"({"key_0":1.0,"key_1":2.0})";
    auto const input2 = R"({"key_0":10.0,"key_1":20.0})";
    auto const input3 = R"({"key_0":100.0,"key_1":200.0})";

    auto const crushed1 = crusher.compress_crush(input1);
    auto const crushed2 = crusher.compress_crush(input2);
    auto const crushed3 = crusher.compress_crush(input3);

    verify_json_equal(input1, crusher.uncrush_decompress(crushed1));
    verify_json_equal(input2, crusher.uncrush_decompress(crushed2));
    verify_json_equal(input3, crusher.uncrush_decompress(crushed3));

    crusher.reset();

    auto const crushed4 = crusher.compress_crush(input1);
    auto const crushed5 = crusher.compress_crush(input2);
    auto const crushed6 = crusher.compress_crush(input3);

    verify_json_equal(input1, crusher.uncrush_decompress(crushed4));
    verify_json_equal(input2, crusher.uncrush_decompress(crushed5));
    verify_json_equal(input3, crusher.uncrush_decompress(crushed6));
  }

  SECTION("キー集合変化後に reset() して動作する") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input_a1 = R"({"a":1.0})";
    auto const input_a2 = R"({"a":10.0})";
    auto const input_a3 = R"({"a":100.0})";

    auto const crushed_a1 = crusher.compress_crush(input_a1);
    auto const crushed_a2 = crusher.compress_crush(input_a2);
    auto const crushed_a3 = crusher.compress_crush(input_a3);

    verify_json_equal(input_a1, crusher.uncrush_decompress(crushed_a1));
    verify_json_equal(input_a2, crusher.uncrush_decompress(crushed_a2));
    verify_json_equal(input_a3, crusher.uncrush_decompress(crushed_a3));

    crusher.reset();

    auto const input_x1 = R"({"x":9.0,"y":8.0})";
    auto const input_x2 = R"({"x":90.0,"y":80.0})";
    auto const input_x3 = R"({"x":900.0,"y":800.0})";

    auto const crushed_x1 = crusher.compress_crush(input_x1);
    auto const crushed_x2 = crusher.compress_crush(input_x2);
    auto const crushed_x3 = crusher.compress_crush(input_x3);

    verify_json_equal(input_x1, crusher.uncrush_decompress(crushed_x1));
    verify_json_equal(input_x2, crusher.uncrush_decompress(crushed_x2));
    verify_json_equal(input_x3, crusher.uncrush_decompress(crushed_x3));
  }
}

TEST_CASE("FastCompressCrusher の出力の意味的等価性", "[fast_compress_crusher]") {
  SECTION("compress_crush の出力が通常パスと意味的に等価") {
    auto compressor = yase_json::FastCompressor{};
    auto crusher = yase_json::FastCrusher{};
    auto decompressor = yase_json::Decompressor{};
    auto fast = yase_json::FastCompressCrusher{2};

    auto const inputs = {
      R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})",
      R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})",
      R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})",
      R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})",
    };

    auto input_vec = std::vector<std::string>{inputs.begin(), inputs.end()};
    for (auto const& input : input_vec) {
      // 通常パス: compress → crush → uncrush → decompress
      auto const compressed = compressor.compress(input);
      auto const crushed = yase_json::crush(compressed);
      auto const uncrushed = yase_json::uncrush(crushed);
      auto const normal_result = decompressor.decompress(uncrushed);

      // Fast パス
      auto const fast_crushed = fast.compress_crush(input);
      auto const fast_result = fast.uncrush_decompress(fast_crushed);

      // 意味的に等価であること
      verify_json_equal(normal_result, fast_result);
    }
  }
}
