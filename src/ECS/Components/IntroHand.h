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

#include <entt/core/fwd.hpp>

namespace openblack::ecs::components
{

/// The god's hand of the opening, which lifts the boy out of the sea and sets him down beside his parents. It is drawn
/// posed by its clip, like the scenery the scripts open and close.
struct IntroHand
{
	/// The clip it plays, in the animation cache
	entt::id_type clip {0};
	/// Its place in the clip, in milliseconds
	int32_t time {0};
	float yaw {0.0f};
	float scale {1.0f};
};

} // namespace openblack::ecs::components
