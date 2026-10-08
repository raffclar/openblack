/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
/// A shark: the Whale of the original (mobile object info 24), kept in the game's whale list. It has no AI of its own:
/// the script's WALK_PATH moves it (ECS/MobileWalkPaths.h).
struct Shark
{
	/// The position at the start of the turn (copied every turn)
	glm::vec3 turnStart {0.0f};
	/// The drawn heading (the Y angle of the turn's move, kept while it stands still). The constructor writes the
	/// creation angle there, then the creation clears it to 0.
	float heading {0.0f};
};

} // namespace openblack::ecs::components
