#include <catch2/catch_all.hpp>
#include <string>

#include "yase-json/crush.hpp"

TEST_CASE("try_crush and try_uncrush basic", "[crush_overloads]") {
  auto const input = std::string{R"({"a":"value", "b":"value"})"};
  auto const expected_crushed = yase_json::try_crush(input);
  REQUIRE(expected_crushed);

  SECTION("try_crush returns success") {
    auto result = yase_json::try_crush(input);
    REQUIRE(result);
    REQUIRE(*result == *expected_crushed);
  }

  SECTION("try_uncrush returns original") {
    auto result = yase_json::try_uncrush(*expected_crushed);
    REQUIRE(result);
    REQUIRE(*result == input);
  }

  SECTION("round-trip preserves content") {
    auto crushed = yase_json::try_crush(input);
    REQUIRE(crushed);
    auto uncrushed = yase_json::try_uncrush(*crushed);
    REQUIRE(uncrushed);
    REQUIRE(*uncrushed == input);
  }
}

TEST_CASE("try_crush handles various inputs", "[crush_overloads]") {
  SECTION("simple object") {
    auto const input = R"({"x":100, "y":200})";
    auto result = yase_json::try_crush(input);
    REQUIRE(result);
    REQUIRE(!result->empty());
    // JSONCrush output ends with '_'
    REQUIRE(result->back() == '_');
  }

  SECTION("empty string") {
    auto result = yase_json::try_crush("");
    REQUIRE(result);
    REQUIRE(*result == "_");
  }

  SECTION("unicode content") {
    auto const input = R"({"emoji":"😀"})";
    auto result = yase_json::try_crush(input);
    REQUIRE(result);
    auto uncrushed = yase_json::try_uncrush(*result);
    REQUIRE(uncrushed);
    REQUIRE(*uncrushed == input);
  }
}
