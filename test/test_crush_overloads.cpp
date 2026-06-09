#include <catch2/catch_all.hpp>
#include <iterator>
#include <string>
#include <vector>

#include "yase-json/crush.hpp"

TEST_CASE("crush and uncrush with std::string& buffer", "[crush_overloads]") {
  auto const input = std::string{R"({"a":"value", "b":"value"})"};
  auto const expected_crushed = yase_json::crush(input);
  
  SECTION("crush appends to string") {
    auto out = std::string{"header:"};
    auto const written = yase_json::crush(input, out);
    
    CHECK(written == expected_crushed.size());
    CHECK(out == "header:" + expected_crushed);
  }
  
  SECTION("uncrush appends to string") {
    auto out = std::string{"result:"};
    auto const written = yase_json::uncrush(expected_crushed, out);
    
    CHECK(written == input.size());
    CHECK(out == "result:" + input);
  }
}

TEST_CASE("crush and uncrush with output_iterator", "[crush_overloads]") {
  auto const input = std::string{R"({"x":100, "y":200})"};
  auto const expected_crushed = yase_json::crush(input);

  SECTION("crush with back_inserter") {
    auto out = std::string{};
    auto it = yase_json::crush(input, std::back_inserter(out));
    
    CHECK(out == expected_crushed);
    // For back_insert_iterator, we can't easily compare it with anything other than itself,
    // but we can check if it's still usable.
    *it++ = '!';
    CHECK(out.back() == '!');
  }

  SECTION("uncrush with back_inserter") {
    auto out = std::vector<char>{};
    auto it = yase_json::uncrush(expected_crushed, std::back_inserter(out));
    
    auto const out_str = std::string(out.begin(), out.end());
    CHECK(out_str == input);
    *it++ = '?';
    CHECK(out.back() == '?');
  }

  SECTION("crush with pointer as iterator") {
    char buffer[1024];
    auto const it = yase_json::crush(input, buffer);
    
    auto const size = static_cast<std::size_t>(it - buffer);
    CHECK(size == expected_crushed.size());
    CHECK(std::string_view(buffer, size) == expected_crushed);
  }
}
