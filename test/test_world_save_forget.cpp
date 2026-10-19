/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CreatureMind.h"
#include "ECS/WorldSave.h"

using namespace openblack;
using openblack::ecs::components::CreatureMindState;

// The game does not keep the actions that the area that held a creature refused: after a load the creature wants them
// again
TEST(WorldSaveForget, ALoadForgetsTheRefusedWalks)
{
	entt::registry registry;
	const auto creature = registry.create();
	auto& mind = registry.emplace<CreatureMindState>(creature);
	mind.leftAreaActions = std::vector<bool> {false, true, true};
	mind.agendaRefused = true;
	ecs::world_save::ForgetUnsavedState(registry);
	EXPECT_TRUE(registry.get<CreatureMindState>(creature).leftAreaActions.empty());
	EXPECT_FALSE(registry.get<CreatureMindState>(creature).agendaRefused);
}
