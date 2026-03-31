#include <catch2/catch_all.hpp>

#include "yase-json/crush.hpp"

TEST_CASE("JSONCrush symmetry", "[crush]") {
  yase_json::JSONCrush crusher;

  SECTION("Simple string") {
    std::string input = R"({"name":"John", "age":30, "city":"New York"})";
    std::string crushed = crusher.crush(input);
    std::string uncrushed = crusher.uncrush(crushed);
    REQUIRE(input == uncrushed);
  }

  SECTION("Redundant string") {
    std::string input = R"({"a":"value", "b":"value", "c":"value", "d":"value"})";
    std::string crushed = crusher.crush(input);
    // Should be smaller than original if crushed effectively
    // But the main goal here is symmetry
    std::string uncrushed = crusher.uncrush(crushed);
    REQUIRE(input == uncrushed);
  }

  SECTION("Long repetitive string") {
    std::string input = "";
    for (int i = 0; i < 10; ++i) {
      input += "repeat_this_pattern_";
    }
    std::string crushed = crusher.crush(input);
    std::string uncrushed = crusher.uncrush(crushed);
    REQUIRE(input == uncrushed);
  }

  SECTION("String with no unused characters") {
    // This might be tricky if JSONCrush depends on unused characters
    std::string input = std::string(yase_json::JS_CRUSH_CHARS);
    std::string crushed = crusher.crush(input);
    std::string uncrushed = crusher.uncrush(crushed);
    REQUIRE(input == uncrushed);
  }

  SECTION("Empty string") {
    std::string input = "";
    std::string crushed = crusher.crush(input);
    std::string uncrushed = crusher.uncrush(crushed);
    REQUIRE(input == uncrushed);
  }
}
