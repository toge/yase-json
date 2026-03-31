#ifndef __YASE_JSON_COMPRESS_HPP__
#define __YASE_JSON_COMPRESS_HPP__

#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <ranges>
#include <algorithm>

#include <glaze/glaze.hpp>

namespace yase_json {
  auto const BASE62_CHARS = std::string_view{"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};

  // 高速なBase62エンコード
  auto const to_base62 = [](uint64_t val) -> std::string {
    if (val == 0) {
      return "0";
    }
    char buf[12];
    int i = 12;
    while (val > 0) {
      buf[--i] = BASE62_CHARS[val % 62];
      val /= 62;
    }
    return std::string(buf + i, 12 - i);
  };

  class Compressor {
  public:
    auto compress(glz::generic const& data) -> glz::generic {
      pool.clear();
      lookup.clear();
      auto root = process(data);

      auto result = glz::generic::array_t{};
      result.emplace_back(std::move(pool));
      result.emplace_back(std::move(root));

      auto final_node = glz::generic{};
      final_node = std::move(result);
      return final_node;
    }

  private:
    glz::generic::array_t pool;
    std::unordered_map<std::string, std::string> lookup;

    auto process(glz::generic const& v) -> std::string {
      auto lookup_key = std::string{};
      glz::write_json(v, lookup_key);

      if (auto const it = lookup.find(lookup_key); it != lookup.end()) {
        return it->second;
      }

      if (v.is_object()) {
        auto compressed_obj = glz::generic::object_t{};
        for (auto const& [k, val] : v.get<glz::generic::object_t>()) {
          compressed_obj.emplace(k, process(val));
        }
        auto const idx = to_base62(pool.size());
        auto node = glz::generic{};
        node = std::move(compressed_obj);
        pool.emplace_back(std::move(node));
        lookup[lookup_key] = idx;
        return idx;
      } else if (v.is_array()) {
        auto compressed_arr = glz::generic::array_t{};
        for (auto const& item : v.get<glz::generic::array_t>()) {
          compressed_arr.emplace_back(process(item));
        }
        auto const idx = to_base62(pool.size());
        auto node = glz::generic{};
        node = std::move(compressed_arr);
        pool.emplace_back(std::move(node));
        lookup[lookup_key] = idx;
        return idx;
      } else {
        auto const idx = to_base62(pool.size());
        pool.emplace_back(v);
        lookup[lookup_key] = idx;
        return idx;
      }
    }
  };
} // namespace yase_json

#endif // __YASE_JSON_COMPRESS_HPP__
