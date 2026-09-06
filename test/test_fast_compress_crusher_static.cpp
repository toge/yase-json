
#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/fast_compress_crusher.hpp"
#include "yase-json/decompress.hpp"

TEST_CASE("StaticFastCompressCrusher のフィールド選択", "[fast_compress_crusher]") {
  SECTION("指定フィールドのみが圧縮・復元される") {
    yase_json::StaticFastCompressCrusher<"name", "age"> crusher{2};

    auto const json = R"({"name": "toge", "age": 25, "gender": "male"})";
    auto crushed_result = crusher.try_compress_crush(json);
    REQUIRE(crushed_result);
    auto result = crusher.try_uncrush_decompress(*crushed_result);
    REQUIRE(result);

    glz::generic data;
    REQUIRE(glz::read_json(data, *result) == 0);
    auto const& obj = data.get<glz::generic::object_t>();
    CHECK(obj.contains("name"));
    CHECK(obj.contains("age"));
    CHECK_FALSE(obj.contains("gender"));
    CHECK(obj.at("name").get<std::string>() == "toge");
    CHECK(obj.at("age").get<double>() == 25.0);
  }

  SECTION("warmup後の try_compress_crush がフィールド選択と併用できる") {
    yase_json::StaticFastCompressCrusher<"key_0", "key_1"> crusher{2};

    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto crushed1_result = crusher.try_compress_crush(input1);
    auto crushed2_result = crusher.try_compress_crush(input2);
    auto crushed3_result = crusher.try_compress_crush(input3);
    REQUIRE(crushed1_result);
    REQUIRE(crushed2_result);
    REQUIRE(crushed3_result);

    auto verify_no_key2 = [](std::string_view json) {
      glz::generic data;
      REQUIRE(glz::read_json(data, json) == 0);
      auto const& obj = data.get<glz::generic::object_t>();
      CHECK(obj.contains("key_0"));
      CHECK(obj.contains("key_1"));
      CHECK_FALSE(obj.contains("key_2"));
    };

    auto result1 = crusher.try_uncrush_decompress(*crushed1_result);
    auto result2 = crusher.try_uncrush_decompress(*crushed2_result);
    auto result3 = crusher.try_uncrush_decompress(*crushed3_result);
    REQUIRE(result1);
    REQUIRE(result2);
    REQUIRE(result3);

    verify_no_key2(*result1);
    verify_no_key2(*result2);
    verify_no_key2(*result3);
  }
}
