#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"

namespace {

using Clock = std::chrono::high_resolution_clock;

struct BenchResult {
  std::string label;
  size_t original_size{};
  size_t compressed_size{};
  double compress_ms{};
  double decompress_ms{};
  size_t iterations{};
};

auto make_small_json() -> std::string {
  return R"({
    "id": 42,
    "name": "alice",
    "active": true,
    "score": 98.5,
    "tags": ["dev", "cpp", "json"],
    "meta": {
      "created": "2026-01-15T10:30:00Z",
      "updated": "2026-06-11T12:00:00Z",
      "version": 3,
      "checksum": "a1b2c3d4e5f6"
    },
    "config": {
      "theme": "dark",
      "locale": "ja-JP",
      "notifications": false,
      "timeout": 30000
    },
    "items": [
      {"id": 1, "val": "foo"},
      {"id": 2, "val": "bar"},
      {"id": 3, "val": "baz"}
    ]
  })";
}

auto make_medium_json() -> std::string {
  auto json = std::string{};
  json += R"([)";
  for (int i = 0; i < 500; ++i) {
    json += R"({"id":)" + std::to_string(i) +
            R"(,"type":"event")" +
            R"(,"action":"click")" +
            R"(,"label":"btn_)" + std::to_string(i % 20) + R"(")" +
            R"(,"value":)" + std::to_string(i * 1.5) +
            R"(,"meta":{"browser":"Chrome","os":"Linux","screen":"1920x1080","ts":")" +
            "2026-06-11T" +
            (i / 60 < 10 ? "0" : "") + std::to_string(i / 60) + ":" +
            (i % 60 < 10 ? "0" : "") + std::to_string(i % 60) + ":00Z" +
            R"("})" +
            R"(,"tags":[)" +
            R"("dev",)" +
            (i % 3 == 0 ? R"("critical")" : R"("normal")") +
            R"(]})";
    if (i < 499) json += ",";
  }
  json += R"(])";
  return json;
}

auto make_large_json() -> std::string {
  auto json = std::string{};
  json += R"({"records":[)";
  for (int i = 0; i < 3000; ++i) {
    auto group_id = i / 100;
    json += R"({"uid":")" + std::to_string(10000 + i) + R"(")"
            R"(,"gid":)" + std::to_string(group_id) +
            R"(,"type":"item")" +
            R"(,"status":)" + (i % 4 == 0 ? "true" : "false") +
            R"(,"priority":)" + std::to_string(i % 5) +
            R"(,"data":{"label":"item_)" + std::to_string(i % 50) + R"(")" +
            R"(,"count":)" + std::to_string(i * 10) +
            R"(,"ratio":)" + std::to_string(1.0 / (i + 1)) +
            R"(,"nested":{"a":)" + std::to_string(i) +
            R"(,"b":)" + std::to_string(i * 2) +
            R"(,"c":)" + std::to_string(i * 3) +
            R"(,"inner":[)" + std::to_string(i) + "," + std::to_string(i + 1) + "," + std::to_string(i + 2) +
            R"(]}}})";
    if (i < 2999) json += ",";
  }
  json += R"(],"total":)" + std::to_string(3000) +
          R"(,"page":1})";
  return json;
}

auto roundtrip_verify(std::string_view original, std::string_view decompressed) -> bool {
  glz::generic orig_parsed, dec_parsed;
  if (glz::read_json(orig_parsed, original)) return false;
  if (glz::read_json(dec_parsed, decompressed)) return false;
  std::string orig_out, dec_out;
  if (glz::write_json(orig_parsed, orig_out)) return false;
  if (glz::write_json(dec_parsed, dec_out)) return false;
  return orig_out == dec_out;
}

auto run_benchmark(std::string const& label, std::string const& json, int iterations) -> BenchResult {
  // warm-up: one compress + decompress to populate caches, verification
  auto warm_compressed_result = yase_json::try_compress(json);
  if (!warm_compressed_result) {
    std::cerr << "COMPRESS FAILED: " << label << " - " << warm_compressed_result.error().message << "\n";
    std::exit(EXIT_FAILURE);
  }
  auto const& warm_compressed = *warm_compressed_result;

  auto warm_decompressed_result = yase_json::try_decompress(warm_compressed);
  if (!warm_decompressed_result) {
    std::cerr << "DECOMPRESS FAILED: " << label << " - " << warm_decompressed_result.error().message << "\n";
    std::exit(EXIT_FAILURE);
  }
  auto const& warm_decompressed = *warm_decompressed_result;

  if (!roundtrip_verify(json, warm_decompressed)) {
    std::cerr << "VERIFICATION FAILED: " << label << "\n";
    std::exit(EXIT_FAILURE);
  }

  // measure compress
  auto const comp_start = Clock::now();
  std::string last_compressed;
  for (int i = 0; i < iterations; ++i) {
    auto result = yase_json::try_compress(json);
    if (!result) {
      std::cerr << "COMPRESS FAILED during benchmark\n";
      std::exit(EXIT_FAILURE);
    }
    last_compressed = std::move(*result);
  }
  auto const comp_end = Clock::now();

  // measure decompress
  auto const dec_start = Clock::now();
  std::string last_decompressed;
  for (int i = 0; i < iterations; ++i) {
    auto result = yase_json::try_decompress(last_compressed);
    if (!result) {
      std::cerr << "DECOMPRESS FAILED during benchmark\n";
      std::exit(EXIT_FAILURE);
    }
    last_decompressed = std::move(*result);
  }
  auto const dec_end = Clock::now();

  auto const comp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(comp_end - comp_start).count();
  auto const dec_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(dec_end - dec_start).count();

  return {
    .label = label,
    .original_size = json.size(),
    .compressed_size = warm_compressed.size(),
    .compress_ms = comp_ns / 1.0e6 / iterations,
    .decompress_ms = dec_ns / 1.0e6 / iterations,
    .iterations = static_cast<size_t>(iterations),
  };
}

auto format_bytes(size_t bytes) -> std::string {
  if (bytes >= 1024 * 1024) return std::to_string(bytes / (1024.0 * 1024.0)).substr(0, 5) + " MB";
  if (bytes >= 1024) return std::to_string(bytes / 1024.0).substr(0, 5) + " KB";
  return std::to_string(bytes) + " B";
}

auto print_separator() { std::cout << "--------------------------------------------------------------\n"; }

auto print_header() {
  print_separator();
  std::cout << "| Size       | Original   | Compressed | Ratio  | Compress  | Decompress | Iter |\n";
  print_separator();
}

auto print_result(BenchResult const& r) {
  auto ratio = static_cast<double>(r.compressed_size) / r.original_size * 100.0;
  printf("| %-10s | %-10s | %-10s | %5.1f%% | %8.3fms | %9.3fms | %4zu |\n",
         r.label.c_str(),
         format_bytes(r.original_size).c_str(),
         format_bytes(r.compressed_size).c_str(),
         ratio,
         r.compress_ms,
         r.decompress_ms,
         r.iterations);
}

} // namespace

int main() {
  std::cout << "=== yase-json Benchmark ===\n\n";

  // generate test data
  std::cout << "Generating test data...\n";
  auto const small = make_small_json();
  auto const medium = make_medium_json();
  auto const large = make_large_json();

  std::cout << "  small:  " << format_bytes(small.size()) << "\n";
  std::cout << "  medium: " << format_bytes(medium.size()) << "\n";
  std::cout << "  large:  " << format_bytes(large.size()) << "\n\n";

  print_header();

  auto const r_small  = run_benchmark("small",  small,  1000);
  print_result(r_small);

  auto const r_medium = run_benchmark("medium", medium, 100);
  print_result(r_medium);

  auto const r_large  = run_benchmark("large",  large,  10);
  print_result(r_large);

  print_separator();

  std::cout << "\nAll verifications passed. Benchmark complete.\n";
  return 0;
}
