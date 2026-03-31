#ifndef __YASE_JSON_CRUSH_HPP__
#define __YASE_JSON_CRUSH_HPP__

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yase_json {

namespace detail {

auto constexpr JSON_CRUSH_DELIMITER = char16_t{u'\x0001'};
auto constexpr JSON_CRUSH_DELIMITER_ASCII = char{'\x01'};

template <typename CharT>
/**
 * @brief JSONCrush の圧縮結果を保持します。
 * @tparam CharT 文字列の文字型。
 */
struct JSCrushResult {
  std::basic_string<CharT> crushed;
  std::basic_string<CharT> split;
};

template <typename CharT>
/**
 * @brief 置換候補となる部分文字列の情報を保持します。
 * @tparam CharT 文字列の文字型。
 */
struct OrderedCandidate {
  std::basic_string<CharT> value;
  int64_t count = 0;
  int64_t encoded_length = 0;
};

/**
 * @brief UTF-16 コードユニットが上位サロゲートか判定します。
 * @param value 判定対象のコードユニット。
 * @return 上位サロゲートなら true、それ以外は false。
 */
auto constexpr is_high_surrogate = [](char16_t const value) {
  return value >= 0xD800 && value <= 0xDBFF;
};

/**
 * @brief UTF-16 コードユニットが下位サロゲートか判定します。
 * @param value 判定対象のコードユニット。
 * @return 下位サロゲートなら true、それ以外は false。
 */
auto constexpr is_low_surrogate = [](char16_t const value) {
  return value >= 0xDC00 && value <= 0xDFFF;
};

/**
 * @brief URI エンコードせずに利用できる文字か判定します。
 * @param value 判定対象の Unicode コードポイント。
 * @return そのまま利用できる場合は true、それ以外は false。
 */
auto constexpr is_uri_unescaped = [](char32_t const value) {
  return (value >= U'0' && value <= U'9') ||
         (value >= U'A' && value <= U'Z') ||
         (value >= U'a' && value <= U'z') ||
         value == U'-' || value == U'_' || value == U'.' || value == U'!' ||
         value == U'~' || value == U'*' || value == U'\'' || value == U'(' ||
         value == U')';
};

      /**
       * @brief Unicode コードポイントの UTF-8 バイト長を返します。
       * @param value 対象の Unicode コードポイント。
       * @return UTF-8 で表現したときのバイト数。
       */
auto constexpr utf8_length = [](char32_t const value) -> int64_t {
  if (value <= 0x7F) {
    return 1;
  }
  if (value <= 0x7FF) {
    return 2;
  }
  if (value <= 0xFFFF) {
    return 3;
  }
  return 4;
};

/**
 * @brief Unicode コードポイントを UTF-16 として文字列へ追記します。
 * @param output 追記先の UTF-16 文字列。
 * @param value 追記する Unicode コードポイント。
 */
auto const append_utf16 = [](std::u16string& output, char32_t const value) {
  if (value <= 0xFFFF) {
    output.push_back(static_cast<char16_t>(value));
    return;
  }

  auto const shifted = value - 0x10000;
  output.push_back(static_cast<char16_t>(0xD800 + (shifted >> 10)));
  output.push_back(static_cast<char16_t>(0xDC00 + (shifted & 0x3FF)));
};

/**
 * @brief Unicode コードポイントを UTF-8 として文字列へ追記します。
 * @param output 追記先の UTF-8 文字列。
 * @param value 追記する Unicode コードポイント。
 */
auto const append_utf8 = [](std::string& output, char32_t const value) {
  if (value <= 0x7F) {
    output.push_back(static_cast<char>(value));
    return;
  }
  if (value <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (value >> 6)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    return;
  }
  if (value <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (value >> 12)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
    return;
  }

  output.push_back(static_cast<char>(0xF0 | (value >> 18)));
  output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
};

/**
 * @brief UTF-8 文字列から 1 つの Unicode コードポイントを読み取ります。
 * @param input 読み取り元の UTF-8 文字列。
 * @param index 現在位置を表すインデックス。読み取り後は次位置へ更新されます。
 * @return 読み取った Unicode コードポイント。
 * @throw std::runtime_error UTF-8 シーケンスが不正な場合。
 */
auto const decode_utf8_code_point = [](std::string_view const input, size_t& index) -> char32_t {
  auto const fail = [] {
    throw std::runtime_error("Invalid UTF-8 input for JSONCrush");
  };

  auto const lead = static_cast<unsigned char>(input[index]);
  if (lead <= 0x7F) {
    ++index;
    return static_cast<char32_t>(lead);
  }

  auto read_continuation = [&](size_t const offset) -> unsigned char {
    if (index + offset >= input.size()) {
      fail();
    }
    auto const byte = static_cast<unsigned char>(input[index + offset]);
    if ((byte & 0xC0) != 0x80) {
      fail();
    }
    return byte;
  };

  if ((lead & 0xE0) == 0xC0) {
    auto const byte1 = read_continuation(1);
    auto const value = static_cast<char32_t>(((lead & 0x1F) << 6) | (byte1 & 0x3F));
    if (value < 0x80) {
      fail();
    }
    index += 2;
    return value;
  }

  if ((lead & 0xF0) == 0xE0) {
    auto const byte1 = read_continuation(1);
    auto const byte2 = read_continuation(2);
    auto const value = static_cast<char32_t>(((lead & 0x0F) << 12) |
                                             ((byte1 & 0x3F) << 6) |
                                             (byte2 & 0x3F));
    if (value < 0x800 || (value >= 0xD800 && value <= 0xDFFF)) {
      fail();
    }
    index += 3;
    return value;
  }

  if ((lead & 0xF8) == 0xF0) {
    auto const byte1 = read_continuation(1);
    auto const byte2 = read_continuation(2);
    auto const byte3 = read_continuation(3);
    auto const value = static_cast<char32_t>(((lead & 0x07) << 18) |
                                             ((byte1 & 0x3F) << 12) |
                                             ((byte2 & 0x3F) << 6) |
                                             (byte3 & 0x3F));
    if (value < 0x10000 || value > 0x10FFFF) {
      fail();
    }
    index += 4;
    return value;
  }

  fail();
  return char32_t{};
};

/**
 * @brief UTF-8 文字列を UTF-16 文字列へ変換します。
 * @param input 変換元の UTF-8 文字列。
 * @return 変換後の UTF-16 文字列。
 * @throw std::runtime_error 入力が不正な UTF-8 の場合。
 */
auto const utf8_to_utf16 = [](std::string_view const input) -> std::u16string {
  auto output = std::u16string{};
  output.reserve(input.size());

  auto index = size_t{0};
  while (index < input.size()) {
    append_utf16(output, decode_utf8_code_point(input, index));
  }
  return output;
};

/**
 * @brief ASCII 文字列を UTF-16 文字列へ変換します。
 * @param input 変換元の ASCII 文字列。
 * @return 変換後の UTF-16 文字列。
 */
auto const ascii_to_utf16 = [](std::string_view const input) -> std::u16string {
  auto output = std::u16string{};
  output.reserve(input.size());
  for (auto const ch : input) {
    output.push_back(static_cast<char16_t>(static_cast<unsigned char>(ch)));
  }
  return output;
};

/**
 * @brief UTF-16 文字列を UTF-8 文字列へ変換します。
 * @param input 変換元の UTF-16 文字列。
 * @return 変換後の UTF-8 文字列。
 * @throw std::runtime_error 入力が不正な UTF-16 の場合。
 */
auto const utf16_to_utf8 = [](std::u16string_view const input) -> std::string {
  auto output = std::string{};
  output.reserve(input.size());

  for (auto index = size_t{0}; index < input.size(); ++index) {
    auto const value = input[index];
    if (is_high_surrogate(value)) {
      if (index + 1 >= input.size() || !is_low_surrogate(input[index + 1])) {
        throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
      }
      auto const code_point = static_cast<char32_t>(
        0x10000 +
        ((static_cast<char32_t>(value - 0xD800) << 10) |
         static_cast<char32_t>(input[index + 1] - 0xDC00)));
      append_utf8(output, code_point);
      ++index;
      continue;
    }
    if (is_low_surrogate(value)) {
      throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
    }
    append_utf8(output, static_cast<char32_t>(value));
  }

  return output;
};

/**
 * @brief UTF-16 文字列を URI エンコードした場合の長さを計算します。
 * @param input 対象の UTF-16 文字列。
 * @return URI エンコード後の文字列長。
 * @throw std::runtime_error 入力が不正な UTF-16 の場合。
 */
auto const encoded_uri_length = [](std::u16string_view const input) -> int64_t {
  auto length = int64_t{0};

  for (auto index = size_t{0}; index < input.size(); ++index) {
    auto code_point = char32_t{input[index]};
    if (is_high_surrogate(input[index])) {
      if (index + 1 >= input.size() || !is_low_surrogate(input[index + 1])) {
        throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
      }
      code_point = static_cast<char32_t>(
        0x10000 +
        ((static_cast<char32_t>(input[index] - 0xD800) << 10) |
         static_cast<char32_t>(input[index + 1] - 0xDC00)));
      ++index;
    } else if (is_low_surrogate(input[index])) {
      throw std::runtime_error("Invalid UTF-16 data for JSONCrush");
    }

    if (is_uri_unescaped(code_point)) {
      ++length;
    } else {
      length += 3 * utf8_length(code_point);
    }
  }

  return length;
};

/**
 * @brief 部分文字列の両端に不完全なサロゲートが存在するか判定します。
 * @param input 判定対象の UTF-16 文字列。
 * @return 不完全なサロゲートを含む場合は true、それ以外は false。
 */
auto constexpr has_unmatched_surrogate = [](std::u16string_view const input) {
  if (input.empty()) {
    return false;
  }
  return is_low_surrogate(input.front()) || is_high_surrogate(input.back());
};

template <typename CharT>
/**
 * @brief 指定文字で文字列を分割します。
 * @tparam CharT 文字列の文字型。
 * @param input 分割対象の文字列。
 * @param delimiter 区切り文字。
 * @return 分割後の文字列配列。
 */
auto split_on_char(std::basic_string_view<CharT> const input, CharT const delimiter)
  -> std::vector<std::basic_string<CharT>> {
  auto parts = std::vector<std::basic_string<CharT>>{};
  auto start = size_t{0};

  while (true) {
    auto const pos = input.find(delimiter, start);
    if (pos == std::basic_string_view<CharT>::npos) {
      parts.emplace_back(input.substr(start));
      return parts;
    }
    parts.emplace_back(input.substr(start, pos - start));
    start = pos + 1;
  }
}

template <typename CharT>
/**
 * @brief 文字列配列を指定セパレーターで連結します。
 * @tparam CharT 文字列の文字型。
 * @param parts 連結対象の文字列配列。
 * @param separator 区切りとして挿入する文字列。
 * @return 連結後の文字列。
 */
auto join_strings(std::vector<std::basic_string<CharT>> const& parts,
                  std::basic_string_view<CharT> const separator)
  -> std::basic_string<CharT> {
  auto output = std::basic_string<CharT>{};
  for (auto const index : std::views::iota(size_t{0}, parts.size())) {
    if (index != 0) {
      output += separator;
    }
    output += parts[index];
  }
  return output;
}

template <typename CharT>
/**
 * @brief 文字列内の部分文字列をすべて別文字列へ置換します。
 * @tparam CharT 文字列の文字型。
 * @param input 置換対象の文字列。
 * @param target 置換元の部分文字列。
 * @param replacement 置換先の文字列。
 * @return 置換後の文字列。
 */
auto replace_all(std::basic_string_view<CharT> const input,
                 std::basic_string_view<CharT> const target,
                 std::basic_string_view<CharT> const replacement)
  -> std::basic_string<CharT> {
  if (target.empty()) {
    return std::basic_string<CharT>{input};
  }

  auto output = std::basic_string<CharT>{};
  auto position = size_t{0};
  while (position < input.size()) {
    auto const found = input.find(target, position);
    if (found == std::basic_string_view<CharT>::npos) {
      output.append(input.substr(position));
      return output;
    }

    output.append(input.substr(position, found - position));
    output.append(replacement);
    position = found + target.size();
  }
  return output;
}

template <typename CharT>
/**
 * @brief 文字列内の部分文字列を 1 文字へ置換します。
 * @tparam CharT 文字列の文字型。
 * @param input 置換対象の文字列。
 * @param target 置換元の部分文字列。
 * @param replacement 置換先の文字。
 * @return 置換後の文字列。
 */
auto replace_all_with_char(std::basic_string_view<CharT> const input,
                           std::basic_string_view<CharT> const target,
                           CharT const replacement)
  -> std::basic_string<CharT> {
  auto const replacement_view = std::basic_string_view<CharT>{&replacement, 1};
  return replace_all(input, target, replacement_view);
}

template <typename CharT>
/**
 * @brief 2 種類のトークンを相互に入れ替えます。
 * @tparam CharT 文字列の文字型。
 * @param input 入れ替え対象の文字列。
 * @param left 入れ替え対象の左側トークン。
 * @param right 入れ替え対象の右側トークン。
 * @return 入れ替え後の文字列。
 */
auto swap_internal(std::basic_string_view<CharT> const input,
                   std::basic_string_view<CharT> const left,
                   std::basic_string_view<CharT> const right)
  -> std::basic_string<CharT> {
  auto output = std::basic_string<CharT>{};
  auto position = size_t{0};

  while (position < input.size()) {
    if (position + left.size() <= input.size() &&
        input.substr(position, left.size()) == left) {
      output += right;
      position += left.size();
      continue;
    }
    if (position + right.size() <= input.size() &&
        input.substr(position, right.size()) == right) {
      output += left;
      position += right.size();
      continue;
    }

    output.push_back(input[position]);
    ++position;
  }

  return output;
}

/**
 * @brief JSONCrush 用の記号置換を UTF-16 文字列に適用または逆適用します。
 * @param input 変換対象の UTF-16 文字列。
 * @param forward true の場合は圧縮向けの置換、false の場合は復元向けの逆置換を行います。
 * @return 変換後の UTF-16 文字列。
 */
auto const json_crush_swap = [](std::u16string_view const input, bool const forward = true) {
  auto string = std::u16string{input};
  auto const swap_groups = std::array{
    std::pair{std::u16string_view{u"\""}, std::u16string_view{u"'"}},
    std::pair{std::u16string_view{u"':"}, std::u16string_view{u"!"}},
    std::pair{std::u16string_view{u",'"}, std::u16string_view{u"~"}},
    std::pair{std::u16string_view{u"}"}, std::u16string_view{u")"}},
    std::pair{std::u16string_view{u"{"}, std::u16string_view{u"("}},
  };

  if (forward) {
    for (auto const& [left, right] : swap_groups) {
      string = swap_internal(std::u16string_view{string.data(), string.size()}, left, right);
    }
    return string;
  }

  for (auto const index : std::views::iota(size_t{0}, swap_groups.size()) | std::views::reverse) {
    auto const& [left, right] = swap_groups[index];
    string = swap_internal(std::u16string_view{string.data(), string.size()}, left, right);
  }
  return string;
};

/**
 * @brief JSONCrush 用の記号置換を ASCII 文字列に適用または逆適用します。
 * @param input 変換対象の ASCII 文字列。
 * @param forward true の場合は圧縮向けの置換、false の場合は復元向けの逆置換を行います。
 * @return 変換後の ASCII 文字列。
 */
auto const json_crush_swap_ascii = [](std::string_view const input, bool const forward = true) {
  auto string = std::string{input};
  auto const swap_groups = std::array{
    std::pair{std::string_view{"\""}, std::string_view{"'"}},
    std::pair{std::string_view{"':"}, std::string_view{"!"}},
    std::pair{std::string_view{",'"}, std::string_view{"~"}},
    std::pair{std::string_view{"}"}, std::string_view{")"}},
    std::pair{std::string_view{"{"}, std::string_view{"("}},
  };

  if (forward) {
    for (auto const& [left, right] : swap_groups) {
      string = swap_internal(std::string_view{string.data(), string.size()}, left, right);
    }
    return string;
  }

  for (auto const index : std::views::iota(size_t{0}, swap_groups.size()) | std::views::reverse) {
    auto const& [left, right] = swap_groups[index];
    string = swap_internal(std::string_view{string.data(), string.size()}, left, right);
  }
  return string;
};

template <typename CharT>
/**
 * @brief 部分文字列比較を高速化するローリングハッシュを提供します。
 * @tparam CharT 文字列の文字型。
 */
class RollingHash {
public:
  /**
   * @brief 文字列全体からローリングハッシュを初期化します。
   * @param input ハッシュ対象の文字列。
   */
  explicit RollingHash(std::basic_string_view<CharT> const input) :
    prefix_(input.size() + 1, 0),
    powers_(input.size() + 1, 1) {
    for (auto const index : std::views::iota(size_t{0}, input.size())) {
      prefix_[index + 1] = prefix_[index] * kBase + code_unit(input[index]);
      powers_[index + 1] = powers_[index] * kBase;
    }
  }

  /**
   * @brief 指定区間のハッシュ値を取得します。
   * @param pos 取得開始位置。
   * @param length 取得する長さ。
   * @return 指定区間のハッシュ値。
   */
  [[nodiscard]] auto slice(size_t const pos, size_t const length) const -> uint64_t {
    return prefix_[pos + length] - prefix_[pos] * powers_[length];
  }

  /**
   * @brief 文字列全体のハッシュ値を計算します。
   * @param input ハッシュ対象の文字列。
   * @return 計算したハッシュ値。
   */
  static auto hash(std::basic_string_view<CharT> const input) -> uint64_t {
    auto value = uint64_t{0};
    for (auto const ch : input) {
      value = value * kBase + code_unit(ch);
    }
    return value;
  }

private:
  /**
   * @brief 文字をハッシュ計算用の整数値へ変換します。
   * @param value 変換対象の文字。
   * @return ハッシュ計算に利用する整数値。
   */
  static auto code_unit(CharT const value) -> uint64_t {
    if constexpr (std::is_same_v<CharT, char>) {
      return static_cast<uint64_t>(static_cast<unsigned char>(value)) + 1;
    } else {
      return static_cast<uint64_t>(value) + 1;
    }
  }

  static constexpr auto kBase = uint64_t{11400714819323198485ull};

  std::vector<uint64_t> prefix_;
  std::vector<uint64_t> powers_;
};

/**
 * @brief 重ならない出現位置を貪欲法で数えます。
 * @param positions 出現位置の配列。
 * @param length 対象部分文字列の長さ。
 * @return 重複しない出現回数。
 */
auto constexpr greedy_non_overlapping_count = [](std::vector<size_t> const& positions, size_t const length) {
  auto count = int64_t{0};
  auto next_allowed = size_t{0};
  auto first = true;
  for (auto const position : positions) {
    if (first || position >= next_allowed) {
      ++count;
      next_allowed = position + length;
      first = false;
    }
  }
  return count;
};

/**
 * @brief 文字列が ASCII のみで構成されるか判定します。
 * @param input 判定対象の文字列。
 * @return ASCII のみなら true、それ以外は false。
 */
auto constexpr is_ascii = [](std::string_view const input) {
  return std::ranges::all_of(input, [](char const ch) {
    return static_cast<unsigned char>(ch) < 0x80;
  });
};

/**
 * @brief ASCII 文字 1 バイトを URI エンコードした場合の長さを返します。
 * @param value 対象の ASCII バイト値。
 * @return URI エンコード後の長さ。
 */
auto constexpr ascii_uri_byte_length = [](unsigned char const value) -> int64_t {
  return is_uri_unescaped(static_cast<char32_t>(value)) ? 1 : 3;
};

auto const ascii_uri_lengths = [] {
  auto lengths = std::array<int64_t, 256>{};
  for (auto const index : std::views::iota(size_t{0}, lengths.size())) {
    lengths[index] = ascii_uri_byte_length(static_cast<unsigned char>(index));
  }
  return lengths;
}();

/**
 * @brief ASCII 文字列を URI エンコードした場合の長さを計算します。
 * @param input 対象の ASCII 文字列。
 * @return URI エンコード後の文字列長。
 */
auto const encoded_uri_length_ascii = [](std::string_view const input) {
  auto length = int64_t{0};
  for (auto const ch : input) {
    length += ascii_uri_lengths[static_cast<unsigned char>(ch)];
  }
  return length;
};

auto const replacement_characters_ascii = [] {
  auto characters = std::string{};
  auto const unescaped = std::string_view{"-_.!~*'()"};

  for (auto i = int{127}; --i;) {
    auto const c = static_cast<char>(i);
    if ((i >= 48 && i <= 57) ||
        (i >= 65 && i <= 90) ||
        (i >= 97 && i <= 122) ||
        unescaped.find(c) != std::string_view::npos) {
      characters.push_back(c);
    }
  }

  for (auto const i : std::views::iota(32, 255)) {
    auto const c = static_cast<char>(i);
    if (c != '\\' && characters.find(c) == std::string::npos) {
      characters.insert(characters.begin(), c);
    }
  }

  return characters;
}();

/**
 * @brief ASCII 文字列内の各文字の出現頻度を集計します。
 * @param input 集計対象の文字列。
 * @return 文字ごとの出現頻度テーブル。
 */
auto const character_frequencies_ascii = [](std::string_view const input) {
  auto frequencies = std::array<uint32_t, 256>{};
  for (auto const ch : input) {
    ++frequencies[static_cast<unsigned char>(ch)];
  }
  return frequencies;
};

/**
 * @brief ASCII 文字列から初期置換候補を生成します。
 * @param string 解析対象の文字列。
 * @param max_substring_length 探索する部分文字列の最大長。
 * @return 置換候補の一覧。
 */
auto const build_initial_ascii_candidates = [](std::string_view const string, int64_t const max_substring_length) {
  auto candidates = std::vector<OrderedCandidate<char>>{};
  if (string.size() < 2) {
    return candidates;
  }

  auto const hasher = RollingHash<char>{string};
  for (auto substring_length = size_t{2};
       substring_length < static_cast<size_t>(max_substring_length) && string.size() > substring_length;
       ++substring_length) {
    struct Group {
      size_t first_start;
      std::vector<size_t> positions;
    };

    auto groups = std::vector<Group>{};
    auto buckets = std::unordered_map<uint64_t, std::vector<size_t>>{};
    auto const last_start = string.size() - substring_length;
    buckets.reserve(last_start + 1);

    for (auto const start : std::views::iota(size_t{0}, last_start + 1)) {
      auto const hash = hasher.slice(start, substring_length);
      auto found = false;
      if (auto const it = buckets.find(hash); it != buckets.end()) {
        for (auto const group_index : it->second) {
          auto const first_start = groups[group_index].first_start;
          if (string.substr(start, substring_length) == string.substr(first_start, substring_length)) {
            groups[group_index].positions.push_back(start);
            found = true;
            break;
          }
        }
      }

      if (!found && start < last_start) {
        auto group = Group{start, {start}};
        groups.push_back(std::move(group));
        buckets[hash].push_back(groups.size() - 1);
      }
    }

    for (auto const& group : groups) {
      auto const count = greedy_non_overlapping_count(group.positions, substring_length);
      if (count <= 1) {
        continue;
      }

      auto const value = string.substr(group.first_start, substring_length);
      candidates.push_back(OrderedCandidate<char>{
        std::string{value},
        count,
        encoded_uri_length_ascii(value),
      });
    }
  }

  return candidates;
};

/**
 * @brief ASCII 置換候補の実際の出現回数を再評価します。
 * @param string 評価対象の文字列。
 * @param candidates 評価対象の候補一覧。
 * @return 出現回数を更新し、有効な候補のみを残した一覧。
 */
auto const count_ascii_candidates = [](std::string_view const string,
                                       std::vector<OrderedCandidate<char>> candidates) {
  if (candidates.empty()) {
    return candidates;
  }

  auto const hasher = RollingHash<char>{string};
  auto positions = std::vector<std::vector<size_t>>(candidates.size());
  auto indices_by_length = std::unordered_map<size_t, std::vector<size_t>>{};
  indices_by_length.reserve(candidates.size());

  for (auto const index : std::views::iota(size_t{0}, candidates.size())) {
    indices_by_length[candidates[index].value.size()].push_back(index);
  }

  for (auto const& [substring_length, indices] : indices_by_length) {
    if (substring_length == 0 || string.size() < substring_length) {
      continue;
    }

    auto buckets = std::unordered_map<uint64_t, std::vector<size_t>>{};
    buckets.reserve(indices.size());
    for (auto const index : indices) {
      buckets[RollingHash<char>::hash(candidates[index].value)].push_back(index);
    }

    auto const last_start = string.size() - substring_length;
    for (auto const start : std::views::iota(size_t{0}, last_start + 1)) {
      auto const hash = hasher.slice(start, substring_length);
      if (auto const it = buckets.find(hash); it != buckets.end()) {
        for (auto const candidate_index : it->second) {
          auto const candidate_view = std::string_view{candidates[candidate_index].value};
          if (string.substr(start, substring_length) == candidate_view) {
            positions[candidate_index].push_back(start);
            break;
          }
        }
      }
    }
  }

  auto filtered = std::vector<OrderedCandidate<char>>{};
  filtered.reserve(candidates.size());
  for (auto const index : std::views::iota(size_t{0}, candidates.size())) {
    auto const count = greedy_non_overlapping_count(positions[index], candidates[index].value.size());
    if (count <= 1) {
      continue;
    }
    candidates[index].count = count;
    filtered.push_back(std::move(candidates[index]));
  }
  return filtered;
};

/**
 * @brief ASCII 文字列に対して JSONCrush 互換の置換圧縮を行います。
 * @param string 圧縮対象の文字列。
 * @param max_substring_length 探索する部分文字列の最大長。
 * @return 圧縮結果と置換テーブル。
 */
auto const js_crush_ascii = [](std::string string, int64_t const max_substring_length = 50) {
  auto split_string = std::string{};
  auto candidates = build_initial_ascii_candidates(string, max_substring_length);
  auto replace_character_pos = replacement_characters_ascii.size();
  auto frequencies = character_frequencies_ascii(string);

  while (true) {
    auto has_replace_character = false;
    auto replace_character = char{};
    while (replace_character_pos > 0) {
      --replace_character_pos;
      auto const candidate = replacement_characters_ascii[replace_character_pos];
      if (frequencies[static_cast<unsigned char>(candidate)] == 0) {
        replace_character = candidate;
        has_replace_character = true;
        break;
      }
    }
    if (!has_replace_character) {
      break;
    }

    auto positive_candidates = std::vector<OrderedCandidate<char>>{};
    positive_candidates.reserve(candidates.size());
    auto best_index = size_t{0};
    auto best_length_delta = int64_t{0};
    auto const replace_length = ascii_uri_lengths[static_cast<unsigned char>(replace_character)];

    for (auto const& candidate : candidates) {
      auto length_delta = (candidate.count - 1) * candidate.encoded_length - (candidate.count + 1) * replace_length;
      if (split_string.empty()) {
        length_delta -= ascii_uri_lengths[static_cast<unsigned char>(JSON_CRUSH_DELIMITER_ASCII)];
      }
      if (length_delta <= 0) {
        continue;
      }
      if (length_delta > best_length_delta) {
        best_length_delta = length_delta;
        best_index = positive_candidates.size();
      }
      positive_candidates.push_back(candidate);
    }

    if (best_length_delta <= 0) {
      break;
    }

    candidates = std::move(positive_candidates);
    auto const best_substring = candidates[best_index].value;
    string = replace_all_with_char(std::string_view{string.data(), string.size()},
                                   std::string_view{best_substring.data(), best_substring.size()},
                                   replace_character);
    string.push_back(replace_character);
    string += best_substring;
    split_string.insert(split_string.begin(), replace_character);
    frequencies = character_frequencies_ascii(string);

    auto rewritten_unique = std::vector<OrderedCandidate<char>>{};
    rewritten_unique.reserve(candidates.size());
    auto dedupe = std::unordered_map<uint64_t, std::vector<size_t>>{};
    dedupe.reserve(candidates.size());

    for (auto const& candidate : candidates) {
      auto rewritten = replace_all_with_char(std::string_view{candidate.value.data(), candidate.value.size()},
                                             std::string_view{best_substring.data(), best_substring.size()},
                                             replace_character);
      auto const rewritten_view = std::string_view{rewritten.data(), rewritten.size()};
      auto const hash = RollingHash<char>::hash(rewritten_view);
      auto seen = false;
      if (auto const it = dedupe.find(hash); it != dedupe.end()) {
        for (auto const index : it->second) {
          if (rewritten_unique[index].value == rewritten) {
            seen = true;
            break;
          }
        }
      }
      if (seen) {
        continue;
      }

      auto const rewritten_length = encoded_uri_length_ascii(rewritten_view);
      dedupe[hash].push_back(rewritten_unique.size());
      rewritten_unique.push_back(OrderedCandidate<char>{
        std::move(rewritten),
        0,
        rewritten_length,
      });
    }

    candidates = count_ascii_candidates(string, std::move(rewritten_unique));
  }

  return JSCrushResult<char>{std::move(string), std::move(split_string)};
};

template <typename CharT>
/**
 * @brief 部分文字列の非重複出現回数を `find` ベースで数えます。
 * @tparam CharT 文字列の文字型。
 * @param input 検索対象の文字列。
 * @param substring 数える部分文字列。
 * @return 非重複の出現回数。
 */
auto count_occurrences_with_find(std::basic_string_view<CharT> const input,
                                 std::basic_string_view<CharT> const substring) -> int64_t {
  if (substring.empty()) {
    return 0;
  }

  auto count = int64_t{0};
  for (auto position = input.find(substring);
       position != std::basic_string_view<CharT>::npos;
       position = input.find(substring, position + substring.size())) {
    ++count;
  }
  return count;
}

auto const replacement_characters_utf16 = [] {
  auto characters = std::u16string{};
  auto const unescaped = std::u16string_view{u"-_.!~*'()"};

  for (auto i = int{127}; --i;) {
    auto const c = static_cast<char16_t>(i);
    if ((i >= 48 && i <= 57) ||
        (i >= 65 && i <= 90) ||
        (i >= 97 && i <= 122) ||
        unescaped.find(c) != std::u16string_view::npos) {
      characters.push_back(c);
    }
  }

  for (auto const i : std::views::iota(32, 255)) {
    auto const c = static_cast<char16_t>(i);
    if (c != u'\\' && characters.find(c) == std::u16string::npos) {
      characters.insert(characters.begin(), c);
    }
  }

  return characters;
}();

/**
 * @brief UTF-16 文字列から初期置換候補を生成します。
 * @param string 解析対象の UTF-16 文字列。
 * @param max_substring_length 探索する部分文字列の最大長。
 * @return 置換候補の一覧。
 */
auto const build_initial_utf16_candidates = [](std::u16string_view const string, int64_t const max_substring_length) {
  auto candidates = std::vector<OrderedCandidate<char16_t>>{};

  if (string.size() < 2) {
    return candidates;
  }

  auto const hasher = RollingHash<char16_t>{string};
  for (auto substring_length = size_t{2};
       substring_length < static_cast<size_t>(max_substring_length) && string.size() > substring_length;
       ++substring_length) {
    struct Group {
      size_t first_start;
      std::vector<size_t> positions;
    };

    auto groups = std::vector<Group>{};
    auto buckets = std::unordered_map<uint64_t, std::vector<size_t>>{};
    auto const last_start = string.size() - substring_length;
    buckets.reserve(last_start + 1);

    for (auto const start : std::views::iota(size_t{0}, last_start + 1)) {
      auto const substring = string.substr(start, substring_length);
      auto const hash = hasher.slice(start, substring_length);
      auto found = false;
      if (auto const it = buckets.find(hash); it != buckets.end()) {
        for (auto const group_index : it->second) {
          auto const first_start = groups[group_index].first_start;
          if (string.substr(first_start, substring_length) == substring) {
            groups[group_index].positions.push_back(start);
            found = true;
            break;
          }
        }
      }

      if (!found && start < last_start && !has_unmatched_surrogate(substring)) {
        groups.push_back(Group{start, {start}});
        buckets[hash].push_back(groups.size() - 1);
      }
    }

    for (auto const& group : groups) {
      auto const count = greedy_non_overlapping_count(group.positions, substring_length);
      if (count <= 1) {
        continue;
      }

      auto const value = string.substr(group.first_start, substring_length);
      candidates.push_back(OrderedCandidate<char16_t>{
        std::u16string{value},
        count,
        encoded_uri_length(value),
      });
    }
  }

  return candidates;
};

/**
 * @brief UTF-16 置換候補の実際の出現回数を再評価します。
 * @param string 評価対象の UTF-16 文字列。
 * @param candidates 評価対象の候補一覧。
 * @return 出現回数を更新し、有効な候補のみを残した一覧。
 */
auto const count_utf16_candidates = [](std::u16string_view const string,
                                       std::vector<OrderedCandidate<char16_t>> candidates) {
  if (candidates.empty()) {
    return candidates;
  }

  auto const hasher = RollingHash<char16_t>{string};
  auto positions = std::vector<std::vector<size_t>>(candidates.size());
  auto indices_by_length = std::unordered_map<size_t, std::vector<size_t>>{};
  indices_by_length.reserve(candidates.size());

  for (auto const index : std::views::iota(size_t{0}, candidates.size())) {
    indices_by_length[candidates[index].value.size()].push_back(index);
  }

  for (auto const& [substring_length, indices] : indices_by_length) {
    if (substring_length == 0 || string.size() < substring_length) {
      continue;
    }

    auto buckets = std::unordered_map<uint64_t, std::vector<size_t>>{};
    buckets.reserve(indices.size());
    for (auto const index : indices) {
      buckets[RollingHash<char16_t>::hash(std::u16string_view{candidates[index].value.data(),
                                                              candidates[index].value.size()})].push_back(index);
    }

    auto const last_start = string.size() - substring_length;
    for (auto const start : std::views::iota(size_t{0}, last_start + 1)) {
      auto const hash = hasher.slice(start, substring_length);
      if (auto const it = buckets.find(hash); it != buckets.end()) {
        for (auto const candidate_index : it->second) {
          auto const candidate_view = std::u16string_view{candidates[candidate_index].value.data(),
                                                          candidates[candidate_index].value.size()};
          if (string.substr(start, substring_length) == candidate_view) {
            positions[candidate_index].push_back(start);
            break;
          }
        }
      }
    }
  }

  auto filtered = std::vector<OrderedCandidate<char16_t>>{};
  filtered.reserve(candidates.size());
  for (auto const index : std::views::iota(size_t{0}, candidates.size())) {
    auto const count = greedy_non_overlapping_count(positions[index], candidates[index].value.size());
    if (count <= 1) {
      continue;
    }
    candidates[index].count = count;
    filtered.push_back(std::move(candidates[index]));
  }
  return filtered;
};

/**
 * @brief UTF-16 文字列に対して JSONCrush 互換の置換圧縮を行います。
 * @param string 圧縮対象の UTF-16 文字列。
 * @param max_substring_length 探索する部分文字列の最大長。
 * @return 圧縮結果と置換テーブル。
 */
auto const js_crush_utf16 = [](std::u16string string, int64_t const max_substring_length = 50) {
  auto split_string = std::u16string{};
  auto candidates = build_initial_utf16_candidates(string, max_substring_length);
  auto replace_character_pos = replacement_characters_utf16.size();

  while (true) {
    auto has_replace_character = false;
    auto replace_character = char16_t{};
    while (replace_character_pos > 0) {
      --replace_character_pos;
      if (string.find(replacement_characters_utf16[replace_character_pos]) == std::u16string::npos) {
        replace_character = replacement_characters_utf16[replace_character_pos];
        has_replace_character = true;
        break;
      }
    }
    if (!has_replace_character) {
      break;
    }

    auto positive_candidates = std::vector<OrderedCandidate<char16_t>>{};
    positive_candidates.reserve(candidates.size());
    auto best_index = size_t{0};
    auto best_length_delta = int64_t{0};
    auto const replace_length = encoded_uri_length(std::u16string_view{&replace_character, 1});

    for (auto const& candidate : candidates) {
      auto length_delta = (candidate.count - 1) * candidate.encoded_length - (candidate.count + 1) * replace_length;
      if (split_string.empty()) {
        length_delta -= encoded_uri_length(std::u16string_view{&JSON_CRUSH_DELIMITER, 1});
      }
      if (length_delta <= 0) {
        continue;
      }
      if (length_delta > best_length_delta) {
        best_length_delta = length_delta;
        best_index = positive_candidates.size();
      }
      positive_candidates.push_back(candidate);
    }

    if (best_length_delta <= 0) {
      break;
    }

    candidates = std::move(positive_candidates);
    auto const best_substring = candidates[best_index].value;
    string = replace_all_with_char(std::u16string_view{string.data(), string.size()},
                                   std::u16string_view{best_substring.data(), best_substring.size()},
                                   replace_character);
    string.push_back(replace_character);
    string += best_substring;
    split_string.insert(split_string.begin(), replace_character);

    auto rewritten_unique = std::vector<OrderedCandidate<char16_t>>{};
    rewritten_unique.reserve(candidates.size());
    auto dedupe = std::unordered_map<uint64_t, std::vector<size_t>>{};
    dedupe.reserve(candidates.size());

    for (auto const& candidate : candidates) {
      auto rewritten = replace_all_with_char(std::u16string_view{candidate.value.data(), candidate.value.size()},
                                             std::u16string_view{best_substring.data(), best_substring.size()},
                                             replace_character);
      auto const rewritten_view = std::u16string_view{rewritten.data(), rewritten.size()};
      auto const hash = RollingHash<char16_t>::hash(rewritten_view);
      auto seen = false;
      if (auto const it = dedupe.find(hash); it != dedupe.end()) {
        for (auto const index : it->second) {
          if (rewritten_unique[index].value == rewritten) {
            seen = true;
            break;
          }
        }
      }
      if (seen) {
        continue;
      }

      auto const rewritten_length = encoded_uri_length(rewritten_view);
      dedupe[hash].push_back(rewritten_unique.size());
      rewritten_unique.push_back(OrderedCandidate<char16_t>{
        std::move(rewritten),
        0,
        rewritten_length,
      });
    }

    candidates = count_utf16_candidates(std::u16string_view{string.data(), string.size()},
                                        std::move(rewritten_unique));
  }

  return JSCrushResult<char16_t>{std::move(string), std::move(split_string)};
};

/**
 * @brief ASCII 向けの JSONCrush 圧縮文字列を復元します。
 * @param input 復元対象の圧縮文字列。
 * @return 復元後の ASCII 文字列。
 */
auto const uncrush_ascii = [](std::string_view const input) {
  auto string = std::string{input};
  if (!string.empty()) {
    string.pop_back();
  }

  auto parts = split_on_char(std::string_view{string}, JSON_CRUSH_DELIMITER_ASCII);
  auto uncrushed = parts.empty() ? std::string{} : parts.front();
  if (parts.size() > 1) {
    auto const split = parts[1];
    for (auto const replacement : split) {
      auto split_array = split_on_char(std::string_view{uncrushed}, replacement);
      auto const original = split_array.empty() ? std::string{} : split_array.back();
      if (!split_array.empty()) {
        split_array.pop_back();
      }
      uncrushed = join_strings(split_array, std::string_view{original});
    }
  }

  return json_crush_swap_ascii(uncrushed, false);
};

} // namespace detail

/**
 * @brief JSONCrush互換の文字列圧縮を行います。
 * @param input 圧縮対象の文字列。
 * @return JSONCrush.crush() 互換の圧縮文字列。
 * @throw std::runtime_error 入力が不正なUTF-8の場合。
 */
auto crush(std::string_view input) -> std::string {
  if (detail::is_ascii(input)) {
    auto string = detail::ascii_to_utf16(input);
    string.erase(std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER), string.end());
    string = detail::json_crush_swap(string);

    auto crushed = detail::js_crush_utf16(std::move(string));
    auto output = std::move(crushed.crushed);
    if (!crushed.split.empty()) {
      output.push_back(detail::JSON_CRUSH_DELIMITER);
      output += crushed.split;
    }
    output.push_back(u'_');
    return detail::utf16_to_utf8(output);
  }

  auto string = detail::utf8_to_utf16(input);
  string.erase(std::remove(string.begin(), string.end(), detail::JSON_CRUSH_DELIMITER), string.end());
  string = detail::json_crush_swap(string);

  auto crushed = detail::js_crush_utf16(std::move(string));
  auto output = std::move(crushed.crushed);
  if (!crushed.split.empty()) {
    output.push_back(detail::JSON_CRUSH_DELIMITER);
    output += crushed.split;
  }
  output.push_back(u'_');
  return detail::utf16_to_utf8(output);
}

/**
 * @brief JSONCrush互換の圧縮文字列を元に戻します。
 * @param input JSONCrush.crush() 互換の圧縮文字列。
 * @return 復元された文字列。
 * @throw std::runtime_error 入力が不正なUTF-8の場合。
 */
auto uncrush(std::string_view input) -> std::string {
  if (detail::is_ascii(input)) {
    return detail::uncrush_ascii(input);
  }

  auto string = detail::utf8_to_utf16(input);
  if (!string.empty()) {
    string.pop_back();
  }

  auto parts = detail::split_on_char(std::u16string_view{string}, detail::JSON_CRUSH_DELIMITER);
  auto uncrushed = parts.empty() ? std::u16string{} : parts.front();
  if (parts.size() > 1) {
    auto const split = parts[1];
    for (auto const replacement : split) {
      auto split_array = detail::split_on_char(std::u16string_view{uncrushed}, replacement);
      auto const original = split_array.empty() ? std::u16string{} : split_array.back();
      if (!split_array.empty()) {
        split_array.pop_back();
      }
      uncrushed = detail::join_strings(split_array, std::u16string_view{original});
    }
  }

  return detail::utf16_to_utf8(detail::json_crush_swap(uncrushed, false));
}

} // namespace yase_json

#endif // __YASE_JSON_CRUSH_HPP__
