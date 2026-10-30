/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

namespace openblack::gui
{

/// The size of the space of the dialogs: 800 by 600
inline constexpr glm::ivec2 k_DialogSize {800, 600};

/// A rectangle in the dialogs' 800 by 600 space, from min up to but not including max
struct DialogRect
{
	glm::ivec2 min;
	glm::ivec2 max;

	[[nodiscard]] bool Contains(glm::ivec2 point) const noexcept
	{
		return point.x >= min.x && point.y >= min.y && point.x < max.x && point.y < max.y;
	}
	[[nodiscard]] glm::ivec2 Centre() const noexcept { return (min + max) / 2; }
	[[nodiscard]] int Width() const noexcept { return max.x - min.x; }
	[[nodiscard]] int Height() const noexcept { return max.y - min.y; }
	[[nodiscard]] bool operator==(const DialogRect&) const = default;
};

} // namespace openblack::gui
