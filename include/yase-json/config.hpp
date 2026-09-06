#pragma once

/**
 * @file yase-json/config.hpp
 * @brief Library configuration.
 *
 * This library is fully exception-free. All operations that can fail return
 * std::expected<T, detail::error>. No throwing APIs are provided.
 * The library can be built with -fno-exceptions.
 */
