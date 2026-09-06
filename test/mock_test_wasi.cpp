// WASI mock test: verifies basic functionality without Catch2
// Builds with -fno-exceptions to confirm exception-free operation
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"

auto main() -> int {
  constexpr std::string_view json = R"({"a":1,"b":[2,3]})";
  int failures = 0;

  // Test compress/decompress round-trip
  {
    auto c = yase_json::try_compress(json);
    if (!c || c->empty()) {
      std::cerr << "FAIL: try_compress\n";
      ++failures;
    } else {
      auto d = yase_json::try_decompress(*c);
      if (!d || *d != R"({"a":1,"b":[2,3]})") {
        std::cerr << "FAIL: try_decompress\n";
        ++failures;
      }
    }
  }

  // Test crush/uncrush round-trip
  {
    auto cr = yase_json::try_crush(json);
    if (!cr) {
      std::cerr << "FAIL: try_crush\n";
      ++failures;
    } else {
      auto unc = yase_json::try_uncrush(*cr);
      if (!unc || *unc != json) {
        std::cerr << "FAIL: try_uncrush\n";
        ++failures;
      }
    }
  }

  // Test error handling - invalid JSON
  {
    auto bad = yase_json::try_compress("{not json}");
    if (bad) {
      std::cerr << "FAIL: try_compress should fail on invalid JSON\n";
      ++failures;
    }
  }

  // Test error handling - malformed compressed input
  {
    auto bad = yase_json::try_decompress("[]");
    if (bad) {
      std::cerr << "FAIL: try_decompress should fail on malformed input\n";
      ++failures;
    }
  }

  // Test error handling - out of range key
  {
    auto bad = yase_json::try_decompress(R"([["a|"],"1"])");
    if (bad) {
      std::cerr << "FAIL: try_decompress should fail on out-of-range key\n";
      ++failures;
    }
  }

  if (failures == 0) {
    std::cout << "All WASI mock tests passed.\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " test(s) failed.\n";
  return EXIT_FAILURE;
}
