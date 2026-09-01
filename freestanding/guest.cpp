// yase-json FREESTANDING guest for wasm32-unknown-unknown (wasm3 埋め込み用)
//
// -nostdlib でリンクするため、この TU が libstdc++ / libc が要求する
// ランタイムシンボルの供給元になる:
//   - libcalls (memcpy/memmove/memset/memcmp/bcmp/strlen)
//   - bump allocator + malloc 系 + operator new/delete
//   - __cxa_guard_* / __cxa_atexit / __cxa_pure_virtual / abort
//   - 浮動小数点分類 (isinf/isnan/isfinite/signbit/fpclassify)
//
// exported ABI (全て wasm3 ホストから呼び出し可能。メモリは --export-memory):
//   _initialize()               : 静的初期化子 (__wasm_call_ctors) を実行
//   ys_alloc(len)               : ゲストメモリから len バイト確保 (bump)
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

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"

// ---------------------------------------------------------------------------
// bump allocator (linear memory, __heap_base 起点, memory.grow で拡張)
// ---------------------------------------------------------------------------

extern "C" unsigned char __heap_base[];
static uintptr_t heap_ptr = 0;
static uintptr_t heap_end = 0;

static auto init_heap() -> void {
  if (heap_ptr == 0) {
    heap_ptr = reinterpret_cast<uintptr_t>(__heap_base);
    heap_ptr = (heap_ptr + 15) & ~uintptr_t{15};
    heap_end = reinterpret_cast<uintptr_t>(
      __builtin_wasm_memory_size(0) * 65536);
  }
}

extern "C" auto alloc_bytes(size_t size) -> void* {
  init_heap();
  size = (size + 15) & ~size_t{15};
  if (size == 0) {
    size = 16;
  }
  if (heap_ptr + size > heap_end) {
    auto const need_pages = ((heap_ptr + size) - heap_end + 65535) / 65536;
    if (__builtin_wasm_memory_grow(0, need_pages) == static_cast<uint32_t>(-1)) {
      return nullptr;
    }
    heap_end = reinterpret_cast<uintptr_t>(__builtin_wasm_memory_size(0)) * 65536;
  }
  auto const p = heap_ptr;
  heap_ptr += size;
  return reinterpret_cast<void*>(p);
}

// ---------------------------------------------------------------------------
// libcalls (guest.cpp は -fno-builtin でコンパイルされ、自己置換されない)
// ---------------------------------------------------------------------------

extern "C" {
auto memcpy(void* dst, void const* src, size_t n) -> void* {
  auto* d = static_cast<unsigned char*>(dst);
  auto const* s = static_cast<unsigned char const*>(src);
  while (n-- > 0) {
    *d++ = *s++;
  }
  return dst;
}

auto memmove(void* dst, void const* src, size_t n) -> void* {
  auto* d = static_cast<unsigned char*>(dst);
  auto const* s = static_cast<unsigned char const*>(src);
  if (d < s) {
    while (n-- > 0) {
      *d++ = *s++;
    }
  } else if (d > s) {
    d += n;
    s += n;
    while (n-- > 0) {
      *--d = *--s;
    }
  }
  return dst;
}

auto memset(void* dst, int c, size_t n) -> void* {
  auto* d = static_cast<unsigned char*>(dst);
  while (n-- > 0) {
    *d++ = static_cast<unsigned char>(c);
  }
  return dst;
}

auto memcmp(void const* a, void const* b, size_t n) -> int {
  auto const* p = static_cast<unsigned char const*>(a);
  auto const* q = static_cast<unsigned char const*>(b);
  while (n-- > 0) {
    if (*p != *q) {
      return *p < *q ? -1 : 1;
    }
    ++p;
    ++q;
  }
  return 0;
}

auto bcmp(void const* a, void const* b, size_t n) -> int {
  return memcmp(a, b, n) != 0;
}

auto strlen(char const* s) -> size_t {
  auto const* p = s;
  while (*p != '\0') {
    ++p;
  }
  return static_cast<size_t>(p - s);
}

auto strcmp(char const* a, char const* b) -> int {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

// ---------------------------------------------------------------------------
// libc: allocator 系 + プロセス系 (freestanding では trap / no-op)
// ---------------------------------------------------------------------------

auto malloc(size_t size) -> void* {
  return alloc_bytes(size);
}

auto calloc(size_t count, size_t size) -> void* {
  auto* p = alloc_bytes(count * size);
  if (p != nullptr) {
    memset(p, 0, count * size);
  }
  return p;
}

auto realloc(void* old_ptr, size_t size) -> void* {
  if (old_ptr == nullptr) {
    return alloc_bytes(size);
  }
  // ponytail: 旧サイズは追跡していないため常に新規確保+コピー。
  // 頻繁な realloc がボトルネックになったらサイズヘッダーを追加する。
  auto* p = alloc_bytes(size);
  if (p != nullptr) {
    memcpy(p, old_ptr, size < 64 ? size : 64);
  }
  return p;
}

auto free(void*) -> void {
  // ponytail: bump allocator — ys_reset() での一括解放のみ対応
}

auto abort() -> void {
  __builtin_trap();
}

auto exit(int) -> void {
  __builtin_trap();
}

auto _Exit(int) -> void {
  __builtin_trap();
}

int __cxa_atexit(void (*)(void*), void*, void*) {
  return 0;
}

void __cxa_finalize(void*) {
}

// ---------------------------------------------------------------------------
// 浮動小数点分類 (wasm ビルトインは inline 展開されるため libcall にならない)
// ---------------------------------------------------------------------------

auto fabs(double) -> double;
auto fabsf(float) -> float;

auto isinf(double const x) -> int {
  return __builtin_fabs(x) == __builtin_inf();
}

auto isinff(float const x) -> int {
  return __builtin_fabsf(x) == __builtin_inff();
}

auto isnan(double const x) -> int {
  return x != x;
}

auto isnanf(float const x) -> int {
  return x != x;
}

auto isfinite(double const x) -> int {
  return !(x != x) && !isinf(x);
}

auto isfinitef(float const x) -> int {
  return !(x != x) && !isinff(x);
}

auto isnormal(double const x) -> int {
  return __builtin_fpclassify(0, 0, 1, 0, 0, x);
}

auto signbit(double const x) -> int {
  return __builtin_signbit(x);
}

auto fpclassify(double const x) -> int {
  return __builtin_fpclassify(0, 1, 4, 3, 2, x);
}
} // extern "C"

// ---------------------------------------------------------------------------
// C++ ランタイム
// ---------------------------------------------------------------------------

auto operator new(size_t const size) -> void* {
  auto* p = alloc_bytes(size);
  if (p == nullptr) {
    abort();
  }
  return p;
}

auto operator new(size_t const size, std::nothrow_t const&) noexcept -> void* {
  return alloc_bytes(size);
}

auto operator new[](size_t const size) -> void* {
  return operator new(size);
}

auto operator new[](size_t const size, std::nothrow_t const&) noexcept -> void* {
  return alloc_bytes(size);
}

auto operator delete(void*) noexcept -> void {
}

auto operator delete(void*, size_t) noexcept -> void {
}

auto operator delete[](void*) noexcept -> void {
}

auto operator delete[](void*, size_t) noexcept -> void {
}

// aligned overloads (デフォルトアラインメント超過型用)
auto operator new(size_t const size, std::align_val_t const) -> void* {
  return operator new(size);
}

auto operator new[](size_t const size, std::align_val_t const) -> void* {
  return operator new(size);
}

auto operator delete(void*, std::align_val_t const) noexcept -> void {
}

auto operator delete(void*, size_t, std::align_val_t const) noexcept -> void {
}

auto operator delete[](void*, std::align_val_t const) noexcept -> void {
}

auto operator delete[](void*, size_t, std::align_val_t const) noexcept -> void {
}

extern "C" {
int __cxa_guard_acquire(long long* guard) {
  return *guard == 0;
}

void __cxa_guard_release(long long* guard) {
  *guard = 1;
}

void __cxa_guard_abort(long long*) {
}

[[noreturn]] void __cxa_pure_virtual() {
  abort();
}
}

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
  auto const p = alloc_bytes(size);
  return p == nullptr ? 0 : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

__attribute__((export_name("ys_reset")))
auto ys_reset() -> void {
  heap_ptr = reinterpret_cast<uintptr_t>(__heap_base);
  heap_ptr = (heap_ptr + 15) & ~uintptr_t{15};
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
