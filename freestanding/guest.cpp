// yase-json FREESTANDING guest for wasm32-unknown-unknown (wasm3 埋め込み用)
//
// ランタイムの供給 (libcalls / bump allocator / operator new / __cxa_guard_* /
// 浮動小数点分類 / errno) は freestanding-runtime リポジトリに移管済み。
// この TU は yase-json 固有の exported ABI のみを供給する。
//
// exported ABI (全て wasm3 ホストから呼び出し可能。メモリは --export-memory):
//   _initialize()               : 静的初期化子 (__wasm_call_ctors) を実行
//   ys_alloc(len)               : ゲストメモリから len バイト確保 (kit の malloc)
//   ys_compress(in, in_len, out, out_cap) -> 圧縮後長さ or -1
//   ys_decompress(in, in_len, out, out_cap) -> 復元後長さ or -1
//   ys_crush(in, in_len, out, out_cap)      -> crush 後長さ or -1
//   ys_uncrush(in, in_len, out, out_cap)    -> uncrush 後長さ or -1
//   ys_last_error()             : 最後のエラーメッセージのアドレス
//   ys_last_error_len()         : その長さ
//   ys_reset()                  : bump ポインタをリセット (全解放)
//
// ホスト側の使い方:
//   1. _initialize() を1回呼ぶ
//   2. ys_alloc(len) → ゲストメモリに入力 JSON を書く
//   3. ys_alloc(cap) した出力領域ポインタで ys_compress(...) を呼ぶ
//   4. 戻り値 (長さ) でゲストメモリから出力を読む (-1 なら ys_last_error)
//   5. 次の処理の前に ys_reset()

#include <cstddef>
#include <cstdint>
#include <stdlib.h>

extern "C" {
auto fs_heap_reset(void) -> void;
}

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"
// ---------------------------------------------------------------------------
// exported ABI
// ---------------------------------------------------------------------------

namespace {

char last_error[256];
uint32_t last_error_len = 0;

template <typename E>
auto store_error(E const& e) -> void {
  auto const message = [&]() -> std::string_view {
    if constexpr (requires { e.message; }) {
      return e.message;
    } else {
      return e;
    }
  }();
  last_error_len = static_cast<uint32_t>(
    message.size() < sizeof(last_error) - 1 ? message.size() : sizeof(last_error) - 1);
  memcpy(last_error, message.data(), last_error_len);
}

// try_* コアを実行し、結果を出力バッファへ書き込む共通ランナー
template <typename F>
auto run(uint32_t const in_ptr, uint32_t const in_len,
         uint32_t const out_ptr, uint32_t const out_cap, F&& f) -> int32_t {
  if (out_ptr == 0 || out_cap == 0) {
    store_error(std::string_view{"No output buffer"});
    return -1;
  }
  auto const input = std::string_view{
    reinterpret_cast<char const*>(in_ptr), static_cast<size_t>(in_len)};
  auto result = f(input);
  if (!result) {
    store_error(result.error());
    return -1;
  }
  if (result->size() > out_cap) {
    store_error(std::string_view{"Output buffer too small"});
    return -1;
  }
  memcpy(reinterpret_cast<void*>(out_ptr), result->data(), result->size());
  return static_cast<int32_t>(result->size());
}

} // namespace

extern "C" {

__attribute__((export_name("_initialize")))
auto _initialize() -> void {
  extern void __wasm_call_ctors();
  __wasm_call_ctors();
}

__attribute__((export_name("ys_alloc")))
auto ys_alloc(uint32_t const size) -> uint32_t {
  auto const p = malloc(size);
  return p == nullptr ? 0 : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

__attribute__((export_name("ys_reset")))
auto ys_reset() -> void {
  fs_heap_reset();
}

__attribute__((export_name("ys_last_error")))
auto ys_last_error() -> uint32_t {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last_error));
}

__attribute__((export_name("ys_last_error_len")))
auto ys_last_error_len() -> uint32_t {
  return last_error_len;
}

__attribute__((export_name("ys_compress")))
auto ys_compress(uint32_t const in, uint32_t const in_len,
                 uint32_t const out, uint32_t const out_cap) -> int32_t {
  return run(in, in_len, out, out_cap, [](std::string_view const s) {
    return yase_json::try_compress(s);
  });
}

__attribute__((export_name("ys_decompress")))
auto ys_decompress(uint32_t const in, uint32_t const in_len,
                   uint32_t const out, uint32_t const out_cap) -> int32_t {
  return run(in, in_len, out, out_cap, [](std::string_view const s) {
    return yase_json::try_decompress(s);
  });
}

__attribute__((export_name("ys_crush")))
auto ys_crush(uint32_t const in, uint32_t const in_len,
              uint32_t const out, uint32_t const out_cap) -> int32_t {
  return run(in, in_len, out, out_cap, [](std::string_view const s) {
    return yase_json::try_crush(s);
  });
}

__attribute__((export_name("ys_uncrush")))
auto ys_uncrush(uint32_t const in, uint32_t const in_len,
                uint32_t const out, uint32_t const out_cap) -> int32_t {
  return run(in, in_len, out, out_cap, [](std::string_view const s) {
    return yase_json::try_uncrush(s);
  });
}

} // extern "C"
