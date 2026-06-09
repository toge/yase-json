#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"

namespace yase_json {

/**
 * @brief 同一キー集合のJSONオブジェクトを高速に圧縮するクラス。
 *
 * 初回呼び出し時にスキーマ（キー→valuesテーブルインデックス）を学習し、
 * 2回目以降はスキーマ解決コストをスキップして compress を行う。
 * 出力は Decompressor::decompress() で復元できる compress-json 互換形式。
 */
class FastCompressor {
public:
  /** @brief 圧縮する。初回呼び出し時にスキーマを学習する。 */
  auto compress(std::string_view json_str) -> std::string;

  /** @brief キャッシュをリセットする（キー集合が変わる場合に呼ぶ） */
  auto reset() noexcept -> void;

private:
  detail::CompressionMemory memory_{};
  size_t schema_snapshot_size_{0};
  bool schema_cached_{false};
  std::vector<std::string> cached_keys_{};
  std::string cached_schema_key_{};

  /** @brief オブジェクトのキーがスキーマと一致するか確認する */
  auto keys_match_schema(glz::generic::object_t const& object) const -> bool;
};

// --- FastCompressor 実装 ---

inline auto FastCompressor::compress(std::string_view json_str) -> std::string {
  auto data = glz::generic{};
  if (auto const ec = glz::read_json(data, json_str)) {
    throw std::runtime_error("Failed to parse JSON: " + glz::format_error(ec, json_str));
  }

  // オブジェクト以外の入力は通常の Compressor に委譲
  if (!data.is_object()) {
    return Compressor{}.compress(json_str);
  }

  auto const& object = data.get<glz::generic::object_t>();

  // 初回、またはキー集合が変化した場合
  if (!schema_cached_ || !keys_match_schema(object)) {
    reset();
    auto const root_key = memory_.add_value(data);

    // スキーマ関連のエントリをキャッシュ
    cached_keys_.clear();
    for (auto const& [key, _] : object) {
      std::ignore = _;
      cached_keys_.emplace_back(key);
    }
    cached_schema_key_ = memory_.get_schema(cached_keys_);
    schema_snapshot_size_ = memory_.values.size();
    schema_cached_ = true;

    // 出力构建
    auto values = glz::generic::array_t{};
    values.reserve(memory_.values.size());
    for (auto const& value : memory_.values) {
      auto node = glz::generic{};
      node = value;
      values.emplace_back(std::move(node));
    }

    auto result = glz::generic::array_t{};
    auto values_node = glz::generic{};
    values_node = std::move(values);
    result.emplace_back(std::move(values_node));

    auto root_node = glz::generic{};
    root_node = root_key;
    result.emplace_back(std::move(root_node));

    auto final_node = glz::generic{};
    final_node = std::move(result);

    auto out = std::string{};
    if (auto const ec = glz::write_json(final_node, out)) {
      throw std::runtime_error("Failed to generate compressed JSON");
    }
    return out;
  }

  // 2回目以降: スキーマは固定。値部分のみをエンコード
  memory_.values.resize(schema_snapshot_size_);

  // value_cache からスナップショット以降のエントリを除去
  auto& cache = memory_.value_cache;
  for (auto it = cache.begin(); it != cache.end();) {
    if (detail::from_base62(it->second) >= schema_snapshot_size_) {
      it = cache.erase(it);
    }
    else {
      ++it;
    }
  }

  // スキーマキーを直接使い、各値をエンコード
  auto encoded = std::string{"o|"};
  encoded += cached_schema_key_;
  for (auto const& [key, child] : object) {
    std::ignore = key;
    encoded.push_back('|');
    encoded += memory_.add_value(child);
  }
  auto const root_key = memory_.get_value_key(std::move(encoded));

  // 出力构建
  auto values = glz::generic::array_t{};
  values.reserve(memory_.values.size());
  for (auto const& value : memory_.values) {
    auto node = glz::generic{};
    node = value;
    values.emplace_back(std::move(node));
  }

  auto result = glz::generic::array_t{};
  auto values_node = glz::generic{};
  values_node = std::move(values);
  result.emplace_back(std::move(values_node));

  auto root_node = glz::generic{};
  root_node = root_key;
  result.emplace_back(std::move(root_node));

  auto final_node = glz::generic{};
  final_node = std::move(result);

  auto out = std::string{};
  if (auto const ec = glz::write_json(final_node, out)) {
    throw std::runtime_error("Failed to generate compressed JSON");
  }
  return out;
}

inline auto FastCompressor::reset() noexcept -> void {
  memory_ = detail::CompressionMemory{};
  schema_snapshot_size_ = 0;
  schema_cached_ = false;
  cached_keys_.clear();
  cached_schema_key_.clear();
}

inline auto FastCompressor::keys_match_schema(glz::generic::object_t const& object) const -> bool {
  if (object.size() != cached_keys_.size()) {
    return false;
  }
  size_t index = 0;
  for (auto const& [key, _] : object) {
    std::ignore = _;
    if (key != cached_keys_[index]) {
      return false;
    }
    ++index;
  }
  return true;
}

/**
 * @brief 同一キー集合のJSONを高速にcrushするクラス。
 *
 * 初回crush呼び出し時に辞書（パターン→置換文字のペアリスト）を構築し、
 * 2回目以降は辞書を再利用して O(N) で crush を行う。
 * 出力は uncrush() で復元できる JSONCrush 互換形式。
 */
class FastCrusher {
public:
  /** @brief テンプレートJSONで辞書を事前構築する（任意。省略時は初回 crush() で構築） */
  auto warm_up(std::string_view template_json) -> void;

  /** @brief crush する。warm_up() 済みか初回呼び出し後は辞書を再利用する。 */
  auto crush(std::string_view input) -> std::string;

  /** @brief 通常の uncrush() に委譲する（互換性のためメンバ関数として提供） */
  [[nodiscard]] auto uncrush(std::string_view input) const -> std::string;

  /** @brief 辞書キャッシュをリセットする */
  auto reset() noexcept -> void;

private:
  /** @brief 辞書の1エントリ（パターンと置換文字のペア） */
  struct DictEntry {
    std::u16string pattern;
    char16_t replacement_char{};
  };

  std::vector<DictEntry> dictionary_{};
  bool dictionary_built_{false};

  /** @brief 通常の crush ルーチンで辞書を構築する */
  auto build_dictionary(std::string_view template_json) -> void;

  /** @brief 辞書を入力文字列に適用する */
  [[nodiscard]] auto apply_dictionary(std::u16string string) const -> std::string;
};

// --- FastCrusher 実装 ---

inline auto FastCrusher::warm_up(std::string_view template_json) -> void {
  reset();
  build_dictionary(template_json);
}

inline auto FastCrusher::crush(std::string_view input) -> std::string {
  if (!dictionary_built_) {
    build_dictionary(input);
  }

  auto string = detail::utf8_to_utf16(input);
  string.erase(
    std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER),
    string.end()
  );
  string = detail::json_crush_swap(string);

  return apply_dictionary(std::move(string));
}

inline auto FastCrusher::uncrush(std::string_view input) const -> std::string {
  return yase_json::uncrush(input);
}

inline auto FastCrusher::reset() noexcept -> void {
  dictionary_.clear();
  dictionary_built_ = false;
}

inline auto FastCrusher::build_dictionary(std::string_view template_json) -> void {
  // 通常の crush と同じ前処理
  auto string = detail::utf8_to_utf16(template_json);
  string.erase(
    std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER),
    string.end()
  );
  string = detail::json_crush_swap(string);

  // js_crush_utf16 と同一の greedy ループで辞書を構築
  auto split_string = std::u16string{};
  auto candidates = detail::build_initial_candidates<char16_t>(string, 50);
  auto replace_pos = static_cast<int64_t>(detail::replacement_characters_utf16.size());

  while (true) {
    // 現在の文字列に出現する文字をセット
    std::bitset<65536> present;
    for (auto const c : string) {
      present.set(static_cast<uint16_t>(c));
    }

    // 未使用の置換文字を探す
    auto replace_char = char16_t{0};
    while (replace_pos > 0) {
      auto const c = detail::replacement_characters_utf16[--replace_pos];
      if (!present.test(static_cast<uint16_t>(c))) {
        replace_char = c;
        break;
      }
    }
    if (replace_char == 0) {
      break;
    }

    // 最適な候補を選択
    auto best_idx = size_t{0};
    auto best_delta = int64_t{0};
    auto const rep_len = detail::encoded_uri_length(std::u16string_view{&replace_char, 1});
    auto const delim_len = detail::encoded_uri_length(
      std::u16string_view{&detail::JSON_CRUSH_DELIMITER, 1}
    );

    auto it = candidates.begin();
    while (it != candidates.end()) {
      auto delta = (it->count - 1) * it->encoded_length - (it->count + 1) * rep_len;
      if (split_string.empty()) {
        delta -= delim_len;
      }

      if (delta <= 0) {
        it = candidates.erase(it);
      }
      else {
        if (delta > best_delta) {
          best_delta = delta;
          best_idx = std::distance(candidates.begin(), it);
        }
        ++it;
      }
    }
    if (best_delta <= 0 || candidates.empty()) {
      break;
    }

    // 選択されたパターンを辞書に記録
    auto const best_sub = candidates[best_idx].value;
    dictionary_.push_back({best_sub, replace_char});

    // 文字列を置換
    string = detail::replace_all_with_char<char16_t>(string, best_sub, replace_char);
    string.push_back(replace_char);
    string.append(best_sub);
    split_string.insert(split_string.begin(), replace_char);

    // 候補リストを再構築
    std::vector<detail::OrderedCandidate<char16_t>> next_cands;
    std::unordered_map<std::u16string, size_t> seen;
    for (auto& c : candidates) {
      auto rewritten = detail::replace_all_with_char<char16_t>(c.value, best_sub, replace_char);
      if (rewritten.size() < 2) {
        continue;
      }
      if (!seen.count(rewritten)) {
        seen[rewritten] = next_cands.size();
        next_cands.push_back({rewritten, 0, detail::encoded_uri_length(rewritten)});
      }
    }
    candidates = std::move(next_cands);
    detail::count_candidates<char16_t>(string, candidates);
  }

  dictionary_built_ = true;
}

inline auto FastCrusher::apply_dictionary(std::u16string string) const -> std::string {
  // split 文字列を構築（先頭に挿入する順序で作成）
  auto split_string = std::u16string{};

  // 辞書の各エントリを順に適用
  for (auto const& entry : dictionary_) {
    string = detail::replace_all_with_char<char16_t>(
      std::u16string_view{string}, entry.pattern, entry.replacement_char
    );
    string.push_back(entry.replacement_char);
    string.append(entry.pattern);
    // 標準の crush と同一の順序で split 文字列を構築
    split_string.insert(split_string.begin(), entry.replacement_char);
  }

  // split 文字列を付加
  if (!split_string.empty()) {
    string.push_back(detail::JSON_CRUSH_DELIMITER);
    string += split_string;
  }

  // 末尾に '_' を追加
  string.push_back(u'_');

  return detail::utf16_to_utf8(string);
}

} // namespace yase_json
