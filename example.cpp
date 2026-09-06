#include <iostream>
#include <string>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"

int main() {
  std::string json_str = R"(
{
  "key_0": 83.65503238356673,
  "key_1": 89.95409841521338,
  "key_2": 47.44338696958149,
  "key_3": 94.43725005738578,
  "key_4": 0.36392421970924826,
  "key_5": 68.47659554361542,
  "key_6": 14.436310626403683,
  "key_7": 31.42182555594355,
  "key_8": 40.578482117782904,
  "key_9": 4.781149012078
}
)";

  // try_* APIs are always available (exception-free)
  auto compressed_res = yase_json::try_compress(json_str);
  if (!compressed_res) {
    std::cerr << "compress failed: " << compressed_res.error().message << '\n';
    return 1;
  }
  std::string compressed = *compressed_res;
  std::cout << "Compressed: " << compressed << '\n';

  auto decompressed_res = yase_json::try_decompress(compressed);
  if (!decompressed_res) {
    std::cerr << "decompress failed: " << decompressed_res.error().message << '\n';
    return 1;
  }
  std::cout << "Decompressed: " << *decompressed_res << '\n';

  auto crushed_res = yase_json::try_crush(json_str);
  if (!crushed_res) {
    std::cerr << "crush failed: " << crushed_res.error().message << '\n';
    return 1;
  }
  std::cout << "Crushed: " << *crushed_res << '\n';
  auto uncrushed_res = yase_json::try_uncrush(*crushed_res);
  if (!uncrushed_res) {
    std::cerr << "uncrush failed: " << uncrushed_res.error().message << '\n';
    return 1;
  }
  std::cout << "Uncrushed: " << *uncrushed_res << '\n';

  return 0;
}
