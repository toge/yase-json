#pragma once

#include <cstdint>
#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace yase_json::detail {

struct error {
  enum class kind_t : uint8_t { runtime_error, out_of_range, invalid_argument };
  kind_t kind{kind_t::runtime_error};
  std::string message;
};

template <typename T>
using result = std::expected<T, error>;

inline auto err(std::string message, error::kind_t const kind = error::kind_t::runtime_error)
  -> std::unexpected<error> {
  return std::unexpected{error{kind, std::move(message)}};
}

#if __cpp_exceptions

[[noreturn]] inline auto throw_error(error const& e) -> void {
  switch (e.kind) {
    case error::kind_t::out_of_range:
      throw std::out_of_range(e.message);
    case error::kind_t::invalid_argument:
      throw std::invalid_argument(e.message);
    default:
      throw std::runtime_error(e.message);
  }
}

template <typename T>
auto unwrap(result<T>&& r) -> T {
  if (!r) {
    throw_error(r.error());
  }
  return std::move(*r);
}

#endif

} // namespace yase_json::detail
