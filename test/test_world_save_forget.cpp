/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <tuple>
#include <vector>

#include <DanceFile.h>
#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/CloudArchetype.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
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
			// NOLINTNEXTLINE(performance-no-automatic-move): the lambda must copy the value on each call
			return worship;
		case DanceInfo::Meeting:
			// NOLINTNEXTLINE(performance-no-automatic-move): the lambda must copy the value on each call
			return meeting;
		default:
			return nullptr;
		}
	});
	EXPECT_EQ(registry.get<ecs::components::Dance>(first).file, worship);
	EXPECT_EQ(registry.get<ecs::components::Dance>(second).file, meeting);
	EXPECT_EQ(registry.get<ecs::components::Dance>(unknown).file, nullptr);
}

// A save from an older version keeps the height of each blow in the scale of the world. The load takes the height again
// from the point of the blow in the mesh space.
TEST(WorldSaveForget, ALoadTakesTheHeightsOfTheBlowsFromTheMesh)
{
	entt::registry registry;
	const auto creature = registry.create();
	auto& fighting = registry.emplace<ecs::components::CreatureFighting>(creature);
	fighting.reaches.push_back({.animation = 3, .point = glm::vec3(0.5f, 12.25f, -4.0f), .height = 12.25f * 1.75f});
	fighting.reaches.push_back({.animation = 4, .point = glm::vec3(0.0f), .height = 0.0f});
	ecs::world_save::TakeBlowHeightsFromTheMesh(registry);
	const auto& reaches = registry.get<ecs::components::CreatureFighting>(creature).reaches;
	ASSERT_EQ(reaches.size(), 2u);
	EXPECT_EQ(reaches[0].height, 12.25f);
	EXPECT_EQ(reaches[0].point, glm::vec3(0.5f, 12.25f, -4.0f));
	EXPECT_EQ(reaches[1].height, 0.0f);
	// A save from this version gives the same heights
	ecs::world_save::TakeBlowHeightsFromTheMesh(registry);
	EXPECT_EQ(reaches[0].height, 12.25f);
}

// A save of a game writes the values that the load of the save gives back, so that a save at once after the load has
// the same bytes. When the save is done, the world has its own values again.
TEST(WorldSaveForget, ASaveHasTheValuesThatItsLoadGivesBack)
{
	ecs::Registry registry;
	const auto god = registry.Create();
	registry.Assign<ecs::components::Player>(god, ecs::components::Player {.name = PlayerNames::PLAYER_TWO});
	registry.Assign<ecs::components::Alignment>(god, ecs::components::Alignment {.value = 0.25f, .pending = 0.0f});
	auto& statistics = registry.Assign<ecs::components::PlayerStatistics>(god);
	statistics.startAlignment = -0.5f;
	auto& player = registry.Assign<ComputerPlayer>(god);
	player.creatureLike = PlayerNames::PLAYER_ONE;
	player.mind = computer_mind::MakeMind();
	player.mind.nodes.at(3).weight = 0.5f;
	player.mind.nodes.at(3).lastPerformed = 700;
	player.hand.locked = true;
	for (int i = 0; i < 2; ++i)
	{
		registry.Assign<ecs::components::Town>(registry.Create());
	}
	registry.Context().globalStatistics.townsAtStart = 7;

	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature);
	auto& mind = registry.Assign<CreatureMindState>(creature);
	mind.leftAreaActions = std::vector<bool> {true, false};
	mind.agendaRefused = true;
	auto& glows = registry.Assign<ecs::components::CreatureHandGlows>(creature);
	creature_hand_glow::Set(glows.glows, creature_hand_glow::Place::LeftHand, 1.5f, 2.0f);
	glows.placed.at(0) = ecs::components::CreatureHandGlows::Placed {.zoom = 3.0f};
	glows.motion.at(0).turn = 1.25f;
	const auto path = registry.Create();
	registry.Assign<ecs::components::FootpathFinder>(path).plan.emplace();

	// The world as the load gives it back
	entt::registry loaded;
	const auto loadedGod = loaded.create(god);
	loaded.emplace<ComputerPlayer>(loadedGod, player);
	const auto loadedCreature = loaded.create(creature);
	loaded.emplace<CreatureMindState>(loadedCreature, mind);
	ecs::world_save::ForgetUnsavedValues(loaded);

	const auto before =
	    std::make_tuple(player.mind.nodes.at(3).lastPerformed, mind.leftAreaActions, glows.placed.at(0).has_value());
	{
		const ecs::world_save::SavedGameForm form(registry, true);
		const auto& saved = registry.Get<const ComputerPlayer>(god);
		const auto& expected = loaded.get<const ComputerPlayer>(loadedGod);
		EXPECT_EQ(saved.mind.nodes.size(), expected.mind.nodes.size());
		EXPECT_EQ(saved.mind.nodes.at(3).lastPerformed, expected.mind.nodes.at(3).lastPerformed);
		EXPECT_EQ(saved.mind.nodes.at(3).weight, 0.5f);
		EXPECT_FALSE(saved.creatureLike.has_value());
		EXPECT_FALSE(saved.hand.locked);
		EXPECT_TRUE(registry.Get<const CreatureMindState>(creature).leftAreaActions.empty());
		EXPECT_FALSE(registry.Get<const CreatureMindState>(creature).agendaRefused);
		EXPECT_FALSE(registry.Get<const ecs::components::FootpathFinder>(path).plan.has_value());
		// The creed powers come back one by one, with no glow placed yet
		const auto& savedGlows = registry.Get<const ecs::components::CreatureHandGlows>(creature);
		EXPECT_TRUE(creature_hand_glow::AnyActive(savedGlows.glows));
		EXPECT_FALSE(savedGlows.placed.at(0).has_value());
		EXPECT_EQ(savedGlows.motion.at(0).turn, 0.0f);
		// The statistics note the start of the game again
		EXPECT_EQ(registry.Get<const ecs::components::PlayerStatistics>(god).startAlignment, 0.25f);
		EXPECT_EQ(registry.Context().globalStatistics.townsAtStart, 2u);
	}
	EXPECT_EQ(std::make_tuple(registry.Get<const ComputerPlayer>(god).mind.nodes.at(3).lastPerformed,
	                          registry.Get<const CreatureMindState>(creature).leftAreaActions,
	                          registry.Get<const ecs::components::CreatureHandGlows>(creature).placed.at(0).has_value()),
	          before);
	EXPECT_TRUE(registry.Get<const ComputerPlayer>(god).creatureLike.has_value());
	EXPECT_TRUE(registry.Get<const ComputerPlayer>(god).hand.locked);
	EXPECT_TRUE(registry.Get<const CreatureMindState>(creature).agendaRefused);
	EXPECT_EQ(registry.Get<const ecs::components::CreatureHandGlows>(creature).motion.at(0).turn, 1.25f);
	EXPECT_EQ(registry.Get<const ecs::components::PlayerStatistics>(god).startAlignment, -0.5f);
	EXPECT_EQ(registry.Context().globalStatistics.townsAtStart, 7u);
	EXPECT_TRUE(registry.Get<const ecs::components::FootpathFinder>(path).plan.has_value());
}
