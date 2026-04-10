#include <iostream>
#include <chrono>
#include <string>

#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"
#include "yase-json/crush.hpp"

int main() {
  std::string json_str = R"([)";
  for (int i = 0; i < 5000; ++i) {
    json_str += R"({"id":)" + std::to_string(i) + R"(, "type": "user_event", "data": {"action": "click", "timestamp": "2026-03-31T12:00:00Z", "meta": {"browser": "Chrome", "os": "Linux"}}})";
    if (i < 4999) {
      json_str += ",";
    }
  }
  json_str += "]";

  yase_json::Compressor compressor;
  yase_json::Decompressor decompressor;

  try {
    using clock_type = std::chrono::steady_clock;
    auto const start = clock_type::now();

    // 1. Compression (Structural)
    auto compressed_str = compressor.compress(json_str);
    auto const checkpoint1 = clock_type::now();

    // 2. JSONCrush (String level)
    auto crushed = yase_json::crush(compressed_str);
    auto const checkpoint2 = clock_type::now();

    // 3. JSONUncrush
    auto uncrushed = yase_json::uncrush(crushed);
    auto const checkpoint3 = clock_type::now();

    // 4. Decompression (Structural)
    auto decompressed = decompressor.decompress(uncrushed);
    auto const checkpoint4 = clock_type::now();

    auto const d1 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint1 - start).count();
    auto const d2 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint2 - checkpoint1).count();
    auto const d3 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint3 - checkpoint2).count();
    auto const d4 = std::chrono::duration_cast<std::chrono::milliseconds>(checkpoint4 - checkpoint3).count();

    std::cout << "--- Benchmark Results ---\n";
    std::cout << "OriginalBytes=" << json_str.size() << '\n';
    std::cout << "CompressedBytes=" << compressed_str.size() << '\n';
    std::cout << "CrushedBytes=" << crushed.size() << '\n';
    std::cout << "CompressionMs=" << d1 << '\n';
    std::cout << "JSONCrushMs=" << d2 << '\n';
    std::cout << "JSONUncrushMs=" << d3 << '\n';
    std::cout << "DecompressionMs=" << d4 << '\n';
    std::cout << "TotalMs=" << (d1 + d2 + d3 + d4) << '\n';

    // Verify correctness
    if (uncrushed != compressed_str) {
      std::cerr << "Verification failed: uncrushed != compressed_str\n";
      return 1;
    }
    // Deep check (optional but recommended in benchmark)
    glz::generic original_parsed, decompressed_parsed;
    if (auto const ec = glz::read_json(original_parsed, json_str)) {
      throw std::runtime_error("Failed to read original JSON in verification: " + glz::format_error(ec, json_str));
    }
    if (auto const ec = glz::read_json(decompressed_parsed, decompressed)) {
      throw std::runtime_error("Failed to read decompressed JSON in verification: " + glz::format_error(ec, decompressed));
    }
    std::string s1, s2;
    if (auto const ec = glz::write_json(original_parsed, s1)) {
      throw std::runtime_error("Failed to write original JSON in verification");
    }
    if (auto const ec = glz::write_json(decompressed_parsed, s2)) {
      throw std::runtime_error("Failed to write decompressed JSON in verification");
    }

    if (s1 != s2) {
      std::cerr << "Verification failed: original != decompressed\n";
      return 1;
    }

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
