/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack
{

/// The kind of place where the game runs. Settings that must differ in a web browser, such as the default key
/// bindings, select their values with k_Platform.
enum class Platform : uint8_t
{
	Desktop,
	/// A web browser. The browser keeps some keys and the pointer for itself.
	Web,
};

#if defined(__EMSCRIPTEN__)
inline constexpr Platform k_Platform = Platform::Web;
#else
inline constexpr Platform k_Platform = Platform::Desktop;
#endif

} // namespace openblack
