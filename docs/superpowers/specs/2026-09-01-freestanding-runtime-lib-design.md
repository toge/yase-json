# freestanding-runtime ライブラリ新規作成 + yase-json 移管 設計

日付: 2026-09-01

## 背景

yase-json の freestanding 対応(`freestanding/`)は、libstdc++ ヘッダーを
`wasm32-unknown-unknown (-nostdlib -fno-exceptions)` でコンパイルするために必要な
shim C ヘッダー群(16 ヘッダー 616 行 + gthr stub 8 行)と、汎用 C++ ランタイム
(libralls / bump allocator / operator new / `__cxa_guard_*` / fp 分類)を実装した。
これらは yase-json 固有ではなく、injamm の NTTP-only freestanding 化(将来)や
wasm4cc 等の一般用途でも必要になる共有部品である。

injamm の現行 freestanding 対応は hosted gcc 上でのマクロ gate 検証のみで、
実ターゲットでは未検証(`throw` 8 箇所残存、shim 未着手)。ただし NTTP-only
(compile-time 完結)パスであれば runtime support 層なしで freestanding 化可能であり、
その場合も shim C ヘッダー層(`<string_view>` 単独で glibc 18 ヘッダーを引き込む実測あり)
は必要になる。したがって shim 層を先にライブラリ化する価値が高い。

## 目的・範囲

- `~/src/freestanding-runtime/` を新規リポジトリとして作成(GitHub: `toge/freestanding-runtime`)。
- yase-json の `freestanding/include/{shim,shim-early}` と `freestanding/guest.cpp` の汎用部を移管し、
  yase-json から参照する形に改修して実証する。
- ターゲットは `wasm32-unknown-unknown` のみ。

## リポジトリ構成

```
freestanding-runtime/
  include/shim-early/bits/gthr-default.h   8 行(そのまま移管)
  include/shim/*.h                        16 ヘッダー 616 行移管 + C 互換ガード追加
  runtime/freestanding_runtime.cpp        guest.cpp 汎用部抽出(~300 行)
  cmake/freestanding_runtime.cmake        libstdc++ include 2dir プローブ関数
  CMakeLists.txt                          変数 + INTERFACE ターゲット定義
  tests/smoke.cpp                         最小 -nostdlib wasm リンクテスト
  tests/CMakeLists.txt
  LICENSE(MIT) / README.md / .gitignore
```

## runtime kit の抽出境界

guest.cpp(422 行)のセクション構成(実測):

| 行 | セクション | 判定 |
|----|-----------|------|
| 35 | bump allocator(memory.grow 拡張, `__heap_base` 起点) | **移管** |
| 70 | libcalls(memcpy/memmove/memset/memcmp/bcmp/strlen/strcmp) | **移管** |
| 142 | libc: malloc/calloc/realloc/free/abort/exit/_Exit/`__cxa_atexit`/`__cxa_finalize` | **移管** |
| 194 | 浮動小数点分類(isinf/isnan/isfinite/isnormal/signbit/fpclassify 系) | **移管** |
| 238 | C++ ランタイム(operator new/delete 全オーバーロード、`__cxa_guard_*`、`__cxa_pure_virtual`) | **移管** |
| 312 | exported ABI(`ys_*`、last_error、`_initialize`) | **yase-json 残置** |

追加で kit が新規に持つもの:

- `__errno_location` / `__wasm_errno` の定義(shim errno.h が宣言するが現状どこにも定義がない。static 変数 1 個 + 関数 1 個)
- `fs_reset()` 相当: bump allocator の一括解放(yase-json の `ys_reset` が呼ぶ)

yase-json 残置:

- exported ABI。`ys_alloc`/`ys_reset` は kit の malloc / reset を呼ぶ薄ラッパーに変更。
- `libstdcxx_support.cpp`(421 行、`_Rb_tree_*` / `_Prime_rehash_policy` /
  `basic_string::_M_replace_cold` / `__throw_*`)。libstdc++ 由来コード(GPL+RTLE)を
  kit に含めないため。injamm が engine パスの freestanding 対応を始めた時点で再検討。

## CMake 統合

kit 側が提供するもの:

- 変数: `FS_SHIM_EARLY_DIR` / `FS_SHIM_DIR` / `FS_RUNTIME_SOURCES`
  (yase-json の `add_custom_command` で clang を直接叩くビルド方式用)
- INTERFACE ターゲット: `freestanding-runtime::shim`(shim-early + shim の 2 include dir を
  順序維持で SYSTEM include に追加)+ `freestanding-runtime::runtime`
  (wasm4cc 等の通常ビルド用)。runtime は STATIC にせず INTERFACE ターゲットとして
  `FS_RUNTIME_SOURCES` を伝搬する。ランタイムは消費者のフラグ(`-fno-builtin` 等)で
  コンパイルされる必要があるため(STATIC 化すると libcalls が自己置換されて壊れる)。
- kit 自身の smoke テストは yase-json と同様に clang を `find_program` で直接叩く
  (親プロジェクトの gcc 影響を受けないようにする)。
- CMake 関数: `freestanding_runtime_find_libstdcxx_dirs(<out_cxx> <out_arch>)`
  — yase-json 現在の `g++ -E -v` 出力パース(~45 行)を移管

yase-json 側の改修:

- `freestanding/CMakeLists.txt` を `add_subdirectory(${FREESTANDING_RUNTIME_DIR})` +
  変数参照に書き換え。
- glaze 探出し(`FS_GLAZE_INCLUDE_DIR`)は yase-json に残す。
- include path 順序は現行どおり: `shim-early` → libstdc++ 2dir → `shim` → yase-json/glaze。

## C 互換化

shim 全ヘッダーに `#ifdef __cplusplus extern "C" { #endif` ガードを追加する。
現状は裸の `extern "C"` で、C から include できない。wasm4cc 等の C 消費者に備える。
行数への影響は各ヘッダー 2〜4 行程度。

## テスト・受け入れ基準

- kit 単体テスト: malloc + memcpy + operator new を使用する最小 guest を
  `-nostdlib` でリンクし wasm が生成されること(シンボル解決の検証。実行検証は
  yase-json の wasm3 スモークが担うため kit 内では行わない)。
- yase-json 移管後: `build_wasm.sh` が現行同等の guest wasm(~104KB)を生成し、
  wasm3 ラウンドトリップスモークが通ること = **完了条件**。
- shim 差分検証: 移管前後で `diff -r` が一致(C 互換ガード追加のみ許容)。

## ライセンス

MIT。libstdc++ 由来コード(GPL + GCC Runtime Library Exception)を kit に含めない。

## 非対象(YAGNI)

- `libstdcxx_support.cpp` の移管(injamm が engine パス対応を始めた時点で再検討)
- injamm の throw 除去・gate 設計・実ターゲットテスト(別プロジェクト)
- multi-target 対応(arm/riscv bare-metal)
- vcpkg ポート化
- wasm3 の kit 内蔵
- shim セットの再設計(実績セットをそのまま移管。early/late 2 ディレクトリ構成も維持)
