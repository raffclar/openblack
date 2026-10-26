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

#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "3D/AllMeshes.h"

namespace openblack::ecs::components
{

/// The soul of a villager that has just died: a white, see-through copy of the villager rising to heaven or sinking to
/// hell from where its body lies, drawn with its clip by the game's clock until the clip is nearly done. Its place is
/// its Transform; it is only a picture.
struct RisingSoul
{
	/// The villager's own model, a child's for a child
	entt::id_type mesh {0};
	AnimId clip {AnimId::PDead1GotoHeaven};
	/// How far into its clip the frames have drawn it, in milliseconds of the game's clock. Only the draw reads it.
	uint32_t timeMs {0};
	uint8_t alpha {0};
	/// Its bones, posed from its clip as it was last moved on. Empty once the frames have drawn its clip to its end.
	std::vector<glm::mat4> bones;
	/// The game turns since it rose. The turns remove it by this count, after the frames have drawn it to its end.
	uint32_t turns {0};
	/// The frames have drawn its clip to its end, and do not draw it again
	bool shownOut {false};
};

} // namespace openblack::ecs::components
