/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <vector>

#include <DanceFile.h>
#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/CloudArchetype.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Villager.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/Registry.h"
#include "ECS/WorldSave.h"

using namespace openblack;
using openblack::ecs::components::ComputerPlayer;
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

// A save from an older version keeps the sky's clouds as things of the land. The load removes them and keeps every
// other thing of the land.
TEST(WorldSaveForget, ALoadRemovesTheSkysCloudsOfAnOlderSave)
{
	entt::registry registry;
	const auto villager = registry.create();
	registry.emplace<ecs::components::Villager>(villager);
	for (int i = 0; i < 3; ++i)
	{
		registry.emplace<ecs::components::Cloud>(registry.create());
	}
	ecs::world_save::RemoveSavedSkyClouds(registry);
	EXPECT_TRUE(registry.view<ecs::components::Cloud>().empty());
	EXPECT_TRUE(registry.valid(villager));
}

// A load of a saved game removes the things of the drawn registry, but the sky's clouds that the land laid out stay
TEST(WorldSaveForget, ALoadKeepsTheSkysCloudsOfTheDrawnRegistry)
{
	ecs::Registry drawn;
	const auto cloud = drawn.Create();
	drawn.Assign<ecs::components::Cloud>(cloud);
	const auto mark = drawn.Create();
	drawn.Assign<ecs::components::Villager>(mark);
	ecs::archetypes::CloudArchetype::RemoveAllButTheSky(drawn);
	EXPECT_TRUE(drawn.Valid(cloud));
	EXPECT_FALSE(drawn.Valid(mark));
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

// The load of a saved game starts the mind of each computer god again, as the game does. The god keeps its weights,
// its suppressions, the flags and the lists of the save. The turn stamps go to 0, the hand can be stopped again, and
// the god forgets the player that it takes the size of its creature from.
TEST(WorldSaveForget, ALoadStartsTheMindOfAComputerGodAgain)
{
	entt::registry registry;
	const auto god = registry.create();
	const auto town = registry.create();
	auto& state = registry.emplace<ComputerPlayer>(god);
	state.switchedOn = true;
	state.marked = true;
	state.started = true;
	state.creatureLike = PlayerNames::PLAYER_ONE;
	state.mind = computer_mind::MakeMind();
	state.mind.nodes.at(3).suppression = 0.25f;
	state.mind.nodes.at(3).weight = 0.5f;
	state.mind.nodes.at(3).lastPerformed = 700;
	state.mind.nodes.at(3).lastWorkedOut = 710;
	state.mind.nodes.at(3).object = town;
	state.mind.nodes.at(0).bestLeaf = 3;
	state.mind.doing = 0.75f;
	state.mind.lastEmptyTurn = 690;
	state.mind.wasBusy = false;
	state.mind.scriptHand = true;
	state.mind.castRequested.at(4) = 7;
	state.mind.queue.push_back({.name = "DIRECTPickupAndDropObject", .first = town, .turn = 650});
	state.hand.locked = true;
	state.hand.part = 1;
	computer_hand::Add(state.hand, computer_hand::StepKind::CastSpellAtObject, {computer_hand::Argument {.object = town}});
	state.casts.push_back(town);
	state.speed = 45.0f;
	state.seen.push_back({.thing = town, .turn = 600});

	ecs::world_save::ForgetUnsavedState(registry);
	const auto& loaded = registry.get<ComputerPlayer>(god);
	EXPECT_EQ(loaded.mind.nodes.at(3).suppression, 0.25f);
	EXPECT_EQ(loaded.mind.nodes.at(3).weight, 0.5f);
	EXPECT_EQ(loaded.mind.nodes.at(3).lastPerformed, 0u);
	EXPECT_EQ(loaded.mind.nodes.at(3).lastWorkedOut, 0u);
	EXPECT_EQ(loaded.mind.nodes.at(3).object, entt::entity {entt::null});
	EXPECT_FALSE(loaded.mind.nodes.at(0).bestLeaf.has_value());
	EXPECT_EQ(loaded.mind.doing, 0.0f);
	EXPECT_EQ(loaded.mind.lastEmptyTurn, 0u);
	EXPECT_TRUE(loaded.mind.wasBusy);
	EXPECT_FALSE(loaded.mind.scriptHand);
	EXPECT_EQ(loaded.mind.castRequested.at(4), 7u);
	ASSERT_EQ(loaded.mind.queue.size(), 1u);
	EXPECT_EQ(loaded.mind.queue.front().first, town);
	EXPECT_FALSE(loaded.creatureLike.has_value());
	EXPECT_FALSE(loaded.hand.locked);
	EXPECT_EQ(loaded.hand.part, 1);
	EXPECT_EQ(loaded.hand.steps.size(), 1u);
	EXPECT_TRUE(loaded.switchedOn);
	EXPECT_TRUE(loaded.marked);
	EXPECT_TRUE(loaded.started);
	EXPECT_EQ(loaded.casts.size(), 1u);
	EXPECT_EQ(loaded.speed, 45.0f);
	ASSERT_EQ(loaded.seen.size(), 1u);
	EXPECT_EQ(loaded.seen.front().turn, 600u);
}

// A save does not keep the file of a dance. After a load each dance takes the file of the row that it dances again, so
// that its groups continue to move.
TEST(WorldSaveForget, ALoadGivesEachDanceTheFileOfItsRowAgain)
{
	entt::registry registry;
	const auto worship = std::make_shared<const dance::DanceFile>(dance::DanceFile {.groupCount = 3});
	const auto meeting = std::make_shared<const dance::DanceFile>(dance::DanceFile {.groupCount = 1});
	const auto first = registry.create();
	registry.emplace<ecs::components::Dance>(first).type = DanceInfo::Worship;
	const auto second = registry.create();
	registry.emplace<ecs::components::Dance>(second).type = DanceInfo::Meeting;
	const auto unknown = registry.create();
	registry.emplace<ecs::components::Dance>(unknown).type = DanceInfo::Creature;
	ecs::world_save::TakeDanceFilesAgain(registry, [&](DanceInfo type) -> std::shared_ptr<const dance::DanceFile> {
		switch (type)
		{
		case DanceInfo::Worship:
			return worship;
		case DanceInfo::Meeting:
			return meeting;
		default:
			return nullptr;
		}
	});
	EXPECT_EQ(registry.get<ecs::components::Dance>(first).file, worship);
	EXPECT_EQ(registry.get<ecs::components::Dance>(second).file, meeting);
	EXPECT_EQ(registry.get<ecs::components::Dance>(unknown).file, nullptr);
}
