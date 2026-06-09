#include <catch2/catch_all.hpp>
#include <iterator>
#include <string>
#include <vector>

#include "yase-json/pipeline.hpp"

TEST_CASE("pipeline::compress_and_crush overloads", "[pipeline_overloads]") {
  std::string const input = R"({"name":"test","data":[1,2,3,1,2,3]})";
  auto const expected = yase_json::pipeline::compress_and_crush(input);

  SECTION("std::string& overload") {
    std::string out = "prefix:";
    auto const written = yase_json::pipeline::compress_and_crush(input, out);
    CHECK(written == expected.size());
    CHECK(out == "prefix:" + expected);
  }

  SECTION("output_iterator overload") {
    std::string out;
    auto it = yase_json::pipeline::compress_and_crush(input, std::back_inserter(out));
    CHECK(out == expected);
    *it++ = '!';
    CHECK(out.back() == '!');
  }
}

TEST_CASE("pipeline::uncrush_and_decompress overloads", "[pipeline_overloads]") {
  std::string const input = R"({"name":"test","data":[1,2,3,1,2,3]})";
  auto const compressed = yase_json::pipeline::compress_and_crush(input);

  SECTION("std::string& overload") {
    std::string out = "result:";
    auto const written = yase_json::pipeline::uncrush_and_decompress(compressed, out);
    // Note: decompressed output might have different formatting than input, 
    // but uncrush_and_decompress returns a string that should match input in content.
    CHECK(written > 0);
    
    // Validate the result content
    std::string result = out.substr(7);
    CHECK(yase_json::pipeline::compress_and_crush(result) == compressed);
  }

  SECTION("output_iterator overload") {
    std::vector<char> out;
    auto it = yase_json::pipeline::uncrush_and_decompress(compressed, std::back_inserter(out));
    std::string result(out.begin(), out.end());
    CHECK(yase_json::pipeline::compress_and_crush(result) == compressed);
    *it++ = '?';
    CHECK(out.back() == '?');
  }
}
