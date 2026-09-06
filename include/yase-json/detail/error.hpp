#pragma once

#include <expected>
#include <string>
#include <utility>

namespace yase_json::detail {

struct error {
  std::string message;
};

template <typename T>
using result = std::expected<T, error>;

inline auto err(std::string message) -> std::unexpected<error> {
  return std::unexpected{error{std::move(message)}};
}

} // namespace yase_json::detail
