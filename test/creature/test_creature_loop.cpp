/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creatures in the game loop (ECS/CreatureLoop.h): the order of the turn's and the frame's calls, the arguments
// they pass on, and the profile stages they are timed in, with recording fakes in the creature services

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/sparse_set.hpp>
#include <gtest/gtest.h>

#include "ECS/CreatureLoop.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Profiler.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::test::creature_loop_fakes;

namespace
{
std::vector<std::string> NamesOf(const CallLog& log)
{
	std::vector<std::string> names;
	names.reserve(log.size());
	for (const auto& call : log)
	{
		names.push_back(call.name);
	}
	return names;
}

class CreatureLoopTest: public ::testing::Test
{
protected:
	CreatureLoopTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::creaturePhysiologySystem::emplace<FakePhysiology>(log);
		Locator::creatureAnimationSystem::emplace<FakeAnimation>(log);
		Locator::creatureSkinSystem::emplace<FakeSkin>(log);
		Locator::creatureHairSystem::emplace<FakeHair>(log);
		Locator::creatureAudioSystem::emplace<FakeAudio>(log);
		Locator::footprintSystem::emplace<FakeFootprints>(log);
		Locator::creatureLocomotionSystem::emplace<FakeLocomotion>(log);
		Locator::creatureMindSystem::emplace<FakeMind>(log);
		Locator::creatureObjectActionSystem::emplace<FakeObjectAction>(log);
		Locator::creatureFightSystem::emplace<FakeFight>(log);
		leash = &static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>(log));
	}

	/// Whether the stage was timed in the profiler's current frame
	[[nodiscard]] bool Timed(Profiler::Stage stage) const
	{
		const auto& entry = profiler->GetEntries().at(profiler->GetEntryIndex(0));
		return entry.stages.at(static_cast<uint8_t>(stage)).finalized;
	}

	CallLog log;
	std::unique_ptr<Profiler> profiler = std::make_unique<Profiler>();
	/// The leash service the test sets the player's creature and its leash in
	FakeLeash* leash {nullptr};

private:
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::creaturePhysiologySystem> _restorePhysiology;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureSkinSystem> _restoreSkin;
	const test::RestoreService<Locator::creatureHairSystem> _restoreHair;
	const test::RestoreService<Locator::creatureAudioSystem> _restoreAudio;
	const test::RestoreService<Locator::footprintSystem> _restoreFootprints;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreObjectAction;
	const test::RestoreService<Locator::creatureFightSystem> _restoreFight;
	const test::RestoreService<Locator::leashSystem> _restoreLeash;
};
} // namespace

TEST_F(CreatureLoopTest, TurnCallsTheServicesInOrder)
{
	ecs::creature_loop::ProcessTurn(*profiler);
	// the creatures' miracles come last, from the spell code itself: with no creature they log nothing here
	const std::vector<std::string> expected {
	    "physiology.ProcessTurn",   "animation.ProcessTurn", "skin.ProcessTurn", "leash.ProcessTurn",
	    "mind.ProcessTurn",         "mind.PlanTurn",         "mind.LearnTurn",   "locomotion.ProcessTurn",
	    "objectAction.ProcessTurn", "fight.ProcessTurn",
	};
	EXPECT_EQ(NamesOf(log), expected);
}

TEST_F(CreatureLoopTest, TurnTimesEightStages)
{
	ecs::creature_loop::ProcessTurn(*profiler);
	for (const auto stage : {Profiler::Stage::CreaturePhysiologyUpdate, Profiler::Stage::CreatureLeashUpdate,
	                         Profiler::Stage::CreatureMindUpdate, Profiler::Stage::CreaturePlannerUpdate,
	                         Profiler::Stage::CreatureLearningUpdate, Profiler::Stage::CreatureLocomotionUpdate,
	                         Profiler::Stage::CreatureObjectActionUpdate, Profiler::Stage::CreatureCombatUpdate})
	{
		EXPECT_TRUE(Timed(stage)) << Profiler::k_StageNames.at(static_cast<uint8_t>(stage));
	}
	EXPECT_FALSE(Timed(Profiler::Stage::CreatureFrame));
	EXPECT_FALSE(Timed(Profiler::Stage::CreatureLeashFrame));
}

TEST_F(CreatureLoopTest, TurnMakesNoStorage)
{
	// the creatures' miracles walk the registry for creatures with spells: none, and no storage made for them
	const auto storages = [] {
		size_t count = 0;
		std::as_const(Locator::entitiesRegistry::value()).EachStorage([&count](entt::id_type, const entt::sparse_set&) {
			++count;
		});
		return count;
	};
	const auto before = storages();
	ecs::creature_loop::ProcessTurn(*profiler);
	EXPECT_EQ(storages(), before);
}

TEST_F(CreatureLoopTest, FrameCallsTheServicesInOrder)
{
	ecs::creature_loop::UpdateFrame(0.25f, 16, *profiler);
	const std::vector<std::string> expected {
	    "fight.Update",      "locomotion.Update",
	    "physiology.Update", "objectAction.UpdateDraw",
	    "animation.Update",  "objectAction.UpdateHeldDraw",
	    "hair.Update",       "audio.Update",
	    "footprints.Update", "skin.Update",
	};
	EXPECT_EQ(NamesOf(log), expected);
	EXPECT_TRUE(Timed(Profiler::Stage::CreatureFrame));
}

TEST_F(CreatureLoopTest, FramePassesTheTurnShareAndTheGameTime)
{
	ecs::creature_loop::UpdateFrame(0.25f, 16, *profiler);
	const CallLog expected {
	    {.name = "fight.Update", .args = {0.25f, 16.0f}},
	    {.name = "locomotion.Update", .args = {0.25f}},
	    {.name = "physiology.Update", .args = {16.0f * 0.001f}},
	    {.name = "objectAction.UpdateDraw", .args = {0.25f}},
	    {.name = "animation.Update", .args = {16.0f}},
	    {.name = "objectAction.UpdateHeldDraw", .args = {}},
	    {.name = "hair.Update", .args = {16.0f}},
	    {.name = "audio.Update", .args = {16.0f}},
	    {.name = "footprints.Update", .args = {16.0f}},
	    {.name = "skin.Update", .args = {}},
	};
	EXPECT_EQ(log, expected);
}

TEST_F(CreatureLoopTest, PausedFrameIsAZeroStep)
{
	// paused, the frame's game time is 0 and the turn's share stays where it was
	ecs::creature_loop::UpdateFrame(0.5f, 0, *profiler);
	ASSERT_EQ(log.size(), 10U);
	for (const auto& call : log)
	{
		if (call.name == "fight.Update")
		{
			EXPECT_EQ(call.args, (std::vector<float> {0.5f, 0.0f}));
		}
		else if (call.name == "locomotion.Update" || call.name == "objectAction.UpdateDraw")
		{
			EXPECT_EQ(call.args, std::vector<float> {0.5f});
		}
		else if (call.name != "skin.Update" && call.name != "objectAction.UpdateHeldDraw")
		{
			EXPECT_EQ(call.args, std::vector<float> {0.0f}) << call.name;
		}
	}
}

TEST_F(CreatureLoopTest, LeashSwingsByTheFrameSeconds)
{
	ecs::creature_loop::UpdateLeash(0.016f, *profiler);
	const CallLog expected {{.name = "leash.Update", .args = {0.016f}}};
	EXPECT_EQ(log, expected);
	EXPECT_TRUE(Timed(Profiler::Stage::CreatureLeashFrame));
}

TEST_F(CreatureLoopTest, NewLandClearsTheFootprints)
{
	ecs::creature_loop::OnLoadMap();
	const CallLog expected {{.name = "footprints.Reset", .args = {}}};
	EXPECT_EQ(log, expected);
}

TEST_F(CreatureLoopTest, AHandDemoTakesOffTheLeashHeldInTheHand)
{
	leash->playersCreature = static_cast<entt::entity>(7);
	leash->leashed = true;
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	const CallLog expected {{.name = "leash.TakeOffHeldLeash", .args = {static_cast<float>(PlayerNames::PLAYER_ONE)}}};
	EXPECT_EQ(log, expected);
}

TEST_F(CreatureLoopTest, AHandDemoLeavesATiedLeashOrNone)
{
	// no creature, or one with no leash
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	leash->playersCreature = static_cast<entt::entity>(7);
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	// a leash tied to something stays
	leash->leashed = true;
	leash->tiedTo = static_cast<entt::entity>(9);
	ecs::creature_loop::ReleaseLeashHeldInHand(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(log.empty());
}

TEST(CreatureLoopFrozen, OnlyPausedInsideTheCitadel)
{
	EXPECT_TRUE(ecs::creature_loop::FrozenInCitadel(true, true));
	EXPECT_FALSE(ecs::creature_loop::FrozenInCitadel(true, false));
	EXPECT_FALSE(ecs::creature_loop::FrozenInCitadel(false, true));
	EXPECT_FALSE(ecs::creature_loop::FrozenInCitadel(false, false));
}

TEST(CreatureLoopStages, NamesFitTheSummary)
{
	for (auto stage = static_cast<uint8_t>(Profiler::Stage::CreaturePhysiologyUpdate);
	     stage <= static_cast<uint8_t>(Profiler::Stage::CreatureLeashFrame); ++stage)
	{
		EXPECT_LE(Profiler::k_StageNames.at(stage).size(), 22U) << Profiler::k_StageNames.at(stage);
		EXPECT_FALSE(Profiler::k_StageNames.at(stage).empty());
	}
}
