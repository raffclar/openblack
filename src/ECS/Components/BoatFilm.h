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

#include <glm/mat4x4.hpp>

namespace openblack::ecs::components
{

/// A thing in the explorers' boat film: the boat, a sailor pushing it, or one of the crew and cargo on its deck. The
/// film makes and moves them, and they go with it.
struct BoatFilmPart
{
	enum class Role : uint8_t
	{
		Boat,
		LaunchSailor,
		DeckPart,
	};
	Role role {Role::Boat};
	/// Which sailor or deck part it is
	uint8_t index {0};
	/// Its place in its clip, in milliseconds; none moves without a clip
	int32_t clipTime {0};
	/// It casts a shadow from the far sun onto the land and the things on it: the boat while it is pushed down the
	/// beach
	bool castsShadow {false};
};

/// A thing drawn by a whole matrix rather than its transform's parts, which a matrix blended between two keyframes of a
/// clip needs: its axes may lean and stretch a little away from square
struct DrawnMatrix
{
	glm::mat4 model {1.0f};
};

} // namespace openblack::ecs::components
