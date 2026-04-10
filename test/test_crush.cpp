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

  SECTION("Tie-breaking matches official JSONCrush order") {
    auto const input = std::string{"ab:ab:ab:ab"};
    auto const expected = std::string{"!!!ab!ab:\u0001!_"};

    REQUIRE(yase_json::crush(input) == expected);
  }

  SECTION("Unicode exact output matches official JSONCrush") {
    auto const input = std::string{R"({"emoji":"😀😀😀😀","word":"éééé"})"};
    auto const expected = std::string{"('emoji!'****'~word!'--')*😀-éé\u0001-*_"};

    REQUIRE(yase_json::crush(input) == expected);
  }

  SECTION("Surrogate-adjacent repeats match official JSONCrush") {
    auto const input = std::string{R"({"mixed":"alpha😀alpha😀alpha"})"};
    auto const expected = std::string{"('mixed!'**-')*-😀-alpha\u0001-*_"};

    REQUIRE(yase_json::crush(input) == expected);
  }

  SECTION("Nested repeated numeric sequences match official JSONCrush") {
    auto const input = std::string{R"({"nested":{"arr":[1,2,3,1,2,3],"obj":{"a":1,"b":1}}})"};
    auto const expected = std::string{"('nested!('arr![*,*]~obj!('a!1~b!1)))*1,2,3\u0001*_"};

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

  SECTION("Unicode exact output uncrushes correctly") {
    auto const input = std::string{"('emoji!'****'~word!'--')*😀-éé\u0001-*_"};
    auto const expected = std::string{R"({"emoji":"😀😀😀😀","word":"éééé"})"};

    REQUIRE(yase_json::uncrush(input) == expected);
  }

  SECTION("Surrogate-adjacent official output uncrushes correctly") {
    auto const input = std::string{"('mixed!'**-')*-😀-alpha\u0001-*_"};
    auto const expected = std::string{R"({"mixed":"alpha😀alpha😀alpha"})"};

    REQUIRE(yase_json::uncrush(input) == expected);
  }

  SECTION("Nested repeated numeric official output uncrushes correctly") {
    auto const input = std::string{"('nested!('arr![*,*]~obj!('a!1~b!1)))*1,2,3\u0001*_"};
    auto const expected = std::string{R"({"nested":{"arr":[1,2,3,1,2,3],"obj":{"a":1,"b":1}}})"};

    REQUIRE(yase_json::uncrush(input) == expected);
  }
}

TEST_CASE("crush output ordering regression", "[crush]") {
  SECTION("Repeated replacement candidate keeps official output ordering") {
    auto const input = std::string{R"({"k1":"abcabcabc","k2":"abcabcabc"})"};
    auto const expected = std::string{"('k1*~k2*)*!'---'-abc\u0001-*_"};

    REQUIRE(yase_json::crush(input) == expected);
    REQUIRE(yase_json::crush(input) == yase_json::crush(input));
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
