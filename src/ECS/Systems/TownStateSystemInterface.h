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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownDesire.h"

namespace openblack::ecs::systems
{
/// The towns' shared lists: the belief sprites waiting to be shown, the villagers without a town, and the alignment
/// turns a caller gets for a town that does not exist (town_belief, town_villagers and town_desire go through it)
class TownStateSystemInterface
{
public:
	virtual ~TownStateSystemInterface() = default;

	/// Last in, first out
	[[nodiscard]] virtual std::vector<town_belief::BeliefSprite>& BeliefSprites() = 0;
	/// The newest first
	[[nodiscard]] virtual std::vector<entt::entity>& Vagrants() = 0;
	/// What AlignmentTurns of no town hands out; its callers may write into it
	[[nodiscard]] virtual std::array<int32_t, town_desire::k_Count>& NoTownAlignmentTurns() = 0;
};
} // namespace openblack::ecs::systems
