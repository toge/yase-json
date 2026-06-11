
#include <catch2/catch_all.hpp>
#include <glaze/glaze.hpp>

#include "yase-json/fast_compress_crusher.hpp"
#include "yase-json/decompress.hpp"

TEST_CASE("StaticFastCompressCrusher のフィールド選択", "[fast_compress_crusher]") {
  SECTION("指定フィールドのみが圧縮・復元される") {
    yase_json::StaticFastCompressCrusher<"name", "age"> crusher{2};

    auto const json = R"({"name": "toge", "age": 25, "gender": "male"})";
    auto const crushed = crusher.compress_crush(json);
    auto const result = crusher.uncrush_decompress(crushed);

    glz::generic data;
    REQUIRE(glz::read_json(data, result) == 0);
    auto const& obj = data.get<glz::generic::object_t>();
    CHECK(obj.contains("name"));
    CHECK(obj.contains("age"));
    CHECK_FALSE(obj.contains("gender"));
    CHECK(obj.at("name").get<std::string>() == "toge");
    CHECK(obj.at("age").get<double>() == 25.0);
  }

  SECTION("warmup後の compress_crush がフィールド選択と併用できる") {
    yase_json::StaticFastCompressCrusher<"key_0", "key_1"> crusher{2};

    auto const input1 = R"({"key_0":10.0,"key_1":20.0,"key_2":30.0})";
    auto const input2 = R"({"key_0":100.0,"key_1":200.0,"key_2":300.0})";
    auto const input3 = R"({"key_0":1000.0,"key_1":2000.0,"key_2":3000.0})";

    auto const crushed1 = crusher.compress_crush(input1);
    auto const crushed2 = crusher.compress_crush(input2);
    auto const crushed3 = crusher.compress_crush(input3);

    auto verify_no_key2 = [](std::string_view json) {
      glz::generic data;
      REQUIRE(glz::read_json(data, json) == 0);
      auto const& obj = data.get<glz::generic::object_t>();
      CHECK(obj.contains("key_0"));
      CHECK(obj.contains("key_1"));
      CHECK_FALSE(obj.contains("key_2"));
    };

    verify_no_key2(crusher.uncrush_decompress(crushed1));
    verify_no_key2(crusher.uncrush_decompress(crushed2));
    verify_no_key2(crusher.uncrush_decompress(crushed3));
  }
}
