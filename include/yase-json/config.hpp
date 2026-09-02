#pragma once

/**
 * @file yase_json/config.hpp
 * @brief ビルドモード設定。
 *
 * YASE_JSON_WASI_MINIMAL が定義されると、ライブラリ内の全ての例外送出
 * (YASE_JSON_THROW) が std::abort() に置き換わり、-fno-exceptions でも
 * ビルドできる「例外なしモード」になる。例外を送出する自由関数 / クラス
 * (compress / decompress / crush / uncrush 等の throw 版, Compressor,
 * Decompressor, Fast* 系) はこのモードでは提供されない。try_* 系
 * (std::expected 返し) のみが利用可能になる。
 * wasm32-wasip1 / wasm32-emscripten は WASI/hosted とみなすため自動では
 * 有効にならず、WASI 上で最小構成を検証する場合は手動で
 * `-DYASE_JSON_WASI_MINIMAL` を指定する。本ライブラリの WASI 対応は
 * wasi-sdk sysroot を用いた wasm32-wasip1 でのビルドを想定
 * (wasm3, wasmedge 等の WASI ランタイムで実行可能)。`<iostream>` は
 * wasip1/wasip2 では WASI 経由で利用可能なため無効化しない。
 *
 * 例: clang++ --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot
 *       -fno-exceptions -DYASE_JSON_WASI_MINIMAL=1 -I include -c src.cpp -o src.o
 */
#if !defined(YASE_JSON_WASI_MINIMAL) && defined(__wasm__) && !defined(__wasi__) && !defined(__EMSCRIPTEN__)
#  define YASE_JSON_WASI_MINIMAL 1
#endif

/**
 * @brief 例外送出の統一マクロ。
 *
 * hosted (既定) では `throw expr` に展開する。YASE_JSON_WASI_MINIMAL 定義時は
 * expr を評価せず `detail::fail()` を呼ぶ。fail() は非 constexpr のため
 * コンパイル時評価では従来どおりコンパイルエラーになり、実行時は std::abort() する。
 * これにより -fno-exceptions でもライブラリ全体がビルドできる。
 */
#ifndef YASE_JSON_WASI_MINIMAL
#  include <stdexcept>
#  define YASE_JSON_THROW(expr) throw expr
#else
#  include <cstdlib>
namespace yase_json::detail {
[[noreturn]] inline void fail() noexcept { std::abort(); }
} // namespace yase_json::detail
#  define YASE_JSON_THROW(expr) ::yase_json::detail::fail()
#endif
