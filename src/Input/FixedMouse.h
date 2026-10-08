/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdio>
#include <cstdlib>

#include <optional>

#include <glm/vec2.hpp>

namespace openblack::input
{

/// (openblack, test runs) OPENBLACK_FIXED_MOUSE=x,y: the mouse stays at that window position and the real mouse is
/// ignored (its events are not dispatched, Game::Update), so runs with a fixed frame clock (Debug/FixedClock) draw the
/// same hand and its light; read once. Unset: the real mouse, as always
inline const std::optional<glm::ivec2>& FixedMouse()
{
	static const std::optional<glm::ivec2> s_Fixed = []() -> std::optional<glm::ivec2> {
		const char* value = std::getenv("OPENBLACK_FIXED_MOUSE");
		int x = 0;
		int y = 0;
		if (value == nullptr || std::sscanf(value, "%d,%d", &x, &y) != 2)
		{
			return std::nullopt;
		}
		return glm::ivec2(x, y);
	}();
	return s_Fixed;
}

} // namespace openblack::input
