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

  SECTION("初回 try_compress は try_decompress で復元できる") {
    auto const input = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})";
    auto compressed_result = compressor.try_compress(input);
    REQUIRE(compressed_result);
    auto const& compressed = *compressed_result;

    auto decompressed_result = yase_json::try_decompress(compressed);
    REQUIRE(decompressed_result);
    verify_json_equal(input, *decompressed_result);
  }

  SECTION("2回目以降も try_decompress で復元できる") {
    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto compressed1_result = compressor.try_compress(input1);
    auto compressed2_result = compressor.try_compress(input2);
    auto compressed3_result = compressor.try_compress(input3);
    REQUIRE(compressed1_result);
    REQUIRE(compressed2_result);
    REQUIRE(compressed3_result);

    verify_json_equal(input1, *yase_json::try_decompress(*compressed1_result));
    verify_json_equal(input2, *yase_json::try_decompress(*compressed2_result));
    verify_json_equal(input3, *yase_json::try_decompress(*compressed3_result));
  }

  SECTION("キー集合変化後も正常に動作する") {
    auto const input_a = R"({"a":1.0,"b":2.0})";
    auto compressed_a_result = compressor.try_compress(input_a);
    REQUIRE(compressed_a_result);
    verify_json_equal(input_a, *yase_json::try_decompress(*compressed_a_result));

    compressor.reset();

    auto const input_x = R"({"x":9.0,"y":8.0})";
    auto compressed_x_result = compressor.try_compress(input_x);
    REQUIRE(compressed_x_result);
    verify_json_equal(input_x, *yase_json::try_decompress(*compressed_x_result));
  }

  SECTION("オブジェクト以外の入力はフォールバックする") {
    auto const input = R"([1,2,3])";
    auto fast_compressed_result = compressor.try_compress(input);
    REQUIRE(fast_compressed_result);
    auto const& fast_compressed = *fast_compressed_result;

    auto normal_compressed_result = yase_json::try_compress(input);
    REQUIRE(normal_compressed_result);

    // 通常の try_compress と同一出力になること
    REQUIRE(fast_compressed == *normal_compressed_result);

    // 復元確認
    verify_json_equal(input, *yase_json::try_decompress(fast_compressed));
  }

  SECTION("スキーマが固定されている場合、2回目以降の出力が安定する") {
    auto const input = R"({"key_0":42.0,"key_1":99.0})";
    auto compressed1_result = compressor.try_compress(input);
    auto compressed2_result = compressor.try_compress(input);
    REQUIRE(compressed1_result);
    REQUIRE(compressed2_result);
    REQUIRE(*compressed1_result == *compressed2_result);
  }
}

TEST_CASE("FastCrusher の基本動作", "[fast_crush]") {
  yase_json::FastCrusher crusher;

  SECTION("warm_up 後の try_crush が try_uncrush で復元できる") {
    auto const template_json = R"({"key_0":1.0,"key_1":"hello"})";
    crusher.warm_up(template_json);

    auto const input = R"({"key_0":9.9,"key_1":"world"})";
    auto crushed_result = crusher.try_crush(input);
    REQUIRE(crushed_result);
    auto uncrushed_result = crusher.try_uncrush(*crushed_result);
    REQUIRE(uncrushed_result);
    verify_json_equal(input, *uncrushed_result);
  }

  SECTION("warm_up なしの初回 try_crush が try_uncrush で復元できる") {
    auto const input = R"({"key_0":1.0,"key_1":2.0})";
    auto crushed_result = crusher.try_crush(input);
    REQUIRE(crushed_result);
    auto uncrushed_result = crusher.try_uncrush(*crushed_result);
    REQUIRE(uncrushed_result);
    verify_json_equal(input, *uncrushed_result);
  }

  SECTION("warm_up 後の複数回 try_crush が各回 try_uncrush で復元できる") {
    auto const template_json = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0})";
    crusher.warm_up(template_json);

    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto crushed1_result = crusher.try_crush(input1);
    auto crushed2_result = crusher.try_crush(input2);
    auto crushed3_result = crusher.try_crush(input3);
    REQUIRE(crushed1_result);
    REQUIRE(crushed2_result);
    REQUIRE(crushed3_result);

    verify_json_equal(input1, *crusher.try_uncrush(*crushed1_result));
    verify_json_equal(input2, *crusher.try_uncrush(*crushed2_result));
    verify_json_equal(input3, *crusher.try_uncrush(*crushed3_result));
  }

  SECTION("メンバ try_uncrush() が yase_json::try_uncrush() と同一結果を返す") {
    auto const input = R"({"key_0":1.0,"key_1":"hello"})";
    auto crushed_result = crusher.try_crush(input);
    REQUIRE(crushed_result);

    auto const result_member = crusher.try_uncrush(*crushed_result);
    auto const result_free = yase_json::try_uncrush(*crushed_result);
    REQUIRE(result_member);
    REQUIRE(result_free);
    REQUIRE(*result_member == *result_free);
  }

  SECTION("reset() 後に再 warm_up して正常動作する") {
    crusher.warm_up(R"({"a":1.0,"b":2.0})");
    auto crushed1_result = crusher.try_crush(R"({"a":10.0,"b":20.0})");
    REQUIRE(crushed1_result);
    verify_json_equal(R"({"a":10.0,"b":20.0})", *crusher.try_uncrush(*crushed1_result));

    crusher.reset();

    crusher.warm_up(R"({"x":1.0,"y":2.0})");
    auto crushed2_result = crusher.try_crush(R"({"x":90.0,"y":80.0})");
    REQUIRE(crushed2_result);
    verify_json_equal(R"({"x":90.0,"y":80.0})", *crusher.try_uncrush(*crushed2_result));
  }
}

TEST_CASE("FastCrusher dictionary collision handling (Bug B regression)", "[fast_crush][regression]") {
  yase_json::FastCrusher crusher;

  SECTION("置換文字が後続入力に含まれる場合でも復元が壊れない") {
    auto const template_json = R"({"key_0":1.0,"key_1":2.0,"key_2":3.0,"key_3":4.0})";
    crusher.warm_up(template_json);

    // 辞書の置換文字は 0x20-0xFE から選ばれる。ASCII範囲の置換文字が
    // 後続入力に含まれると以前は復元が破損した。per-entry skip により
    // 正しく復元されることを検証(有効なUTF-8のみを使用)
    std::string many_chars = R"({"k":")";
    for (int c = 0x20; c < 0x7F; ++c) {
      if (c == '"' || c == '\\') continue;
      many_chars.push_back(static_cast<char>(c));
    }
    many_chars += "\"}";
    auto crushed_result = crusher.try_crush(many_chars);
    REQUIRE(crushed_result);
    auto uncrushed_result = yase_json::try_uncrush(*crushed_result);
    REQUIRE(uncrushed_result);
    verify_json_equal(many_chars, *uncrushed_result);
    // crusher.try_uncrush も同一
    auto member_result = crusher.try_uncrush(*crushed_result);
    REQUIRE(member_result);
    verify_json_equal(many_chars, *member_result);
  }

  SECTION("ASCIIのみの入力では従来通り圧縮・復元できる") {
    crusher.warm_up(R"({"a":1.0,"b":2.0})");
    auto const input = R"({"a":10.0,"b":20.0})";
    auto crushed_result = crusher.try_crush(input);
    REQUIRE(crushed_result);
    verify_json_equal(input, *yase_json::try_uncrush(*crushed_result));
    auto member_result = crusher.try_uncrush(*crushed_result);
    REQUIRE(member_result);
    verify_json_equal(input, *member_result);
  }
}

TEST_CASE("FastCompressor schema_cache 整合性", "[fast_compress]") {
  yase_json::FastCompressor compressor;

  SECTION("異なるキー集合を reset 後に再圧縮しても正しく復元できる") {
    for (auto i = 0; i < 3; ++i) {
      auto const input = R"({"a":1.0,"b":2.0,"c":3.0})";
      auto compressed_result = compressor.try_compress(input);
      REQUIRE(compressed_result);
      verify_json_equal(input, *yase_json::try_decompress(*compressed_result));
    }
    compressor.reset();
    for (auto i = 0; i < 3; ++i) {
      auto const input = R"({"x":9.0,"y":8.0,"z":7.0})";
      auto compressed_result = compressor.try_compress(input);
      REQUIRE(compressed_result);
      verify_json_equal(input, *yase_json::try_decompress(*compressed_result));
    }
  }
}
