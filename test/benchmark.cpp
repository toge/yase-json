#include <iostream>
#include <chrono>
#include <vector>
#include <string>

#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"

int main() {
  std::string json_str = R"([)";
  for (int i = 0; i < 500; ++i) {
    json_str += R"({"id":)" + std::to_string(i) + R"(, "type": "user_event", "data": {"action": "click", "timestamp": "2026-03-31T12:00:00Z", "meta": {"browser": "Chrome", "os": "Linux"}}})";
    if (i < 499) {
      json_str += ",";
    }
  }
  json_str += "]";

  glz::generic original;
  glz::read_json(original, json_str);

  yase_json::Compressor compressor;
  yase_json::Decompressor decompressor;
  yase_json::JSONCrush crusher;

  auto start = std::chrono::high_resolution_clock::now();

  // 1. Compression
  auto compressed = compressor.compress(original);
  auto checkpoint1 = std::chrono::high_resolution_clock::now();

  // 2. JSONCrush
  std::string compressed_str;
  glz::write_json(compressed, compressed_str);
  auto crushed = crusher.crush(compressed_str);
  auto checkpoint2 = std::chrono::high_resolution_clock::now();

  // 3. JSONUncrush
  auto uncrushed = crusher.uncrush(crushed);
  auto checkpoint3 = std::chrono::high_resolution_clock::now();

  // 4. Decompression
  glz::generic back;
  if (auto ec = glz::read_json(back, uncrushed)) {
    std::cerr << "JSON Parse Error after uncrush: " << (int)ec.ec << "\n";
    return 1;
  }
  auto decompressed = decompressor.decompress(back);
  auto checkpoint4 = std::chrono::high_resolution_clock::now();

  auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint1 - start).count();
  auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint2 - checkpoint1).count();
  auto d3 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint3 - checkpoint2).count();
  auto d4 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint4 - checkpoint3).count();

  std::cout << "--- Final Optimized Benchmark Results ---\n";
  std::cout << "Original Size:   " << json_str.size() << " bytes\n";
  std::cout << "Crushed Size:    " << crushed.size() << " bytes\n";
  std::cout << "Compression:     " << d1 << " ms\n";
  std::cout << "JSONCrush:       " << d2 << " ms\n";
  std::cout << "JSONUncrush:     " << d3 << " ms\n";
  std::cout << "Decompression:   " << d4 << " ms\n";
  std::cout << "Total Time:      " << (d1 + d2 + d3 + d4) << " ms\n";

  return 0;
}
