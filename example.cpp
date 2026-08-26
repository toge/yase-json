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

  yase_json::Compressor compressor;
  std::string compressed = compressor.compress(json_str);
  std::cout << "Compressed: " << compressed << '\n';

  yase_json::Decompressor decompressor;
  std::string decompressed = decompressor.decompress(compressed);
  std::cout << "Decompressed: " << decompressed << '\n';

  auto crushed = yase_json::crush(json_str);
  std::cout << "Crushed: " << crushed << '\n';
  auto uncrushed = yase_json::uncrush(crushed);
  std::cout << "Uncrushed: " << uncrushed << '\n';

  return 0;
}
