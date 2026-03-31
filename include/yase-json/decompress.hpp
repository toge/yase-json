#ifndef __YASE_JSON_DECOMPRESS_HPP__
#define __YASE_JSON_DECOMPRESS_HPP__

#include <stdexcept>
#include <array>

#include <immintrin.h>

#include <glaze/glaze.hpp>

namespace yase_json {

namespace detail {

// Base62 デコード用高速ルックアップテーブル
static constexpr auto decode_table = [] {
  std::array<uint8_t, 256> t{};
  for (size_t i = 0; i < 256; ++i) t[i] = 0;
  auto const chars = std::string_view{"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
  for (size_t i = 0; i < 62; ++i) {
    t[static_cast<uint8_t>(chars[i])] = static_cast<uint8_t>(i);
  }
  return t;
}();

// 高速なBase62デコード
auto const from_base62 = [](std::string_view s) -> uint64_t {
  if (s.empty()) {
    return 0;
  }
  uint64_t res = 0;
  // 短い文字列が多いため、テーブルルックアップがSIMDよりもオーバーヘッドが少なく高速
  for (auto const c : s) {
    res = res * 62 + decode_table[static_cast<uint8_t>(c)];
  }
  return res;
};

} // namespace detail

class Decompressor {
public:
  auto decompress(glz::generic const& compressed) -> glz::generic {
    if (!compressed.is_array()) {
      throw std::runtime_error("Root must be array");
    }
    auto const& arr = compressed.get<glz::generic::array_t>();
    if (arr.size() < 2) {
      throw std::runtime_error("Root must have 2 elements");
    }

    pool_ptr = &arr[0].get<glz::generic::array_t>();
    auto const& root_idx = arr[1].get<std::string>();
    return resolve(root_idx);
  }

private:
  glz::generic::array_t const* pool_ptr = nullptr;

  auto resolve(std::string_view idx_str) -> glz::generic {
    auto const idx = detail::from_base62(idx_str);
    if (idx >= pool_ptr->size()) {
      throw std::out_of_range("idx out of pool range");
    }
    auto const& val = pool_ptr->at(idx);

    if (val.is_object()) {
      auto res = glz::generic::object_t{};
      for (auto const& [key, v_idx_node] : val.get<glz::generic::object_t>()) {
        res.emplace(key, resolve(v_idx_node.get<std::string>()));
      }
      auto node = glz::generic{};
      node = std::move(res);
      return node;
    } else if (val.is_array()) {
      auto res = glz::generic::array_t{};
      auto const& src = val.get<glz::generic::array_t>();
      res.reserve(src.size());
      for (auto const& item_idx_node : src) {
        res.emplace_back(resolve(item_idx_node.get<std::string>()));
      }
      auto node = glz::generic{};
      node = std::move(res);
      return node;
    }
    return val;
  }
};

} // namespace yase_json

#endif // __YASE_JSON_DECOMPRESS_HPP__
