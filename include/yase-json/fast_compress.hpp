#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

#include "yase-json/compress.hpp"
#include "yase-json/crush.hpp"
#include "yase-json/decompress.hpp"

namespace yase_json {

template<size_t N>
struct FixedString {
  char buf[N];
  constexpr FixedString(char const (&s)[N]) {
    for (size_t i = 0; i < N; ++i) buf[i] = s[i];
  }
  constexpr operator std::string_view() const { return {buf, N - 1}; }
};

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

  /** @brief 圧縮対象のフィールドを設定する */
  auto set_fields(std::vector<std::string> fields) -> void {
    reset();
    target_fields_ = std::move(fields);
    is_selective_ = true;
  }

private:
  detail::CompressionMemory memory_{};
  size_t schema_snapshot_size_{0};
  bool schema_cached_{false};
  std::vector<std::string> cached_keys_{};
  std::string cached_schema_key_{};

  std::vector<std::string> target_fields_{};
  bool is_selective_{false};

  /** @brief オブジェクトのキーがスキーマと一致するか確認する */
  auto keys_match_schema(glz::generic::object_t const& object) const -> bool;
};

template<FixedString... Fields>
class StaticFastCompressor : public FastCompressor {
public:
  StaticFastCompressor() {
    set_fields({std::string(Fields)...});
  }
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

  // フィールド選択が有効な場合、オブジェクトをフィルタリング
  if (is_selective_) {
    auto const& original_object = data.get<glz::generic::object_t>();
    auto filtered_object = glz::generic::object_t{};
    for (auto const& field : target_fields_) {
      if (original_object.contains(field)) {
        filtered_object[field] = original_object.at(field);
      }
    }
    data = std::move(filtered_object);
  }

  auto const& object = data.get<glz::generic::object_t>();

  // 初回、またはキー集合が変化した場合
  if (!schema_cached_ || !keys_match_schema(object)) {
    reset();
    auto const root_key = detail::unwrap(memory_.add_value(data));

    // スキーマ関連のエントリをキャッシュ
    cached_keys_.clear();
    for (auto const& [key, _] : object) {
      std::ignore = _;
      cached_keys_.emplace_back(key);
    }
    cached_schema_key_ = memory_.get_schema(cached_keys_);
    schema_snapshot_size_ = memory_.values.size();
    schema_cached_ = true;

    return detail::unwrap(detail::write_compressed(memory_.values, root_key));
  }

  // 2回目以降: スキーマは固定。値部分のみをエンコード
  memory_.values.resize(schema_snapshot_size_);

  // value_cache からスナップショット以降のエントリを除去
  auto& cache = memory_.value_cache;
  for (auto it = cache.begin(); it != cache.end();) {
    if (detail::unwrap(detail::from_base62(it->second)) >= schema_snapshot_size_) {
      it = cache.erase(it);
    }
    else {
      ++it;
    }
  }

  // schema_cache のエントリが参照する value_key も
  // スナップショット範囲内 [0, schema_snapshot_size_) に収まることを保証する。
  // スキーマは固定されているため通常クリア不要だが、
  // values の resize 後に schema_cache の整合性を明示的に検証する。
  // キャッシュヒット後は schema_snapshot_size_ 以降に新規エントリは追加されないため
  // 既存の schema_cache エントリは全て有効である。
  // ただし不変条件として assert を追加する:
#ifndef NDEBUG
  for (auto const& [sig, key] : memory_.schema_cache) {
    assert(detail::unwrap(detail::from_base62(key)) < schema_snapshot_size_
           && "schema_cache key out of snapshot range");
  }
#endif

  // スキーマキーを直接使い、各値をエンコード
  auto encoded = std::string{"o|"};
  encoded += cached_schema_key_;
  for (auto const& [key, child] : object) {
    std::ignore = key;
    encoded.push_back('|');
    encoded += detail::unwrap(memory_.add_value(child));
  }
  auto const root_key = memory_.get_value_key(std::move(encoded));
  return detail::unwrap(detail::write_compressed(memory_.values, root_key));
}

inline auto FastCompressor::reset() noexcept -> void {
  // memory_ の再構築により value_cache / schema_cache を含む全キャッシュをクリアする
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

  auto string = detail::unwrap(detail::utf8_to_utf16(input));
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
  auto string = detail::unwrap(detail::utf8_to_utf16(template_json));
  string.erase(
    std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER),
    string.end()
  );
  string = detail::json_crush_swap(string);

  std::ignore = detail::run_greedy_loop(
    std::move(string), 50,
    [this](std::u16string const& pattern, char16_t const replace_char) {
      dictionary_.push_back({pattern, replace_char});
    }
  );
  dictionary_built_ = true;
}

inline auto FastCrusher::apply_dictionary(std::u16string string) const -> std::string {
  // split 文字列を構築（先頭に挿入する順序で作成）
  auto split_string = std::u16string{};

  // 辞書の各エントリを順に適用。置換文字が入力に自然に含まれる場合は
  // uncrush の区切り文字と衝突して復元が破損するため、そのエントリをスキップする
  // ponytail: per-entry skip keeps correctness; full fallback if throughput matters
  for (auto const& entry : dictionary_) {
    if (string.find(entry.replacement_char) != std::u16string::npos) {
      continue;
    }
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

  return detail::unwrap(detail::utf16_to_utf8(string));
}

} // namespace yase_json
