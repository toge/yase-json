#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/fast_compress.hpp"

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

TEST_CASE("FastCompressor の基本動作", "[fast_compress]") {
  yase_json::FastCompressor compressor;
  yase_json::Decompressor decompressor;

  SECTION("初回 compress は Decompressor で復元できる") {
    auto const input = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})";
    auto const compressed = compressor.compress(input);
    auto const decompressed = decompressor.decompress(compressed);
    verify_json_equal(input, decompressed);
  }

  SECTION("2回目以降も Decompressor で復元できる") {
    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto const compressed1 = compressor.compress(input1);
    auto const compressed2 = compressor.compress(input2);
    auto const compressed3 = compressor.compress(input3);

    verify_json_equal(input1, decompressor.decompress(compressed1));
    verify_json_equal(input2, decompressor.decompress(compressed2));
    verify_json_equal(input3, decompressor.decompress(compressed3));
  }

  SECTION("キー集合変化後も正常に動作する") {
    auto const input_a = R"({"a":1.0,"b":2.0})";
    auto const compressed_a = compressor.compress(input_a);
    verify_json_equal(input_a, decompressor.decompress(compressed_a));

    compressor.reset();

    auto const input_x = R"({"x":9.0,"y":8.0})";
    auto const compressed_x = compressor.compress(input_x);
    verify_json_equal(input_x, decompressor.decompress(compressed_x));
  }

  SECTION("オブジェクト以外の入力はフォールバックする") {
    auto const input = R"([1,2,3])";
    auto const fast_compressed = compressor.compress(input);

    yase_json::Compressor normal_compressor;
    auto const normal_compressed = normal_compressor.compress(input);

    // 通常の Compressor と同一出力になること
    REQUIRE(fast_compressed == normal_compressed);

    // 復元確認
    verify_json_equal(input, decompressor.decompress(fast_compressed));
  }

  SECTION("スキーマが固定されている場合、2回目以降の出力が安定する") {
    auto const input = R"({"key_0":42.0,"key_1":99.0})";
    auto const compressed1 = compressor.compress(input);
    auto const compressed2 = compressor.compress(input);
    REQUIRE(compressed1 == compressed2);
  }
}

TEST_CASE("FastCrusher の基本動作", "[fast_crush]") {
  yase_json::FastCrusher crusher;

  SECTION("warm_up 後の crush が uncrush で復元できる") {
    auto const template_json = R"({"key_0":1.0,"key_1":"hello"})";
    crusher.warm_up(template_json);

    auto const input = R"({"key_0":9.9,"key_1":"world"})";
    auto const crushed = crusher.crush(input);
    auto const uncrushed = crusher.uncrush(crushed);
    verify_json_equal(input, uncrushed);
  }

  SECTION("warm_up なしの初回 crush が uncrush で復元できる") {
    auto const input = R"({"key_0":1.0,"key_1":2.0})";
    auto const crushed = crusher.crush(input);
    auto const uncrushed = crusher.uncrush(crushed);
    verify_json_equal(input, uncrushed);
  }

  SECTION("warm_up 後の複数回 crush が各回 uncrush で復元できる") {
    auto const template_json = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})";
    crusher.warm_up(template_json);

    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto const crushed1 = crusher.crush(input1);
    auto const crushed2 = crusher.crush(input2);
    auto const crushed3 = crusher.crush(input3);

    verify_json_equal(input1, crusher.uncrush(crushed1));
    verify_json_equal(input2, crusher.uncrush(crushed2));
    verify_json_equal(input3, crusher.uncrush(crushed3));
  }

  SECTION("メンバ uncrush() が yase_json::uncrush() と同一結果を返す") {
    auto const input = R"({"key_0":1.0,"key_1":"hello"})";
    auto const crushed = crusher.crush(input);

    auto const result_member = crusher.uncrush(crushed);
    auto const result_free = yase_json::uncrush(crushed);
    REQUIRE(result_member == result_free);
  }

  SECTION("reset() 後に再 warm_up して正常動作する") {
    crusher.warm_up(R"({"a":1.0,"b":2.0})");
    auto const crushed1 = crusher.crush(R"({"a":10.0,"b":20.0})");
    verify_json_equal(R"({"a":10.0,"b":20.0})", crusher.uncrush(crushed1));

    crusher.reset();

    crusher.warm_up(R"({"x":1.0,"y":2.0})");
    auto const crushed2 = crusher.crush(R"({"x":90.0,"y":80.0})");
    verify_json_equal(R"({"x":90.0,"y":80.0})", crusher.uncrush(crushed2));
  }
}

TEST_CASE("FastCompressor schema_cache 整合性", "[fast_compress]") {
  yase_json::FastCompressor compressor;
  yase_json::Decompressor decompressor;

  SECTION("異なるキー集合を reset 後に再圧縮しても正しく復元できる") {
    for (auto i = 0; i < 3; ++i) {
      auto const input = R"({"a":1.0,"b":2.0,"c":3.0})";
      auto const compressed = compressor.compress(input);
      verify_json_equal(input, decompressor.decompress(compressed));
    }
    compressor.reset();
    for (auto i = 0; i < 3; ++i) {
      auto const input = R"({"x":9.0,"y":8.0,"z":7.0})";
      auto const compressed = compressor.compress(input);
      verify_json_equal(input, decompressor.decompress(compressed));
    }
  }
}
