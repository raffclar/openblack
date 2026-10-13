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

namespace openblack::ecs::components
{

/// A villager inspecting a creature: how far from it the villager means to stand, and the turns it still looks before
/// stepping closer
struct VillagerCreatureInspection
{
	float keep {0.0f};
	uint32_t lookTurns {0};
};

} // namespace openblack::ecs::components
