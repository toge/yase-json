// wasm3 ホストから FREESTANDING ゲスト (yase-json-guest.wasm) を実行するスモークテスト。
//
// 検証内容:
//   1. _initialize → 静的初期化子が動く
//   2. compress → decompress ラウンドトリップ (数値/文字列/配列/オブジェクト)
//   3. crush → uncrush ラウンドトリップ
//   4. 異常入力で -1 + ys_last_error にメッセージが入る
//
// ビルド: ゲスト wasm は CMake (freestanding/CMakeLists.txt) が生成し、
// GUEST_WASM マクロでパスを受ける。wasm3 コア (source/*.c) と共にリンク。

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "wasm3.h"

#ifndef GUEST_WASM
#define GUEST_WASM "yase-json-guest.wasm"
#endif

namespace {

int failures = 0;

auto check(bool const ok, char const* label) -> void {
  std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
  if (!ok) {
    ++failures;
  }
}

struct guest_t {
  IM3Environment env = nullptr;
  IM3Runtime runtime = nullptr;
  IM3Module module = nullptr;
  uint8_t* memory = nullptr;
  size_t memory_size = 0;

  IM3Function initialize{};
  IM3Function alloc{};
  IM3Function reset{};
  IM3Function last_error{};
  IM3Function last_error_len{};
  IM3Function compress{};
  IM3Function decompress{};
  IM3Function crush{};
  IM3Function uncrush{};

  ~guest_t() {
    if (runtime != nullptr) {
      m3_FreeRuntime(runtime);
    }
    if (env != nullptr) {
      m3_FreeEnvironment(env);
    }
  }
};

auto load_wasm(char const* path) -> std::vector<uint8_t> {
  auto* file = std::fopen(path, "rb");
  if (file == nullptr) {
    std::printf("cannot open %s\n", path);
    std::exit(1);
  }
  std::fseek(file, 0, SEEK_END);
  auto const size = static_cast<size_t>(std::ftell(file));
  std::fseek(file, 0, SEEK_SET);
  auto bytes = std::vector<uint8_t>(size);
  if (std::fread(bytes.data(), 1, size, file) != size) {
    std::printf("read error\n");
    std::exit(1);
  }
  std::fclose(file);
  return bytes;
}

IM3Runtime g_runtime = nullptr;

template <typename... Args>
auto call_scalar(IM3Function f, Args... args) -> int64_t {
  M3Result const result = m3_CallV(f, args...);
  if (result != m3Err_none) {
    std::printf("m3 error: %s\n", result);
    M3ErrorInfo info{};
    m3_GetErrorInfo(g_runtime, &info);
    std::printf("  trap in function: %s\n", info.function ? m3_GetFunctionName(info.function) : "?");
    return -2;
  }
  int32_t out = 0;
  if (m3_GetResultsV(f, &out) != m3Err_none) {
    return -2;
  }
  return out;
}

auto call_scalar(IM3Function f) -> int64_t {
  if (m3_CallV(f) != m3Err_none) {
    return -2;
  }
  int32_t result = 0;
  if (m3_GetResultsV(f, &result) != m3Err_none) {
    return -2;
  }
  return result;
}

} // namespace

auto main(int argc, char** argv) -> int {
  auto const wasm_path = argc > 1 ? argv[1] : GUEST_WASM;
  auto const wasm_bytes = load_wasm(wasm_path);

  auto guest = guest_t{};
  guest.env = m3_NewEnvironment();
  // glaze の再帰パース + yase の再帰圧縮はスタックを多用するため
  // wasm3 インタープリタスタックは 2MB 確保する
  guest.runtime = m3_NewRuntime(guest.env, 2 * 1024 * 1024, nullptr);
  g_runtime = guest.runtime;

  if (m3_ParseModule(guest.env, &guest.module, wasm_bytes.data(), static_cast<uint32_t>(wasm_bytes.size())) != m3Err_none) {
    std::printf("parse failed\n");
    return 1;
  }
  if (m3_LoadModule(guest.runtime, guest.module) != m3Err_none) {
    std::printf("load failed\n");
    return 1;
  }
  guest.memory = m3_GetMemory(guest.module, &guest.memory_size, 0);
  check(guest.memory != nullptr, "guest memory exported");

  auto find = [&](IM3Function& f, char const* name) {
    if (m3_FindFunction(&f, guest.runtime, name) != m3Err_none) {
      std::printf("export missing: %s\n", name);
      std::exit(1);
    }
  };
  find(guest.initialize, "_initialize");
  find(guest.alloc, "ys_alloc");
  find(guest.reset, "ys_reset");
  find(guest.last_error, "ys_last_error");
  find(guest.last_error_len, "ys_last_error_len");
  find(guest.compress, "ys_compress");
  find(guest.decompress, "ys_decompress");
  find(guest.crush, "ys_crush");
  find(guest.uncrush, "ys_uncrush");

  check(call_scalar(guest.initialize) == 0, "_initialize runs ctors");

  // ゲスト操作ヘルパー: メモリに書き込み→演算→読み出し
  // 注意: memory.grow で wasm3 がメモリを再確保するため、毎回 m3_GetMemory で取得し直す
  auto mem = [&]() -> uint8_t* {
    size_t size = 0;
    auto* p = m3_GetMemory(guest.module, &size, 0);
    if (p != nullptr) {
      guest.memory = p;
      guest.memory_size = size;
    }
    return p;
  };
  auto write_input = [&](std::string const& input) -> uint32_t {
    auto const ptr = static_cast<uint32_t>(call_scalar(guest.alloc, static_cast<uint32_t>(input.size())));
    std::memcpy(mem() + ptr, input.data(), input.size());
    return ptr;
  };
  auto run_op = [&](IM3Function f, uint32_t in, std::string const& input, uint32_t cap) -> std::string {
    auto const out = static_cast<uint32_t>(call_scalar(guest.alloc, cap));
    auto const len = call_scalar(f, in, static_cast<uint32_t>(input.size()), out, cap);
    if (len < 0) {
      return {};
    }
    return std::string{reinterpret_cast<char*>(mem() + out), static_cast<size_t>(len)};
  };
  auto last_error = [&]() -> std::string {
    auto const ptr = static_cast<uint32_t>(call_scalar(guest.last_error));
    auto const len = static_cast<size_t>(call_scalar(guest.last_error_len));
    return std::string{reinterpret_cast<char*>(mem() + ptr), len};
  };

  // --- compress / decompress ラウンドトリップ ---
  // 注意: glz::generic は object を std::map (キーソート) で保持するため
  // 往復後の文字列一致にはキーをソート済みにしておく
  auto const json = std::string{
    R"({"active":true,"name":"yase-json","nested":{"deep":{"value":null}},"pi":3.14159,"tags":["json","wasm3","freestanding"],"version":[1,2,3]})"};
  call_scalar(guest.reset);
  auto const in_ptr = write_input(json);
  auto const compressed = run_op(guest.compress, in_ptr, json, json.size() * 2 + 4096);
  if (compressed.empty()) {
    std::printf("compress error: %s\n", last_error().c_str());
  }
  check(!compressed.empty(), "compress returns data");
  // 小さいユニーク JSON はむしろ膨らむ (compress-json 形式の性質)。
  // 繰り返し構造では圧縮が効くことを確認する。
  {
    auto repetitive = std::string{R"({"active":true,"id":0,"name":"row","tags":[]})"};
    auto rows = std::string{"["};
    for (int i = 0; i < 50; ++i) {
      if (i != 0) {
        rows.push_back(',');
      }
      rows += repetitive;
    }
    rows.push_back(']');
    call_scalar(guest.reset);
    auto const r_ptr = write_input(rows);
    auto const rows_compressed = run_op(guest.compress, r_ptr, rows, rows.size() + 4096);
    check(!rows_compressed.empty() && rows_compressed.size() < rows.size() / 4,
          "compress shrinks repetitive JSON");
  }

  call_scalar(guest.reset);
  auto const c_ptr = write_input(compressed);
  auto const restored = run_op(guest.decompress, c_ptr, compressed, json.size() * 4 + 4096);
  check(restored == json, "decompress(compress(json)) == json");

  // --- crush / uncrush ラウンドトリップ ---
  auto const text = std::string{
    "The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog again and again."};
  call_scalar(guest.reset);
  auto const t_ptr = write_input(text);
  auto const crushed = run_op(guest.crush, t_ptr, text, text.size() * 2 + 256);
  check(!crushed.empty(), "crush returns data");

  call_scalar(guest.reset);
  auto const cr_ptr = write_input(crushed);
  auto const uncrushed = run_op(guest.uncrush, cr_ptr, crushed, text.size() * 4 + 256);
  check(uncrushed == text, "uncrush(crush(text)) == text");

  // --- 日本語 (マルチバイト UTF-8) の crush ラウンドトリップ ---
  auto const jp = std::string{reinterpret_cast<char const*>(u8"yase-json は wasm3 に埋め込める。同じ文字列、同じ文字列。")};
  call_scalar(guest.reset);
  auto const j_ptr = write_input(jp);
  auto const crushed_jp = run_op(guest.crush, j_ptr, jp, jp.size() * 2 + 256);
  call_scalar(guest.reset);
  auto const cj_ptr = write_input(crushed_jp);
  auto const uncrushed_jp = run_op(guest.uncrush, cj_ptr, crushed_jp, jp.size() * 4 + 256);
  check(uncrushed_jp == jp, "uncrush(crush(utf8 japanese)) == input");

  // --- 異常系 ---
  auto const bad = std::string{"{not json"};
  call_scalar(guest.reset);
  auto const b_ptr = write_input(bad);
  auto const bad_result = run_op(guest.compress, b_ptr, bad, 4096);
  check(bad_result.empty(), "compress rejects invalid JSON (-1)");
  check(!last_error().empty(), "ys_last_error has message");

  // --- 数値配列の正確性 (base62 / 小数パス) ---
  // 往復で文字列が変わらない表記のみ使用 (1e5 等は 100000 に正規化される)
  auto const numbers = std::string{R"([0,1,-1,3.14,9007199254740991,0.1])"};
  call_scalar(guest.reset);
  auto const n_ptr = write_input(numbers);
  auto const n_comp = run_op(guest.compress, n_ptr, numbers, 4096);
  call_scalar(guest.reset);
  auto const nc_ptr = write_input(n_comp);
  auto const n_restored = run_op(guest.decompress, nc_ptr, n_comp, 4096);
  check(n_restored == numbers, "number roundtrip exact");

  std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
  return failures == 0 ? 0 : 1;
}
