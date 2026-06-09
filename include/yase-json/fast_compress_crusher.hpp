#pragma once

#include <string>
#include <string_view>

#include "yase-json/fast_compress.hpp"

namespace yase_json {

/**
 * @brief compress → crush パイプラインを一体管理するクラス。
 *
 * FastCompressor と FastCrusher を組み合わせ、compress 出力の安定性を観察して
 * crush 辞書を自動的に構築する。
 * 呼び出し側は JSON を渡すだけで、compress → crush の全処理が実行される。
 */
class FastCompressCrusher {
public:
  /**
   * @brief コンストラクタ
   * @param warmup_threshold 辞書構築を開始するまでのフォールバック回数（デフォルト: 2）
   */
  explicit FastCompressCrusher(std::size_t warmup_threshold = 2)
    : warmup_threshold_{warmup_threshold} {}

  /**
   * @brief JSON を compress して crush した文字列を返す
   * @param json_str 圧縮対象の JSON 文字列
   * @return uncrush() → Decompressor::decompress() で復元可能な文字列
   */
  auto compress_crush(std::string_view json_str) -> std::string;

  /**
   * @brief compress_crush の逆変換
   * @param crushed compress_crush() の出力
   * @return 元の JSON と意味的に等価な JSON 文字列
   */
  auto uncrush_decompress(std::string_view crushed) -> std::string;

  /**
   * @brief 全キャッシュをリセットする
   */
  auto reset() noexcept -> void;

private:
  FastCompressor fast_compressor_{};
  FastCrusher    fast_crusher_{};
  Decompressor   decompressor_{};
  std::size_t    warmup_threshold_{};
  std::size_t    stable_count_{};
  std::string    last_compressed_{};
  bool           crusher_ready_{};
};

// --- FastCompressCrusher 実装 ---

inline auto FastCompressCrusher::compress_crush(std::string_view json_str) -> std::string {
  auto const compressed = fast_compressor_.compress(json_str);

  if (!crusher_ready_) {
    // 出力の安定性を検出
    if (compressed == last_compressed_) {
      ++stable_count_;
    }
    else {
      stable_count_ = 1;
    }
    last_compressed_ = compressed;

    // 辞書構築条件を判定
    if (stable_count_ >= warmup_threshold_) {
      fast_crusher_.warm_up(compressed);
      crusher_ready_ = true;
    }
    else {
      return yase_json::crush(compressed);
    }
  }

  return fast_crusher_.crush(compressed);
}

inline auto FastCompressCrusher::uncrush_decompress(std::string_view crushed) -> std::string {
  auto const uncrushed = yase_json::uncrush(crushed);
  return decompressor_.decompress(uncrushed);
}

inline auto FastCompressCrusher::reset() noexcept -> void {
  fast_compressor_.reset();
  fast_crusher_.reset();
  stable_count_ = 0;
  last_compressed_.clear();
  crusher_ready_ = false;
}

} // namespace yase_json
