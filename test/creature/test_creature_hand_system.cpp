/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature hand system with a fake hand and a fake mind: who the hand may hold, the creature along a line of sight,
// strokes and slaps over the frames, and what the hand is given to send on as it lets go

#define LOCATOR_IMPLEMENTATIONS

#include <utility>
#include <vector>

#include <entt/entity/sparse_set.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFeedback.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureHandSystem;
using openblack::test::creature_fakes::FakeHand;
using openblack::test::creature_fakes::FakeMind;

namespace
{
class CreatureHandSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto& hand = static_cast<FakeHand&>(Locator::handSystem::emplace<FakeHand>());
		hand.left = test::creature_world::World::Registry().Create();
		_hand = &hand;
		_mind = &static_cast<FakeMind&>(Locator::creatureMindSystem::emplace<FakeMind>());
	}

	/// The hand held to a creature, as Grab leaves it
	HandOnCreature& HoldTo(entt::entity creature)
	{
		return test::creature_world::World::Registry().Assign<HandOnCreature>(_hand->left,
		                                                                      HandOnCreature {.creature = creature});
	}

	/// An ape standing upright: a body of two bones from its feet to its head, its rig in the cache
	entt::entity PosedApe()
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {}};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
		auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
		animation.skeleton.parents = {skeletal_animation::k_NoParent, 0};
		animation.boneMatrices = {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f))};
		return creature;
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	const test::RestoreService<Locator::handSystem> _restoreHand;
	const test::RestoreService<Locator::creatureMindSystem> _restoreMind;
	FakeHand* _hand {nullptr};
	FakeMind* _mind {nullptr};
	CreatureHandSystem _system;
};
} // namespace

TEST_F(CreatureHandSystemTest, OnlyAPlayersCreatureMayBeHeld)
{
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_THREE);
	const auto nobodys = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::NEUTRAL);
	EXPECT_TRUE(_system.MayHold(own));
	EXPECT_TRUE(_system.MayHold(other));
	EXPECT_FALSE(_system.MayHold(nobodys));
	const auto thing = test::creature_world::World::Registry().Create();
	EXPECT_FALSE(_system.MayHold(thing));
	// what may not be held is not taken hold of
	EXPECT_FALSE(_system.Grab(nobodys));
	EXPECT_FALSE(_system.Grab(thing));
	EXPECT_FALSE(_system.GetCreature().has_value());
	// a creature that may be is
	EXPECT_TRUE(_system.Grab(own));
	EXPECT_EQ(_system.GetCreature(), std::optional(own));
}

TEST_F(CreatureHandSystemTest, TheNearestCreatureAlongASightAndHowFar)
{
	const auto creature = PosedApe();
	// from 50 in front of the ape, at its middle: its body some way off, nearer than 50
	const auto hit = _system.CreatureAlong(glm::vec3(100.0f, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->creature, creature);
	EXPECT_GT(hit->distance, 40.0f);
	EXPECT_LT(hit->distance, 50.0f);
	// looking away from it: none
	EXPECT_FALSE(_system.CreatureAlong(glm::vec3(100.0f, 7.0f, 150.0f), glm::vec3(0.0f, 0.0f, 1.0f)).has_value());
}

TEST_F(CreatureHandSystemTest, TheCreatureUnderTheHandIsWhatTheHandStored)
{
	EXPECT_FALSE(_system.CreatureUnderHand().has_value());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.SetCreatureUnderHand(creature);
	EXPECT_EQ(_system.CreatureUnderHand(), std::optional(creature));
	_system.SetCreatureUnderHand(std::nullopt);
	EXPECT_FALSE(_system.CreatureUnderHand().has_value());
}

TEST_F(CreatureHandSystemTest, WithoutAContactUpdateMakesNoStorage)
{
	const auto storages = []() {
		std::vector<std::pair<entt::id_type, size_t>> all;
		std::as_const(test::creature_world::World::Registry())
		    .EachStorage([&all](entt::id_type id, const entt::sparse_set& storage) { all.emplace_back(id, storage.size()); });
		return all;
	};
	const auto before = storages();
	EXPECT_FALSE(_system.Update(glm::vec3(0.0f, 10.0f, 50.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f), 0.016f));
	EXPECT_FALSE(_system.Release().has_value());
	EXPECT_EQ(storages(), before);
	EXPECT_TRUE(_mind->forced.empty());
}

TEST_F(CreatureHandSystemTest, Resting_TheHandStrokes_SweptFastItSlaps)
{
	const auto creature = PosedApe();
	HoldTo(creature);
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	// resting on the body, still, for over a second: one stroke
	for (int frame = 0; frame < 12; ++frame)
	{
		const auto pose = _system.Update(glm::vec3(100.0f, 7.0f, 150.0f), ahead, glm::vec2(400.0f, 300.0f), 0.1f);
		ASSERT_TRUE(pose.has_value());
		EXPECT_TRUE(pose->onBody);
		EXPECT_EQ(_system.Pose()->position, pose->position);
	}
	ASSERT_EQ(_mind->forced.size(), 1u);
	EXPECT_FLOAT_EQ(_system.GetFeedbackSum(), creature_feedback::k_StrokeAmount);

	// swept fast onto the body from beside it: a gentle slap, 110 a second against the 75 of a creature 15 high, which
	// takes the stroke back
	EXPECT_FALSE(_system.Update(glm::vec3(95.0f, 7.0f, 150.0f), ahead, glm::vec2(350.0f, 300.0f), 0.05f)->onBody);
	EXPECT_TRUE(_system.Update(glm::vec3(100.5f, 7.0f, 150.0f), ahead, glm::vec2(405.0f, 300.0f), 0.05f)->onBody);
	ASSERT_EQ(_mind->forced.size(), 2u);
	EXPECT_LT(_system.GetFeedbackSum(), creature_feedback::k_StrokeAmount);
	EXPECT_TRUE(_system.Pose()->slapping);
}

TEST_F(CreatureHandSystemTest, LettingGoGivesHowTheCreatureWasTreated)
{
	const auto creature = test::creature_world::World::MakeCreature();
	HoldTo(creature).sum = 0.6f;
	EXPECT_FLOAT_EQ(_system.GetLastFeedbackSum(), 0.6f);
	const auto given = _system.Release();
	ASSERT_TRUE(given.has_value());
	EXPECT_EQ(given->creature, creature);
	EXPECT_FLOAT_EQ(given->feedback, creature_feedback::Delivered(0.6f));
	// the hand sends it on as a packet: the mind is not told here
	EXPECT_TRUE(_mind->feedback.empty());
	// let go, the sum it let go with is kept for the panel
	EXPECT_FALSE(_system.GetCreature().has_value());
	EXPECT_FALSE(_system.Pose().has_value());
	EXPECT_FLOAT_EQ(_system.GetFeedbackSum(), 0.0f);
	EXPECT_FLOAT_EQ(_system.GetLastFeedbackSum(), 0.6f);
	EXPECT_TRUE(std::as_const(test::creature_world::World::Registry()).AllOf<HandLastFeedback>(_hand->left));
	// letting go again gives nothing more
	EXPECT_FALSE(_system.Release().has_value());
}

TEST_F(CreatureHandSystemTest, TheFeedbackGivenIsClampedToOne)
{
	const auto creature = test::creature_world::World::MakeCreature();
	HoldTo(creature).sum = 3.0f;
	EXPECT_FLOAT_EQ(_system.Release()->feedback, 1.0f);
	HoldTo(creature).sum = -3.0f;
	EXPECT_FLOAT_EQ(_system.Release()->feedback, -1.0f);
}
