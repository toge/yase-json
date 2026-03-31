#ifndef __YASE_JSON_CRUSH_HPP__
#define __YASE_JSON_CRUSH_HPP__

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <ranges>
#include <cstdint>

namespace yase_json {

auto constexpr JS_CRUSH_CHARS = std::string_view{
  R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)"
};

/**
 * @brief JSON文字列を文字列レベルで圧縮します。
 * @param input 圧縮対象のJSON文字列。
 * @return 圧縮後の文字列。
 */
auto crush(std::string_view input) -> std::string {
  if (input.empty()) {
    return "";
  }
  auto crushed = std::string(input);

  auto const split_char = '\1';

  // 1. 未使用文字の抽出 (高速スキャン)
  auto unused_chars = std::vector<char>{};
  unused_chars.reserve(JS_CRUSH_CHARS.size());
  {
    bool used[256] = {false};
    for (auto const c : crushed) {
      used[static_cast<unsigned char>(c)] = true;
    }
    for (auto const c : JS_CRUSH_CHARS) {
      if (!used[static_cast<unsigned char>(c)]) {
        unused_chars.push_back(c);
      }
    }
  }
  std::reverse(unused_chars.begin(), unused_chars.end());

  // 2. ローリングハッシュを用いた候補抽出
  // 頻出する部分文字列を効率的に見つける
  while (!unused_chars.empty()) {
    struct Candidate {
      std::string_view sub;
      int64_t savings;
    };
    auto best = Candidate{std::string_view{}, 0};

    // 現実的な長さ (4, 8, 12, 16) に絞ってハッシュでカウント
    static constexpr size_t lens[] = {4, 8, 12, 16};
    auto counts = std::unordered_map<std::string_view, int>{};
    counts.reserve(crushed.size() / 2);

    for (auto const len : lens) {
      if (crushed.size() < len) continue;
      // ステップ実行で高速化
      for (size_t i = 0; i <= crushed.size() - len; i += (len / 2)) {
        counts[std::string_view(crushed).substr(i, len)]++;
      }
    }

    for (auto const& [sub, count] : counts) {
      if (count <= 1) {
        continue;
      }
      // 正確な出現回数を再確認
      int actual_count = 0;
      auto pos = crushed.find(sub);
      while (pos != std::string::npos) {
        actual_count++;
        pos = crushed.find(sub, pos + sub.size());
      }

      auto const savings = (int64_t(sub.size()) * (actual_count - 1)) - (int64_t(sub.size()) + 2);
      if (savings > best.savings) {
        best = {sub, savings};
      }
    }

    if (best.savings <= 0) {
      break;
    }

    auto const replace_char = unused_chars.back();
    unused_chars.pop_back();

    std::string target(best.sub);
    auto pos = size_t{0};
    while ((pos = crushed.find(target, pos)) != std::string::npos) {
      crushed.replace(pos, target.length(), 1, replace_char);
      pos += 1;
    }

    crushed += split_char;
    crushed += replace_char;
    crushed += target;
  }

  return crushed;
}

/**
 * @brief crushで圧縮された文字列を元の形式に復元します。
 * @param input 圧縮された文字列。
 * @return 復元された元のJSON文字列。
 */
auto uncrush(std::string_view input) -> std::string {
  if (input.empty()) {
    return "";
  }
  auto const split_char = '\1';
  auto current_str = std::string(input);

  while (true) {
    auto const sep = current_str.find_last_of(split_char);
    if (sep == std::string::npos) {
      break;
    }

    auto const entry = current_str.substr(sep + 1);
    current_str = current_str.substr(0, sep);

    if (entry.size() >= 2) {
      auto const replace_char = entry[0];
      auto const original_sub = entry.substr(1);

      auto pos = size_t{0};
      while ((pos = current_str.find(replace_char, pos)) != std::string::npos) {
        current_str.replace(pos, 1, original_sub);
        pos += original_sub.length();
      }
    }
  }
  return current_str;
}

} // namespace yase_json

#endif // __YASE_JSON_CRUSH_HPP__
