#include <catch2/catch_all.hpp>
#include <yase-json/fast_compress.hpp>
#include <yase-json/decompress.hpp>
#include <glaze/glaze.hpp>

TEST_CASE("FastCompressorSelectiveTest", "[selective]") {
  yase_json::FastCompressor compressor;
  compressor.set_fields({"name", "age"});

  std::string json = R"({"name": "John", "age": 30, "city": "New York"})";
  auto compressed_result = compressor.try_compress(json);
  REQUIRE(compressed_result);
  auto const& compressed = *compressed_result;

  auto decompressed_result = yase_json::try_decompress(compressed);
  REQUIRE(decompressed_result);
  auto const& decompressed = *decompressed_result;

  glz::generic data;
  auto ec = glz::read_json(data, decompressed);
  REQUIRE_FALSE(ec);
  REQUIRE(data.is_object());

  auto const& obj = data.get<glz::generic::object_t>();
  CHECK(obj.contains("name"));
  CHECK(obj.contains("age"));
  CHECK_FALSE(obj.contains("city"));

  CHECK(obj.at("name").get<std::string>() == "John");
  CHECK(obj.at("age").get<double>() == 30.0);
}
