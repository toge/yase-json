# FREESTANDING モード設計 (wasm32-unknown-unknown / wasm3 ゲスト)

日付: 2026-09-01

## 目的

yase-json を `wasm32-unknown-unknown` + `-nostdlib -fno-exceptions -ffreestanding`
でビルドし、wasm3 埋め込みホストのゲストモジュールとして使えるようにする。

## 検証済み前提 (実験結果)

- glaze 6.5.1 は `-fno-exceptions -fno-rtti -ffreestanding` でそのままコンパイル可能
  (`GLZ_THROW_OR_ABORT` が `__cpp_exceptions` 未定義時に `std::abort()` へフォールバック)。
- glibc / libstdc++ ヘッダーは小さな shim ヘッダー群 (~16個) + シングルスレッド
  gthr stub で置換可能 (libstdc++ 16 / glibc 環境で実証済み)。
- yase-json 側の改修は例外 (~20箇所) と `std::stod` (2箇所) / `std::stoull` (1箇所) の排除のみ。

## 決定事項

| 項目 | 決定 |
|------|------|
| エラーAPI | expected コア + 例外 wrapper (公開API非破壊) |
| ゲスト演算 | compress / decompress / crush / uncrush の4種 |
| ビルド統合 | メイン CMakeLists に `YASE_JSON_FREESTANDING` option |
| 検証 | wasm3 をビルドしラウンドトリップスモークテストまで |

## 構成

### 1. コアの例外排除 (ホスト互換維持)

- `detail/error.hpp` (新規): `error` (kind + message) / `result<T>` / `err()` /
  hosted 用 `throw_error()` / `unwrap()`。kind は runtime_error / out_of_range /
  invalid_argument — 既存テストの `REQUIRE_THROWS_AS` を保存するため。
- `detail/compress_json_compat.hpp`: 全関数を例外フリー化。
  - `from_base62` → `result<uint64_t>` (invalid_argument kind を保持)
  - `std::stod` → `parse_number()` (`glz::read_json<double>`、glaze 内蔵パーサー)
  - `std::stoull` → 手書き桁 accumulate (2^53 以下保証済み)
  - `num_to_s` / `encode_number` / `add_value` / `write_compressed` を result 化
- `compress.hpp` / `decompress.hpp` / `crush.hpp`:
  - `try_compress` / `try_decompress` / `try_crush` / `try_uncrush` を常に提供
  - 既存の例外を投げる公開 API (`compress` / `Compressor` / `decompress` /
    `Decompressor::decompress` / `crush` / `uncrush` 各 overload) は
    `#if __cpp_exceptions` でガードし、try_ コアを呼んで throw。
    メッセージ・例外型は現行と同一 (テスト無変更)。
  - `crush.hpp` 内部: `decode_utf8_code_point` / `utf8_to_utf16` / `utf16_to_utf8` を
    result 化。`encoded_uri_length` の throw は到達不能 (入力は常にペア済み UTF-16)
    なので非ペアを1文字として概算する total 関数へ (ponytail コメント付き)。
- `fast_compress.hpp` / `pipeline.hpp`: 未使用の例外 API として維持。
  detail 呼び出し 4箇所を `detail::unwrap()` 経由に差し替え (挙動不変)。
- テスト: `test_compress_decompress.cpp` の `from_base62` 直接呼出し1行のみ
  `.value()` に追従。それ以外のテストは無変更。

### 2. freestanding/ ディレクトリ

- `include/shim-early/bits/gthr-default.h`: シングルスレッド gthread stub
  (pthread 依存遮断。libstdc++ arch dir より先に -isystem する)
- `include/shim/`: glibc C ヘッダー shim 16種
  (features, wordsize, wchar, wctype, locale, ctype, stdlib, string, math, time,
   errno, assert, stdio, inttypes, libintl)。libstdc++ arch dir より後に -isystem
  し `#include_next` を満たす。
- `guest.cpp`: ゲスト1TU。内訳:
  - libcalls (memcpy/memmove/memset/memcmp/bcmp/strlen/strcmp) — `-fno-builtin` で自己置換防止
  - bump allocator (`__heap_base` 起点 + `__builtin_wasm_memory_grow`)、
    malloc/calloc/realloc/free、operator new/delete 全 overload
  - `__cxa_guard_*` / `__cxa_atexit`(no-op) / `__cxa_pure_virtual` / abort / exit
  - isnan / isinf / isfinite / signbit / fpclassify (inline ビット判定)
  - exported ABI: `ys_alloc` / `ys_compress` / `ys_decompress` / `ys_crush` /
    `ys_uncrush` / `ys_last_error` / `ys_last_error_len` / `ys_reset` /
    `_initialize` (`__wasm_call_ctors` 呼出)
  - 関数は `(in_ptr, in_len, out_ptr, out_cap) -> out_len` (-1=エラー)。
    メモリは `--export-memory` されホストが直接読み書き。import 0。

### 3. ビルド

- メイン CMakeLists に `option(YASE_JSON_FREESTANDING)` → `add_subdirectory(freestanding)`。
- `freestanding/CMakeLists.txt` は clang を `find_program` で探し
  `add_custom_command` で直接 `yase-json-guest.wasm` を生成
  (親プロジェクトが gcc でも動くよう CMake コンパイラ機構を経由しない)。
- libstdc++ include 2dir は `g++/clang++ -v` 出力をパースして検出。
- `build_wasm.sh`: vcpkg 構成 → ゲスト wasm ビルド → wasm3 取得/ビルド → スモーク実行。

### 4. 検証

1. 既存テスト全绿 (gcc, hosted — 例外APIが完全互換であること)。
2. `llvm-objdump -x` で import 0 を確認。
3. wasm3 ホストプログラム (`freestanding/smoke/wasm3_smoke.cpp`) で
   compress→decompress / crush→uncrush ラウンドトリップ + 異常系 (-1) を実行。

## 非対象 (YAGNI)

- fast_compress / pipeline 系のゲストABI (必要になったら5行パターンで追加)
- aligned realloc / スレッド対応 / WASI
