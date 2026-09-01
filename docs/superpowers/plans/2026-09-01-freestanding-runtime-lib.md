# freestanding-runtime ライブラリ + yase-json 移管 実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** yase-json の freestanding 資産(shim C ヘッダー群 + 汎用 wasm32 ランタイム)を新リポジトリ `freestanding-runtime` に抽出し、yase-json から参照させる。

**Architecture:** shim(16 C ヘッダー + gthr stub)は `include/shim{,-early}/` にそのまま移管し C 互換ガードのみ追加。ランタイムは yase-json `freestanding/guest.cpp` の汎用部(行 34-311)を `runtime/freestanding_runtime.cpp` に抽出し、exported ABI(行 312 以降)だけを guest.cpp に残す。CMake はキャッシュ変数(`FS_SHIM_EARLY_DIR` / `FS_SHIM_DIR` / `FS_RUNTIME_SOURCES` / `FS_SHIM_FILES`)と INTERFACE ターゲットで両方式(custom_command 直叩き / 通常ビルド)をサポートする。

**Tech Stack:** C++23, clang `--target=wasm32-unknown-unknown -nostdlib -fno-exceptions`, CMake 3.24+, Node.js(smoke ランナー)

**Spec:** `docs/superpowers/specs/2026-09-01-freestanding-runtime-lib-design.md`

---

## ファイル構成(全体像)

新規リポジトリ `~/src/freestanding-runtime/`:

| ファイル | 責務 |
|---------|------|
| `include/shim/*.h` (16) | glibc C ヘッダー置換(libstdc++ ヘッダーが引き込む宣言+マクロ)。C/C++ 両対応 |
| `include/shim-early/bits/gthr-default.h` | シングルスレッド gthread stub。include パス最前端に置く |
| `runtime/freestanding_runtime.cpp` | 汎用ランタイム: libcalls / bump allocator / malloc 系 / operator new/delete / `__cxa_guard_*` / fp 分類 / errno / `fs_heap_reset` |
| `cmake/freestanding_runtime.cmake` | `freestanding_runtime_find_libstdcxx_dirs()` — libstdc++ include 2dir プローブ |
| `CMakeLists.txt` | キャッシュ変数 + INTERFACE ターゲット定義 + tests サブディレクトリ |
| `tests/smoke.cpp` / `tests/smoke.mjs` | 最小 -nostdlib wasm のリンク + node 実行検証 |
| `LICENSE` / `README.md` / `.gitignore` | MIT / 使い方 / build 無視 |

yase-json 側の変更:

| ファイル | 変更 |
|---------|------|
| `freestanding/include/` (削除) | shim 2 ディレクトリを削除(kit から供給) |
| `freestanding/CMakeLists.txt` | kit を `add_subdirectory` し、shim パスとランタイムソースを kit 変数から取得 |
| `freestanding/guest.cpp` | 汎用部(行 34-311)を削除。exported ABI のみ。`ys_alloc`/`ys_reset` は kit の malloc/`fs_heap_reset` を呼ぶ |

**境界の前提(実測):** guest.cpp のセクションマーカーは行 35(bump allocator)/70(libralls)/142(libc)/194(fp 分類)/238(C++ ランタイム)/312(exported ABI)。抽出範囲は 34-311 行、残置は 1-33 行(ヘッダーコメント+include)+312 行以降。

---

### Task 1: リポジトリ初期化

**Files:**
- Create: `/home/toge/src/freestanding-runtime/LICENSE`
- Create: `/home/toge/src/freestanding-runtime/README.md`
- Create: `/home/toge/src/freestanding-runtime/.gitignore`

- [ ] **Step 1: ディレクトリ作成と git init**

```bash
mkdir -p /home/toge/src/freestanding-runtime && cd /home/toge/src/freestanding-runtime && git init
```

Expected: `Initialized empty Git repository in /home/toge/src/freestanding-runtime/.git/`

- [ ] **Step 2: .gitignore を書く**

`.gitignore`:

```
build/
```

- [ ] **Step 3: LICENSE(MIT)を書く**

`LICENSE`:

```
MIT License

Copyright (c) 2026 toge

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 4: README.md を書く**

`README.md`:

```markdown
# freestanding-runtime

`wasm32-unknown-unknown (-nostdlib -fno-exceptions)` で libstdc++ ヘッダーを使う
C++ を動かすための shim C ヘッダー群と汎用ランタイム。

wasm32-unknown-unknown には libc が存在しないため、libstdc++ ヘッダー(`<string_view>`
1 つでも glibc 18 ヘッダーを引き込む)をコンパイルするには C ヘッダーの置き換えが必須。
本リポジトリはその shim 層と、どの -nostdlib C++ プログラムでも必要になるランタイム
(libralls / bump allocator / operator new / `__cxa_guard_*` / fp 分類 / errno)を提供する。

## 構成

- `include/shim-early/` — include パスの最前端に置くヘッダー(現在は `bits/gthr-default.h` のみ)
- `include/shim/` — glibc C ヘッダー置換 16 ヘッダー。C と C++ の両方から include 可能
- `runtime/freestanding_runtime.cpp` — 汎用ランタイム。**`-fno-builtin` でコンパイルすること**
  (自分自身が libcalls を自己置換しないため)
- `cmake/freestanding_runtime.cmake` — libstdc++ include 2dir プローブ関数

## 提供する CMake エントリ

`add_subdirectory()` すると以下が使えるようになる(すべて CACHE 変数):

- `FS_SHIM_EARLY_DIR` — shim-early のパス
- `FS_SHIM_DIR` — shim のパス
- `FS_RUNTIME_SOURCES` — 汎用ランタイムのソース一覧
- `FS_SHIM_FILES` — shim ヘッダー一覧(custom command の DEPENDS 用)
- ターゲット `freestanding-runtime::shim`(2 include dir の INTERFACE)と
  `freestanding-runtime::runtime`(ソースを伝搬する INTERFACE ターゲット。
  STATIC にしない理由: ランタイムは消費者のフラグ `-fno-builtin` 等でコンパイルされる必要がある)
- 関数 `freestanding_runtime_find_libstdcxx_dirs(<out_cxx> <out_arch>)`

## include パス順序(重要)

`-nostdinc++` 下での推奨順序:

```
-isystem${FS_SHIM_EARLY_DIR}     # gthr-default.h 等を早期解決
-isystem${glibcxx_cxx_dir}       # libstdc++ C++ ヘッダー(プローブ関数で取得)
-isystem${glibcxx_arch_dir}      # libstdc++ arch bits
-isystem${FS_SHIM_DIR}           # C ヘッダー置換
```

## ランタイムが供給するシンボル

memcpy/memmove/memset/memcmp/bcmp/strlen/strcmp、malloc/calloc/realloc/free、
abort/exit/_Exit、`__cxa_atexit`/`__cxa_finalize`、isinf/isnan/isfinite/isnormal/
signbit/fpclassify 系、operator new/delete 全オーバーロード、`__cxa_guard_*`、
`__cxa_pure_virtual`、`__errno_location`/`__wasm_errno`、`fs_heap_reset()`
(bump ポインタのリセット。`ys_reset` 等が呼ぶ)。

bump allocator の制約: `free()` は no-op。メモリは `fs_heap_reset()` での一括解放のみ。

## ライセンス

MIT。libstdc++ 由来コード(GPL + GCC Runtime Library Exception)は含まない。
```

- [ ] **Step 5: コミット**

```bash
cd /home/toge/src/freestanding-runtime && git add -A && git commit -m "chore: リポジトリ初期化 (MIT, README, gitignore)"
```

Expected: コミット成功

---

### Task 2: shim 移管 + C 互換ガード

**Files:**
- Create: `include/shim/*.h` (16 ファイル、yase-json からコピー)
- Create: `include/shim-early/bits/gthr-default.h`
- Create: `tools/add_c_guard.py`(ガード適用スクリプト)

前提(実測): 16 ヘッダー中 13 個(`assert, ctype, errno, inttypes, libintl, locale, math, stdio, stdlib, string, time, wchar, wctype`)が裸の `extern "C"` を 1 回含み C から include 不可。3 個(`gthr-default.h, features.h, bits/wordsize.h`)は既に C 互換。

- [ ] **Step 1: shim をコピー**

```bash
cd /home/toge/src/freestanding-runtime && mkdir -p include && cp -r /home/toge/src/yase-json/freestanding/include/shim-early include/ && cp -r /home/toge/src/yase-json/freestanding/include/shim include/
```

Expected: `include/shim/` に 16 ヘッダー、`include/shim-early/bits/gthr-default.h` が存在

- [ ] **Step 2: ガード適用スクリプトを書く**

`tools/add_c_guard.py`:

```python
#!/usr/bin/env python3
# shim ヘッダーの extern "C" ブロックを C/C++ 両対応にする。
# extern "C" { ... } を #if defined(__cplusplus) で括る(C では extern "C" を出さない)。
import glob

OPEN_TOK = 'extern "C" {'

changed = 0
for path in sorted(glob.glob('include/shim/*.h')):
    src = open(path).read()
    if OPEN_TOK not in src:
        print(f'skip (no extern "C"): {path}')
        continue
    i = src.index(OPEN_TOK)
    # 先頭の '{' から括弧対応で閉じ '}' を探す(宣言内の struct 定義も釣り合う)
    depth = 0
    j = i + len(OPEN_TOK) - 1
    while True:
        c = src[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    out = (
        src[:i]
        + '#if defined(__cplusplus)\n' + OPEN_TOK + '\n#endif'
        + src[i + len(OPEN_TOK):j]
        + '\n#if defined(__cplusplus)\n}  // extern "C"\n#endif'
        + src[j + 1:]
    )
    open(path, 'w').write(out)
    changed += 1
    print(f'guarded: {path}')
print(f'{changed} files guarded')
```

- [ ] **Step 3: スクリプトを実行**

```bash
cd /home/toge/src/freestanding-runtime && python3 tools/add_c_guard.py
```

Expected: `guarded:` が 13 ファイル(assert, ctype, errno, inttypes, libintl, locale, math, stdio, stdlib, string, time, wchar, wctype)+ `13 files guarded`。`skip (no extern "C")` は出ない(shim/ 配下は全て extern "C" 持ち)。

- [ ] **Step 4: 差分検証(yase-json 原 shim との差がガード行のみであること)**

```bash
diff -ru /home/toge/src/yase-json/freestanding/include/shim /home/toge/src/freestanding-runtime/include/shim | grep '^[+-][^+-]' | grep -v -E "__cplusplus|extern \"C\"|\}  // extern" 
```

Expected: **空出力**(差分はガード行のみ)

- [ ] **Step 5: C コンパイル検証(全 shim ヘッダーが C として通ること)**

```bash
cd /home/toge/src/freestanding-runtime && find include/shim -name '*.h' -exec gcc -std=c11 -fsyntax-only -x c {} \; && echo "C ok"
```

Expected: `C ok`(エラーなし)。もし特定ヘッダーが typedef 足りず失敗する場合は、その型定義を `#ifndef` ガードで追加する(C++ 側の挙動は変えない)。

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/freestanding-runtime && git add -A && git commit -m "feat: yase-json から shim ヘッダー群を移管し C 互換ガードを追加"
```

---

### Task 3: runtime 抽出 + kit スモークテスト

**Files:**
- Create: `runtime/freestanding_runtime.cpp`
- Create: `cmake/freestanding_runtime.cmake`
- Create: `CMakeLists.txt`
- Create: `tests/smoke.cpp`
- Create: `tests/smoke.mjs`
- Create: `tests/CMakeLists.txt`

- [ ] **Step 1: runtime ファイルのヘッダー+include を作成**

`runtime/freestanding_runtime.cpp` に以下を書く(この時点では include まで):

```cpp
// freestanding-runtime: 汎用 wasm32 ランタイム (-nostdlib ビルド用)
//
// libstdc++ / libc が要求するランタイムシンボルのうちターゲット非依存なものを供給する:
//   - libcalls (memcpy/memmove/memset/memcmp/bcmp/strlen/strcmp)
//   - bump allocator + malloc 系 + operator new/delete
//   - __cxa_guard_* / __cxa_atexit / __cxa_pure_virtual / abort
//   - 浮動小数点分類 (isinf/isnan/isfinite/signbit/fpclassify)
//   - errno (__errno_location / __wasm_errno)
//
// yase-json freestanding/guest.cpp の汎用部を抽出したもの。
// -fno-builtin でコンパイルされること (自己置換を防ぐため)。

#include <cstddef>
#include <cstdint>
#include <new>
```

- [ ] **Step 2: guest.cpp の汎用部(行 34-311)をそのまま追記**

```bash
cd /home/toge/src/freestanding-runtime && sed -n '34,311p' /home/toge/src/yase-json/freestanding/guest.cpp >> runtime/freestanding_runtime.cpp
```

- [ ] **Step 3: 移管コメントの参照名を修正**

`runtime/freestanding_runtime.cpp` 内の 1 行を修正:

```
旧: // libcalls (guest.cpp は -fno-builtin でコンパイルされ、自己置換されない)
新: // libcalls (この TU は -fno-builtin でコンパイルされ、自己置換されない)
```

- [ ] **Step 4: errno 定義と fs_heap_reset を追記**

`runtime/freestanding_runtime.cpp` の末尾に追記:

```cpp
// ---------------------------------------------------------------------------
// errno (shim errno.h の宣言に対応する定義)
// ---------------------------------------------------------------------------

extern "C" {
int __wasm_errno = 0;

auto __errno_location(void) -> int* {
  return &__wasm_errno;
}
}

// ---------------------------------------------------------------------------
// bump allocator の一括解放 (yase-json ys_reset 等が呼ぶ)
// ---------------------------------------------------------------------------

extern "C" {
auto fs_heap_reset(void) -> void {
  heap_ptr = reinterpret_cast<uintptr_t>(__heap_base);
  heap_ptr = (heap_ptr + 15) & ~uintptr_t{15};
}
}
```

(`heap_ptr` は移管した bump allocator セクションの static 変数、`__heap_base` はそのセクション内の extern 宣言 — 同一 TU 内なので参照可能。)

- [ ] **Step 5: CMake プローブ関数を作成**

`cmake/freestanding_runtime.cmake`:

```cmake
# libstdc++ ヘッダーの2ディレクトリ (C++ ヘッダー + arch bits) を検出する
# (yase-json freestanding/CMakeLists.txt のプローブ処理を移管)
function(freestanding_runtime_find_libstdcxx_dirs out_cxx_dir out_arch_dir)
  set(_probe_cc "")
  if(CMAKE_CXX_COMPILER MATCHES "clang\\+\\+|g\\+\\+|c\\+\\+")
    set(_probe_cc "${CMAKE_CXX_COMPILER}")
  else()
    find_program(_probe_cc g++)
  endif()
  if(NOT _probe_cc)
    find_program(_probe_cc clang++)
  endif()
  execute_process(
    COMMAND ${_probe_cc} -E -x c++ -v /dev/null
    OUTPUT_QUIET ERROR_VARIABLE _probe_out
    RESULT_VARIABLE _probe_res
  )
  if(NOT _probe_res EQUAL 0)
    message(FATAL_ERROR "failed to probe C++ include dirs with ${_probe_cc}")
  endif()
  set(_marker "#include <...> search starts here:")
  string(FIND "${_probe_out}" "${_marker}" _pos)
  string(FIND "${_probe_out}" "End of search list" _end)
  if(_pos EQUAL -1 OR _end EQUAL -1)
    message(FATAL_ERROR "could not parse include dirs from ${_probe_cc}")
  endif()
  string(LENGTH "${_marker}" _marker_len)
  math(EXPR _pos "${_pos} + ${_marker_len} + 1")
  math(EXPR _len "${_end} - ${_pos}")
  string(SUBSTRING "${_probe_out}" ${_pos} ${_len} _dirs)
  string(REPLACE "\n" ";" _dirs "${_dirs}")
  set(_clean_dirs "")
  foreach(_d IN LISTS _dirs)
    string(STRIP "${_d}" _d)
    if(NOT _d STREQUAL "")
      list(APPEND _clean_dirs "${_d}")
    endif()
  endforeach()
  list(LENGTH _clean_dirs _n_dirs)
  if(_n_dirs LESS 2)
    message(FATAL_ERROR "expected at least 2 C++ include dirs, got: ${_clean_dirs}")
  endif()
  list(GET _clean_dirs 0 _glibcxx_dir)
  list(GET _clean_dirs 1 _glibcxx_arch_dir)
  set(${out_cxx_dir} "${_glibcxx_dir}" PARENT_SCOPE)
  set(${out_arch_dir} "${_glibcxx_arch_dir}" PARENT_SCOPE)
endfunction()
```

- [ ] **Step 6: kit ルート CMakeLists.txt を作成**

`CMakeLists.txt`:

```cmake
# freestanding-runtime — shim + 汎用 wasm32 ランタイム
cmake_minimum_required(VERSION 3.24)
project(freestanding-runtime LANGUAGES CXX)

set(FR_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(FS_SHIM_EARLY_DIR "${FR_DIR}/include/shim-early"
  CACHE PATH "shim-early include dir (gthr-default.h など早期解決ヘッダー)")
set(FS_SHIM_DIR "${FR_DIR}/include/shim"
  CACHE PATH "shim include dir (C ヘッダー置換群)")
set(FS_RUNTIME_SOURCES "${FR_DIR}/runtime/freestanding_runtime.cpp"
  CACHE STRING "汎用ランタイムのソース一覧")
file(GLOB_RECURSE _fr_shim_files "${FR_DIR}/include/*.h")
set(FS_SHIM_FILES "${_fr_shim_files}" CACHE STRING "shim ヘッダー一覧 (custom command の DEPENDS 用)")

list(APPEND CMAKE_MODULE_PATH "${FR_DIR}/cmake")
include(freestanding_runtime)

# shim include dirs (INTERFACE ターゲット。順序: early → shim)
add_library(freestanding-runtime_shim INTERFACE)
target_include_directories(freestanding-runtime_shim SYSTEM INTERFACE
  "${FS_SHIM_EARLY_DIR}"
  "${FS_SHIM_DIR}"
)
add_library(freestanding-runtime::shim ALIAS freestanding-runtime_shim)

# runtime (STATIC にしない: 消費者のフラグ -fno-builtin 等でコンパイルされる必要があるため
# ソースを INTERFACE で伝搬する)
add_library(freestanding-runtime_runtime INTERFACE)
target_sources(freestanding-runtime_runtime INTERFACE ${FS_RUNTIME_SOURCES})
add_library(freestanding-runtime::runtime ALIAS freestanding-runtime_runtime)

option(FR_BUILD_TESTS "Build freestanding-runtime smoke test" ON)
if(FR_BUILD_TESTS)
  add_subdirectory(tests)
endif()
```

- [ ] **Step 7: smoke テスト(ゲスト側)を作成**

`tests/smoke.cpp`:

```cpp
// freestanding-runtime smoke: malloc / memset(libcall) / operator new[] が
// -nostdlib 下でリンク解決・動作することを検証する
#include <stdlib.h>
#include <string.h>

extern "C" __attribute__((export_name("smoke_run")))
auto smoke_run() -> int {
  auto* p = static_cast<unsigned char*>(malloc(16));
  memset(p, 3, 16);
  int sum = 0;
  for (int i = 0; i < 16; ++i) {
    sum += p[i];  // 3 * 16 = 48
  }
  auto* arr = new int[4]{10, 20, 30, 40};
  for (int i = 0; i < 4; ++i) {
    sum += arr[i];  // +100
  }
  delete[] arr;
  free(p);
  return sum;  // 期待値: 148
}
```

- [ ] **Step 8: smoke ランナー(node)を作成**

`tests/smoke.mjs`:

```js
// freestanding-runtime smoke ランナー: wasm を読み込み smoke_run() の戻り値を検証する
import { readFileSync } from 'node:fs';
import process from 'node:process';

const wasmPath = process.argv[2];
if (!wasmPath) {
  console.error('usage: node smoke.mjs <wasm path>');
  process.exit(2);
}
const bytes = readFileSync(wasmPath);
const mod = await WebAssembly.instantiate(bytes, {});
const ret = mod.instance.exports.smoke_run();
if (ret !== 148) {
  console.error(`smoke FAIL: expected 148, got ${ret}`);
  process.exit(1);
}
console.log(`smoke ok: ${ret}`);
```

- [ ] **Step 9: tests/CMakeLists.txt を作成**

`tests/CMakeLists.txt`:

```cmake
# freestanding-runtime smoke: -nostdlib wasm を clang 直叩きでリンクし node で実行する
find_program(FR_CLANG clang++ REQUIRED)
find_program(FR_NODE node)

freestanding_runtime_find_libstdcxx_dirs(FR_GLIBCXX_DIR FR_GLIBCXX_ARCH_DIR)

set(FR_COMPILE_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib" "-ffreestanding" "-fno-exceptions" "-fno-rtti"
  "-fno-builtin" "-Oz" "-std=c++23"
  "-D__STDC_HOSTED__=1"
  "-nostdinc++"
  "-isystem${FS_SHIM_EARLY_DIR}"
  "-isystem${FR_GLIBCXX_DIR}"
  "-isystem${FR_GLIBCXX_ARCH_DIR}"
  "-isystem${FS_SHIM_DIR}"
)
set(FR_LINK_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib"
  "-Wl,--no-entry"
  "-Wl,--export-memory"
)

set(FR_SMOKE_WASM "${CMAKE_CURRENT_BINARY_DIR}/frt-smoke.wasm")
add_custom_command(
  OUTPUT "${FR_SMOKE_WASM}"
  COMMAND ${FR_CLANG} ${FR_COMPILE_FLAGS} ${FR_LINK_FLAGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/smoke.cpp"
          ${FS_RUNTIME_SOURCES}
          -o "${FR_SMOKE_WASM}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/smoke.cpp"
          ${FS_RUNTIME_SOURCES}
          ${FS_SHIM_FILES}
  VERBATIM
)
add_custom_target(frt-smoke ALL DEPENDS "${FR_SMOKE_WASM}")

if(FR_NODE)
  add_custom_command(TARGET frt-smoke POST_BUILD
    COMMAND ${FR_NODE} "${CMAKE_CURRENT_SOURCE_DIR}/smoke.mjs" "${FR_SMOKE_WASM}"
    COMMENT "freestanding-runtime smoke (node + WebAssembly)"
    VERBATIM
  )
else()
  message(STATUS "node not found — smoke run skipped (link check only)")
endif()
```

- [ ] **Step 10: ビルドして smoke が通ることを確認**

```bash
cd /home/toge/src/freestanding-runtime && cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

Expected: `frt-smoke.wasm` が生成され、POST_BUILD で `smoke ok: 148` が出力されて終了コード 0。

- [ ] **Step 11: コミット**

```bash
cd /home/toge/src/freestanding-runtime && git add -A && git commit -m "feat: 汎用ランタイム抽出 + -nostdlib wasm スモークテスト (node 実行)"
```

---

### Task 4: yase-json 移管(shim 削除 + CMakeLists 書換 + guest.cpp スリム化)

**Files:**
- Delete: `/home/toge/src/yase-json/freestanding/include/`(ディレクトリごと)
- Modify: `/home/toge/src/yase-json/freestanding/CMakeLists.txt`(全面書換)
- Modify: `/home/toge/src/yase-json/freestanding/guest.cpp`(行 34-311 削除 + ys_alloc/ys_reset 変更)

⚠️ このタスクの CMakeLists 書換と guest.cpp スリム化は**同一コミットで行うこと**。
順序を分けると(CMake 先 → guest 未変更)、malloc が二重定義でリンクエラー、
(guest 先 → CMake 未変更)、malloc 未解決でリンクエラーになる。

- [ ] **Step 1: yase-json の shim を削除**

```bash
cd /home/toge/src/yase-json && rm -rf freestanding/include
```

- [ ] **Step 2: freestanding/CMakeLists.txt を全面書換**

新しい全文:

```cmake
# FREESTANDING guest (wasm32-unknown-unknown) for wasm3 embedding.
# 親プロジェクトのコンパイラ機構を経由せず clang を直接叩く
# (親が gcc でも、CMAKE_CXX_FLAGS に -march=native 等があっても影響を受けない)。
# shim + 汎用ランタイムは freestanding-runtime リポジトリから供給される。

option(YASE_JSON_FREESTANDING "Build wasm32-unknown-unknown freestanding guest" OFF)

if(NOT YASE_JSON_FREESTANDING)
  return()
endif()

find_program(FS_CLANG clang++ REQUIRED)

# freestanding-runtime (shim + 汎用ランタイム)
set(FREESTANDING_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../freestanding-runtime"
  CACHE PATH "freestanding-runtime リポジトリのパス")
if(NOT EXISTS "${FREESTANDING_RUNTIME_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "freestanding-runtime not found at ${FREESTANDING_RUNTIME_DIR}\n"
    "clone it: git clone https://github.com/toge/freestanding-runtime \"${FREESTANDING_RUNTIME_DIR}\"")
endif()
set(FR_BUILD_TESTS OFF)  # yase-json 側では kit 単体テストをビルドしない
add_subdirectory("${FREESTANDING_RUNTIME_DIR}" "${CMAKE_BINARY_DIR}/freestanding-runtime")

# glaze ヘッダーの場所 (vcpkg インストール済みツリーを優先)
find_path(FS_GLAZE_INCLUDE_DIR glaze/glaze.hpp
  HINTS
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include"
    "${CMAKE_BINARY_DIR}/../build/vcpkg_installed/x64-linux/include"
)
if(NOT FS_GLAZE_INCLUDE_DIR)
  message(FATAL_ERROR "glaze headers not found — build normally (build.sh) first, then build_wasm.sh")
endif()

# libstdc++ ヘッダーの2ディレクトリ (C++ ヘッダー + arch bits) を検出 (kit のプローブ関数)
freestanding_runtime_find_libstdcxx_dirs(_glibcxx_dir _glibcxx_arch_dir)

set(FS_COMPILE_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib" "-ffreestanding" "-fno-exceptions" "-fno-rtti"
  "-fno-builtin" "-Oz" "-std=c++23"
  "-D__STDC_HOSTED__=1"
  "-nostdinc++"
  "-isystem${FS_SHIM_EARLY_DIR}"
  "-isystem${_glibcxx_dir}"
  "-isystem${_glibcxx_arch_dir}"
  "-isystem${FS_SHIM_DIR}"
  "-I${CMAKE_SOURCE_DIR}/include"
  "-I${FS_GLAZE_INCLUDE_DIR}"
)
set(FS_LINK_FLAGS
  "--target=wasm32-unknown-unknown"
  "-nostdlib"
  "-Wl,--no-entry"
  "-Wl,-z,stack-size=1048576"
  "-Wl,--export-memory"
  "-Wl,--strip-all"
)

set(FS_GUEST_WASM "${CMAKE_CURRENT_BINARY_DIR}/yase-json-guest.wasm")

add_custom_command(
  OUTPUT "${FS_GUEST_WASM}"
  COMMAND ${FS_CLANG} ${FS_COMPILE_FLAGS} ${FS_LINK_FLAGS}
          "${CMAKE_CURRENT_SOURCE_DIR}/guest.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/libstdcxx_support.cpp"
          ${FS_RUNTIME_SOURCES}
          -o "${FS_GUEST_WASM}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/guest.cpp"
          "${CMAKE_CURRENT_SOURCE_DIR}/libstdcxx_support.cpp"
          ${FS_RUNTIME_SOURCES}
          ${FS_SHIM_FILES}
  VERBATIM
)
add_custom_target(yase-json-guest ALL DEPENDS "${FS_GUEST_WASM}")

add_subdirectory(smoke)
```

- [ ] **Step 3: guest.cpp をスリム化**

(a) ABI 部(行 312 以降)を一時ファイルに抽出:

```bash
cd /home/toge/src/yase-json && sed -n '312,$p' freestanding/guest.cpp > /tmp/yase_guest_abi.cpp
```

(b) guest.cpp を新ヘッダー+include で書き直す。ファイル先頭を以下で置換(行 1-27 のコメントを差し替え、include は cstddef/cstdint に stdlib.h と extern 宣言を追加):

```cpp
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
```

(c) 抽出した ABI 部を追記:

```bash
cd /home/toge/src/yase-json && cat /tmp/yase_guest_abi.cpp >> freestanding/guest.cpp
```

(d) `ys_alloc` / `ys_reset` を kit 呼び出しに変更。2 箇所を編集:

```
旧:
__attribute__((export_name("ys_alloc")))
auto ys_alloc(uint32_t const size) -> uint32_t {
  auto const p = alloc_bytes(size);
  return p == nullptr ? 0 : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

新:
__attribute__((export_name("ys_alloc")))
auto ys_alloc(uint32_t const size) -> uint32_t {
  auto const p = malloc(size);
  return p == nullptr ? 0 : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}
```

```
旧:
__attribute__((export_name("ys_reset")))
auto ys_reset() -> void {
  heap_ptr = reinterpret_cast<uintptr_t>(__heap_base);
  heap_ptr = (heap_ptr + 15) & ~uintptr_t{15};
}

新:
__attribute__((export_name("ys_reset")))
auto ys_reset() -> void {
  fs_heap_reset();
}
```

- [ ] **Step 4: 古いシンボル参照が残っていないことを確認**

```bash
cd /home/toge/src/yase-json && grep -n "alloc_bytes\|heap_ptr\|__heap_base" freestanding/guest.cpp freestanding/CMakeLists.txt; echo "exit=$?"
```

Expected: マッチなし(`exit=1`、grep の no match)。

- [ ] **Step 5: ビルド + wasm3 スモーク(完了条件)**

```bash
cd /home/toge/src/yase-json && ./build_wasm.sh
```

Expected: ビルド成功。`freestanding/yase-json-guest.wasm` が生成され、POST_BUILD の wasm3 スモーク(roundtrip)が exit 0 で完了。

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/yase-json && git add -A && git commit -m "refactor: freestanding shim+ランタイムを freestanding-runtime リポジトリに移管"
```

---

### Task 5: 最終検証(受け入れ基準)

**Files:** なし(検証のみ)

- [ ] **Step 1: kit をクリーン再ビルド**

```bash
cd /home/toge/src/freestanding-runtime && rm -rf build && cmake -B build -S . && cmake --build build
```

Expected: `smoke ok: 148`

- [ ] **Step 2: yase-json をクリーン再ビルド(guest wasm サイズ確認含む)**

```bash
cd /home/toge/src/yase-json && rm -rf build_wasm && ./build_wasm.sh && ls -la build_wasm/freestanding/yase-json-guest.wasm
```

Expected: wasm3 スモーク exit 0。guest wasm サイズ ~100KB 前後(移管前 104KB と同程度)。

- [ ] **Step 3: 両リポジトリの作業ツリーがクリーンであること**

```bash
cd /home/toge/src/freestanding-runtime && git status --short && cd /home/toge/src/yase-json && git status --short
```

Expected: 両方とも空出力。
