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
#include <cstdlib>
#include <cstring>

#include <SDL_events.h>

namespace openblack::input
{
/// The SDL events that come from the real mouse: its motion, its buttons and its wheel
[[nodiscard]] constexpr bool IsMouseEvent(uint32_t type)
{
	return type == SDL_MOUSEMOTION || type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP || type == SDL_MOUSEWHEEL;
}

/// A real mouse event is not dispatched while the mouse is fixed (OPENBLACK_FIXED_MOUSE) or the real input is ignored
/// (OPENBLACK_IGNORE_REAL_INPUT); every other event always is
[[nodiscard]] constexpr bool DropRealEvent(uint32_t type, bool mouseFixed, bool ignoreRealInput)
{
	return (mouseFixed || ignoreRealInput) && IsMouseEvent(type);
}

/// (openblack, test runs) OPENBLACK_IGNORE_REAL_INPUT=1: the real mouse is ignored, so a run is the same whatever the
/// person at the machine does with it. The cursor is then only OPENBLACK_MOUSE_AT's or a hand demo's. Read once;
/// unset, the real mouse is read as always
[[nodiscard]] inline bool IgnoreRealInput()
{
	static const bool k_Ignore = [] {
		const char* value = std::getenv("OPENBLACK_IGNORE_REAL_INPUT");
		return value != nullptr && std::strcmp(value, "1") == 0;
	}();
	return k_Ignore;
}
} // namespace openblack::input
