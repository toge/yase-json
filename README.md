# yase-json

`yase-json` は、[glaze](https://github.com/stephenberry/glaze)をベースにした、C++ 用の軽量なJSON圧縮・ミニファイライブラリです。
構造的な冗長性を排除するコンプレッサーと、文字列レベルでの圧縮を行うJSONCrushを提供します。

## 特徴

- **構造的圧縮 (Compressor/Decompressor)**:
  - [`beenotung/compress-json`](https://github.com/beenotung/compress-json) と互換な values/root 形式で JSON を圧縮します。
  - object / array / string / number / boolean / null の符号化規則は compress-json に合わせています。
  - 配列内の重複オブジェクトなどが非常に多いデータセットで高い圧縮率を発揮します。
- **文字列圧縮 (JSONCrush)**:
  - [JSONCrush](https://github.com/KilledByAPixel/JSONCrush)にインスパイアされたアルゴリズムで、頻出する部分文字列を未使用の文字に置き換えることで、文字列全体のサイズを削減します。
- **高速な処理**:
  - `glaze` の `glz::generic` を活用し、効率的な JSON 操作を実現しています。
  - Base62のデコードには高速なルックアップテーブルを使用しています。
  - **選択的フィールド圧縮**: 特定のフィールドのみを対象にした高速な圧縮が可能です（静的・動的選択をサポート）。

## 要件

- **C++23 以上** (C++26推奨)
- **依存ライブラリ**:
  - [glaze](https://github.com/stephenberry/glaze)
  - [Catch2](https://github.com/catchorg/Catch2) (テスト用)

## インストール

`vcpkg` を使用して依存関係を管理することを推奨します。

```bash
# vcpkg.json があるディレクトリで以下を実行してください
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## WASI モード (wasm32-wasip1)

本リポジトリの WASI 対応は `wasm32-wasip1` + `wasi-sdk` sysroot を想定し、`YASE_JSON_WASI_MINIMAL` で例外なし (no exceptions) モードを提供します。
hosted ヘッダが必要なため真の freestanding (`wasm32-unknown-unknown` + `-nostdlib`) は非対応です。

```bash
# 手動で WASI minimal を有効化 (ホスト検証や -fno-exceptions ビルド):
g++ -std=c++23 -I include -DYASE_JSON_WASI_MINIMAL -fno-exceptions ...
cmake -DENABLE_WASI_MINIMAL=ON ...
clang++ --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot -DYASE_JSON_WASI_MINIMAL -fno-exceptions ...
```

`wasm32-wasip2` 環境の対応は現時点では未検証です。wasi-sdk が正式対応したら検証予定です。

### 有効化方法

| 方法             | 手順                                                                               |
| ---------------- | ---------------------------------------------------------------------------------- |
| コンパイラフラグ | `-DYASE_JSON_WASI_MINIMAL` を付与                                                  |
| CMake            | `-DENABLE_WASI_MINIMAL=ON` (`CMakeLists.txt:7`, `include/yase-json/config.hpp:11`) |

`wasm32-wasip1` / `wasm32-emscripten` は WASI/hosted とみなすため自動では有効にならず、WASI 上で WASI_MINIMAL サブセットを検証したい場合は明示的に `-DYASE_JSON_WASI_MINIMAL` を付与してください。

### 例外なしモードの挙動

`YASE_JSON_WASI_MINIMAL` 定義時、ライブラリ内の全ての例外送出は `YASE_JSON_THROW` マクロ（`include/yase-json/config.hpp`）経由で `std::abort()` に置き換わります。`<stdexcept>` は include されず `-fno-exceptions` でビルドできます。`wasip1` では `iostream` が WASI 経由で利用可能なため無効化しません。

| 状況 | hosted | WASI_MINIMAL |
| --- | --- | --- |
| コンパイル時（不正入力） | コンパイルエラー | コンパイルエラー（変わらず） |
| 実行時（不正JSON、`at`外し等） | `std::runtime_error` 等を throw | `std::abort()` |

`Compressor` / `Decompressor` / `crush` / `uncrush` / `Fast*` / `pipeline::*` を含む全 API は `WASI_MINIMAL` でも利用可能ですが、失敗時は throw ではなく abort します。`try_*` 系（`std::expected` 返し）はどちらのモードでも同じく利用でき、WASI 上でエラーをハンドルしたい場合は `try_*` の使用を推奨します。

## 使い方

### 構造的圧縮

`yase_json::Compressor` を使用してJSONデータを圧縮、`yase_json::Decompressor` で元に戻すことができます。
圧縮結果は [compress-json](https://github.com/beenotung/compress-json) の出力形式と互換です。

```cpp
#include <iostream>
#include <string>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"

int main() {
  std::string json_str = R"([{"name":"item","val":1},{"name":"item","val":1}])";

  yase_json::Compressor compressor;
  std::string compressed = compressor.compress(json_str);
  std::cout << "Compressed: " << compressed << '\n';

  yase_json::Decompressor decompressor;
  std::string decompressed = decompressor.decompress(compressed);
  std::cout << "Decompressed: " << decompressed << '\n';

  return 0;
}
```

### 高速な構造的圧縮 (FastCompressor)

`yase_json::FastCompressor` を使用して、特定のフィールドのみを対象にした高速な圧縮が可能です。

#### 動的選択 (実行時)
```cpp
yase_json::FastCompressor compressor;
compressor.set_fields({"name", "age"}); // 圧縮対象フィールドを指定
std::string compressed = compressor.compress(json_str);
```

#### 静的選択 (コンパイル時)
```cpp
// コンパイル時にフィールドを固定して最適化
yase_json::StaticFastCompressor<"name", "age"> compressor;
std::string compressed = compressor.compress(json_str);
```

### JSONCrush (文字列レベルの圧縮)

文字列としてJSONをさらに圧縮したい場合に有効です。

`yase_json::crush` / `yase_json::uncrush` は
[KilledByAPixel/JSONCrush](https://github.com/KilledByAPixel/JSONCrush)
の `JSONCrush.crush()` / `JSONCrush.uncrush()` が返す**生の文字列**と互換です。

圧縮結果は [JSONCrush](https://github.com/KilledByAPixel/JSONCrush) の出力形式と互換です。

URLクエリなどで利用する場合は、JavaScript版と同様に圧縮後の文字列を別途
`encodeURIComponent` 相当でエンコードしてください。

```cpp
#include <iostream>
#include "yase-json/crush.hpp"

int main() {
  std::string input = R"({"a":"value", "b":"value", "c":"value"})";

  auto const crushed = yase_json::crush(input);
  std::cout << "Crushed: " << crushed << '\n';

  auto const uncrushed = yase_json::uncrush(crushed);
  std::cout << "Uncrushed: " << uncrushed << '\n';

  return 0;
}
```

## 制約と注意

- **自由関数API**: `yase_json::compress()` / `yase_json::decompress()` は `Compressor` / `Decompressor` クラスと等価な自由関数です。状態を持たない用途ではこちらを推奨します。
- **深さ上限**: 圧縮・展開ともにネスト深さ512で打ち切ります。敵意ある入力によるスタック溢れを防ぐためです。超過時は `std::runtime_error` を送出します。
- **U+0001の除去**: `crush()` は JSONCrush 仕様に準拠し、入力中の `U+0001` を無言で除去します。
- **-0の正規化**: `-0.0` は `0` に正規化されます(意味的には等価)。
- **スレッド安全性**: `FastCompressor` / `FastCrusher` / `FastCompressCrusher` は内部キャッシュを持つためスレッドセーフではありません。スレッドごとにインスタンスを分けてください。
- **FastCrusherの辞書衝突**: 辞書の置換文字が後続入力に自然に含まれる場合、該当エントリの適用をスキップして復元を保証します(圧縮率はわずかに低下)。
- **選択的フィールド圧縮**: `set_fields` / `StaticFastCompressor` はトップレベルオブジェクトのフィールドのみを対象とし、指定外のフィールドは出力から除外されます(情報の損失を伴います)。

## ベンチマーク

圧縮率・パフォーマンスの詳細は [BENCHMARK.md](BENCHMARK.md) を参照してください。

## ライセンス

[MIT License](LICENSE)
