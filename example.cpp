#include <iostream>
#include <string>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"

int main() {
  std::string json_str = R"([{"name":"item","val":1},{"name":"item","val":1}])";

  yase_json::Compressor compressor;
  std::string compressed = compressor.compress(json_str);
  std::cout << "Compressed: " << compressed << std::endl;

  yase_json::Decompressor decompressor;
  std::string decompressed = decompressor.decompress(compressed);
  std::cout << "Decompressed: " << decompressed << std::endl;

  auto crushed = yase_json::crush(json_str);
  std::cout << "Crushed: " << crushed << std::endl;
  auto uncrushed = yase_json::uncrush(crushed);
  std::cout << "Uncrushed: " << uncrushed << std::endl;

  return 0;
}
