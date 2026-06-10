#include <catch2/catch_all.hpp>
#include <yase-json/fast_compress.hpp>
#include <yase-json/decompress.hpp>
#include <glaze/glaze.hpp>

TEST_CASE("FastCompressorSelectiveTest", "[selective]") {
  yase_json::FastCompressor compressor;
  compressor.set_fields({"name", "age"});

  std::string json = R"({"name": "John", "age": 30, "city": "New York"})";
  std::string compressed = compressor.compress(json);

  yase_json::Decompressor decompressor;
  std::string decompressed = decompressor.decompress(compressed);

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
