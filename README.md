# yase-json

`yase-json` は、[glaze](https://github.com/stephenberry/glaze)をベースにした、C++ 用の軽量なJSON圧縮・ミニファイライブラリです。
構造的な冗長性を排除するコンプレッサーと、文字列レベルでの圧縮を行うJSONCrushを提供します。

## 特徴

- **構造的圧縮 (Compressor/Decompressor)**:
  - 重複する値をプールし、Base62 エンコードされたインデックスで参照することで、JSON の構造的な冗長性を削減します。
  - 配列内の重複オブジェクトなどが非常に多いデータセットで高い圧縮率を発揮します。
- **文字列圧縮 (JSONCrush)**:
  - [JS-Crush](https://iteral.com/jscrush/) にインスパイアされたアルゴリズムで、頻出する部分文字列を未使用の文字に置き換えることで、文字列全体のサイズを削減します。
- **高速な処理**:
  - `glaze` の `glz::generic` を活用し、効率的な JSON 操作を実現しています。
  - Base62のデコードには高速なルックアップテーブルを使用しています。

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

```cpp
#include <iostream>
#include <string>

#include "yase-json/compress.hpp"
#include "yase-json/decompress.hpp"

int main() {
  std::string json_str = R"([{"name":"item","val":1},{"name":"item","val":1}])";

  glz::generic original;
  glz::read_json(original, json_str);

  yase_json::Compressor compressor;
  auto compressed = compressor.compress(original);

  std::string compressed_out;
  glz::write_json(compressed, compressed_out);
  std::cout << "Compressed: " << compressed_out << std::endl;

  yase_json::Decompressor decompressor;
  auto decompressed = decompressor.decompress(compressed);

  std::string decompressed_out;
  glz::write_json(decompressed, decompressed_out);
  std::cout << "Decompressed: " << decompressed_out << std::endl;

  return 0;
}
```

### JSONCrush (文字列レベルの圧縮)

文字列として JSON をさらに圧縮したい場合に有効です。

```cpp
#include <iostream>
#include "yase-json/crush.hpp"

int main() {
  std::string input = R"({"a":"value", "b":"value", "c":"value"})";

  yase_json::JSONCrush crusher;
  auto const crushed = crusher.crush(input);
  std::cout << "Crushed: " << crushed << std::endl;

  auto const uncrushed = crusher.uncrush(crushed);
  std::cout << "Uncrushed: " << uncrushed << std::endl;

  return 0;
}
```

## ライセンス

[MIT License](LICENSE)
