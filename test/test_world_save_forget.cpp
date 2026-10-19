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
#include "ECS/Components/Temple.h"
#include "ECS/Components/Villager.h"
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

// A save from the save room has the rooms of the temple. The load removes them and keeps every thing of the land.
TEST(WorldSaveForget, ALoadRemovesTheRoomsOfTheTempleAndKeepsTheLand)
{
	entt::registry registry;
	std::vector<entt::entity> villagers;
	for (int i = 0; i < 58; ++i)
	{
		villagers.push_back(registry.create());
		registry.emplace<ecs::components::Villager>(villagers.back());
	}
	for (int i = 0; i < 810; ++i)
	{
		registry.emplace<ecs::components::TempleInteriorPart>(registry.create(), TempleRoom::Main);
	}
	ecs::world_save::ForgetUnsavedState(registry);
	EXPECT_TRUE(registry.view<ecs::components::TempleInteriorPart>().empty());
	EXPECT_EQ(registry.view<ecs::components::Villager>().size(), villagers.size());
	for (const auto villager : villagers)
	{
		EXPECT_TRUE(registry.valid(villager));
	}
}

// Inside the temple the camera of the world stays where the player left it. A save keeps that camera, so a load puts
// the player back outside the temple.
TEST(WorldSaveForget, ASaveInsideTheTempleKeepsTheCameraOfTheWorld)
{
	const OutsideCamera temple {.origin = {10.0f, 2.0f, 10.0f}, .focus = {11.0f, 2.0f, 10.0f}};
	const OutsideCamera world {.origin = {1900.0f, 60.0f, 2500.0f}, .focus = {1910.0f, 30.0f, 2520.0f}};
	const auto inside = ecs::world_save::CameraToSave(world, temple);
	EXPECT_EQ(inside.origin, world.origin);
	EXPECT_EQ(inside.focus, world.focus);
	const auto outside = ecs::world_save::CameraToSave(std::nullopt, world);
	EXPECT_EQ(outside.origin, world.origin);
	EXPECT_EQ(outside.focus, world.focus);
}
