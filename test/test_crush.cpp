#include <catch2/catch_all.hpp>

#include "yase-json/crush.hpp"

TEST_CASE("crush matches official JSONCrush output", "[crush]") {
  SECTION("Empty string") {
    auto const input = std::string{};
    auto const expected = std::string{"_"};

    REQUIRE(yase_json::crush(input) == expected);
  }

  SECTION("Repeated JSON values") {
    auto const input = std::string{R"({"a":"value", "b":"value", "c":"value", "d":"value"})"};
    auto const expected = std::string{"('a*b*c*d-)*-, '-!'value'\u0001-*_"};

    REQUIRE(yase_json::crush(input) == expected);
  }

  SECTION("Nested JSON object and array") {
    auto const input = std::string{R"({"students":[{"name":"Jack","age":17},{"name":"Jill","age":16}],"class":"math"})"};
    auto const expected = std::string{"('students![*ack-7),*ill-6)]~class!'math')*('name!'J-'~age!1\u0001-*_"};

    REQUIRE(yase_json::crush(input) == expected);
  }
}

TEST_CASE("uncrush accepts official JSONCrush output", "[crush]") {
  SECTION("Empty string") {
    auto const input = std::string{"_"};
    auto const expected = std::string{};

    REQUIRE(yase_json::uncrush(input) == expected);
  }

  SECTION("Repeated JSON values") {
    auto const input = std::string{"('a*b*c*d-)*-, '-!'value'\u0001-*_"};
    auto const expected = std::string{R"({"a":"value", "b":"value", "c":"value", "d":"value"})"};

    REQUIRE(yase_json::uncrush(input) == expected);
  }

  SECTION("Nested JSON object and array") {
    auto const input = std::string{"('students![*ack-7),*ill-6)]~class!'math')*('name!'J-'~age!1\u0001-*_"};
    auto const expected = std::string{R"({"students":[{"name":"Jack","age":17},{"name":"Jill","age":16}],"class":"math"})"};

    REQUIRE(yase_json::uncrush(input) == expected);
  }
}

TEST_CASE("crush and uncrush remain symmetric", "[crush]") {
  SECTION("Long repetitive string") {
    auto input = std::string{};
    for (auto const _ : std::views::iota(0, 10)) {
      std::ignore = _;
      input += "repeat_this_pattern_";
    }

    REQUIRE(yase_json::uncrush(yase_json::crush(input)) == input);
  }
}
