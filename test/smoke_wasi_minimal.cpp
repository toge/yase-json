// WASI Minimal smoke: try_* APIsのみで例外なしビルドを検証
// -DYASE_JSON_WASI_MINIMAL -fno-exceptions でビルド可能であることを確認する
#include <cassert>
#include <string>
#include <string_view>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"

int main() {
  constexpr std::string_view json = R"({"a":1,"b":[2,3]})";
  {
    auto c = yase_json::try_compress(json);
    assert(c && !c->empty());
    auto d = yase_json::try_decompress(*c);
    assert(d && *d == R"({"a":1,"b":[2,3]})");
  }
  {
    auto cr = yase_json::try_crush(json);
    assert(cr);
    auto unc = yase_json::try_uncrush(*cr);
    assert(unc && *unc == json);
  }
  {
    auto bad = yase_json::try_compress("{not json}");
    assert(!bad);
  }
  {
    auto bad = yase_json::try_decompress("[]");
    assert(!bad);
  }
  return 0;
}
