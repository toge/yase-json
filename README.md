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

## ベンチマーク

圧縮率・パフォーマンスの詳細は [BENCHMARK.md](BENCHMARK.md) を参照してください。

## ライセンス

[MIT License](LICENSE)
