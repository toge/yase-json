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
  SECTION("warmup前のフォールバック出力が try_uncrush_decompress で復元できる") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})";
    auto crushed_result = crusher.try_compress_crush(input);
    REQUIRE(crushed_result);
    auto result = crusher.try_uncrush_decompress(*crushed_result);
    REQUIRE(result);
    verify_json_equal(input, *result);
  }

  SECTION("warmup後の try_compress_crush が try_uncrush_decompress で復元できる") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto crushed1_result = crusher.try_compress_crush(input1);
    auto crushed2_result = crusher.try_compress_crush(input2);
    auto crushed3_result = crusher.try_compress_crush(input3);
    REQUIRE(crushed1_result);
    REQUIRE(crushed2_result);
    REQUIRE(crushed3_result);

    verify_json_equal(input1, *crusher.try_uncrush_decompress(*crushed1_result));
    verify_json_equal(input2, *crusher.try_uncrush_decompress(*crushed2_result));
    verify_json_equal(input3, *crusher.try_uncrush_decompress(*crushed3_result));
  }

  SECTION("threshold=1 で初回から辞書が構築される") {
    auto crusher = yase_json::FastCompressCrusher{1};
    auto const input1 = R"({"a":1.0,"b":2.0})";
    auto const input2 = R"({"a":10.0,"b":20.0})";
    auto const input3 = R"({"a":100.0,"b":200.0})";

    // threshold=1: 1回目はフォールバック、2回目以降は辞書使用
    auto crushed1_result = crusher.try_compress_crush(input1);
    auto crushed2_result = crusher.try_compress_crush(input2);
    auto crushed3_result = crusher.try_compress_crush(input3);
    REQUIRE(crushed1_result);
    REQUIRE(crushed2_result);
    REQUIRE(crushed3_result);

    verify_json_equal(input1, *crusher.try_uncrush_decompress(*crushed1_result));
    verify_json_equal(input2, *crusher.try_uncrush_decompress(*crushed2_result));
    verify_json_equal(input3, *crusher.try_uncrush_decompress(*crushed3_result));
  }

  SECTION("reset() 後に再学習できる") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input1 = R"({"key_0":1.0,"key_1":2.0})";
    auto const input2 = R"({"key_0":10.0,"key_1":20.0})";
    auto const input3 = R"({"key_0":100.0,"key_1":200.0})";

    auto crushed1_result = crusher.try_compress_crush(input1);
    auto crushed2_result = crusher.try_compress_crush(input2);
    auto crushed3_result = crusher.try_compress_crush(input3);
    REQUIRE(crushed1_result);
    REQUIRE(crushed2_result);
    REQUIRE(crushed3_result);

    verify_json_equal(input1, *crusher.try_uncrush_decompress(*crushed1_result));
    verify_json_equal(input2, *crusher.try_uncrush_decompress(*crushed2_result));
    verify_json_equal(input3, *crusher.try_uncrush_decompress(*crushed3_result));

    crusher.reset();

    auto crushed4_result = crusher.try_compress_crush(input1);
    auto crushed5_result = crusher.try_compress_crush(input2);
    auto crushed6_result = crusher.try_compress_crush(input3);
    REQUIRE(crushed4_result);
    REQUIRE(crushed5_result);
    REQUIRE(crushed6_result);

    verify_json_equal(input1, *crusher.try_uncrush_decompress(*crushed4_result));
    verify_json_equal(input2, *crusher.try_uncrush_decompress(*crushed5_result));
    verify_json_equal(input3, *crusher.try_uncrush_decompress(*crushed6_result));
  }

  SECTION("キー集合変化後に reset() して動作する") {
    auto crusher = yase_json::FastCompressCrusher{2};
    auto const input_a1 = R"({"a":1.0})";
    auto const input_a2 = R"({"a":10.0})";
    auto const input_a3 = R"({"a":100.0})";

    auto crushed_a1_result = crusher.try_compress_crush(input_a1);
    auto crushed_a2_result = crusher.try_compress_crush(input_a2);
    auto crushed_a3_result = crusher.try_compress_crush(input_a3);
    REQUIRE(crushed_a1_result);
    REQUIRE(crushed_a2_result);
    REQUIRE(crushed_a3_result);

    verify_json_equal(input_a1, *crusher.try_uncrush_decompress(*crushed_a1_result));
    verify_json_equal(input_a2, *crusher.try_uncrush_decompress(*crushed_a2_result));
    verify_json_equal(input_a3, *crusher.try_uncrush_decompress(*crushed_a3_result));

    crusher.reset();

    auto const input_x1 = R"({"x":9.0,"y":8.0})";
    auto const input_x2 = R"({"x":90.0,"y":80.0})";
    auto const input_x3 = R"({"x":900.0,"y":800.0})";

    auto crushed_x1_result = crusher.try_compress_crush(input_x1);
    auto crushed_x2_result = crusher.try_compress_crush(input_x2);
    auto crushed_x3_result = crusher.try_compress_crush(input_x3);
    REQUIRE(crushed_x1_result);
    REQUIRE(crushed_x2_result);
    REQUIRE(crushed_x3_result);

    verify_json_equal(input_x1, *crusher.try_uncrush_decompress(*crushed_x1_result));
    verify_json_equal(input_x2, *crusher.try_uncrush_decompress(*crushed_x2_result));
    verify_json_equal(input_x3, *crusher.try_uncrush_decompress(*crushed_x3_result));
  }
}

TEST_CASE("FastCompressCrusher の出力の意味的等価性", "[fast_compress_crusher]") {
  SECTION("try_compress_crush の出力が通常パスと意味的に等価") {
    auto compressor = yase_json::FastCompressor{};
    auto crusher = yase_json::FastCrusher{};
    auto fast = yase_json::FastCompressCrusher{2};

    auto const inputs = {
      R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})",
      R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})",
      R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})",
      R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})",
    };

    auto input_vec = std::vector<std::string>{inputs.begin(), inputs.end()};
    for (auto const& input : input_vec) {
      // 通常パス: try_compress → try_crush → try_uncrush → try_decompress
      auto compressed_result = compressor.try_compress(input);
      REQUIRE(compressed_result);
      auto crushed_result = yase_json::try_crush(*compressed_result);
      REQUIRE(crushed_result);
      auto uncrushed_result = yase_json::try_uncrush(*crushed_result);
      REQUIRE(uncrushed_result);
      auto normal_result = yase_json::try_decompress(*uncrushed_result);
      REQUIRE(normal_result);

      // Fast パス
      auto fast_crushed_result = fast.try_compress_crush(input);
      REQUIRE(fast_crushed_result);
      auto fast_result = fast.try_uncrush_decompress(*fast_crushed_result);
      REQUIRE(fast_result);

      // 意味的に等価であること
      verify_json_equal(*normal_result, *fast_result);
    }
  }
}
