/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TownStateSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class TownStateSystem final: public TownStateSystemInterface
{
public:
	[[nodiscard]] std::vector<town_belief::BeliefSprite>& BeliefSprites() override { return _beliefSprites; }
	[[nodiscard]] std::vector<entt::entity>& Vagrants() override { return _vagrants; }
	[[nodiscard]] std::array<int32_t, town_desire::k_Count>& NoTownAlignmentTurns() override { return _noTownAlignmentTurns; }

private:
	std::vector<town_belief::BeliefSprite> _beliefSprites;
	std::vector<entt::entity> _vagrants;
	std::array<int32_t, town_desire::k_Count> _noTownAlignmentTurns {};
};
} // namespace openblack::ecs::systems
