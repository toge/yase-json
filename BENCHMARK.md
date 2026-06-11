# ベンチマーク

`yase-json` の圧縮・伸長パフォーマンスを測定するベンチマークです。

## 実行方法

```bash
cmake -B build -S . -DBUILD_BENCH=ON
cmake --build build
./build/test/benchmark
```

`BUILD_BENCH=ON` を指定するとベンチマーク用実行ファイルがビルドされます。
（通常のテストには影響しません。）

## 測定項目

各サイズのJSONデータに対して以下を計測します:

| 項目 | 説明 |
|---|---|
| Original | 圧縮前のJSONサイズ |
| Compressed | `Compressor` による圧縮後のサイズ |
| Ratio | 圧縮率 (Compressed / Original) |
| Compress | 圧縮にかかった時間 (平均) |
| Decompress | 伸長にかかった時間 (平均) |

## 計測環境

- **CPU**: Intel(R) Core(TM) i7-10750H @ 2.60GHz
- **RAM**: 32GB
- **OS**: Ubuntu 24.04
- **Compiler**: GCC 14.2
- **C++ Standard**: C++23
- **Build**: `-O3 -march=native`

## 計測結果 (2026-06-11)

| Size  | Original   | Compressed | Ratio  | Compress  | Decompress | Iter |
|-------|------------|------------|--------|-----------|------------|------|
| small | 508 B      | 483 B      | 95.1%  | 0.011 ms  | 0.007 ms   | 1000 |
| medium| 94.14 KB   | 38.00 KB   | 40.4%  | 1.427 ms  | 1.331 ms   | 100  |
| large | 542.2 KB   | 345.1 KB   | 63.7%  | 15.401 ms | 13.783 ms  | 10   |

### テストデータの性質

| サイズ | データ構造 | 特徴 |
|--------|-----------|------|
| small (508B) | 単一のネストされたオブジェクト | 混合型, 短い文字列, 小配列 |
| medium (94KB) | 500要素の配列 | 共通スキーマを持つオブジェクトの繰り返し, タイムスタンプ付き |
| large (542KB) | 3000要素の配列 (ネスト) | 深いネスト, 繰り返しの多い値, 浮動小数点数 |

## 補足

- Compressor/Decompressor は1回目の呼び出しでキャッシュを構築し、
  2回目以降の結果を計測しています（初回は warm-up として扱う）。
- カッコ内の Iter は計測に使用したイテレーション回数です。
- 1000回未満のデータはテーブル総時間が適切な範囲に収まるよう調整しています。
