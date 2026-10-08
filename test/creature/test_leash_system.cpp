/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The leash system with a fake hand and fake locomotion: who may lead which creature, the refusals kept on the players'
// entities, putting the leash on and taking it off, a taut rope pulling the creature to the hand, the home it is kept
// near, and a land with a temple getting no leash posts

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "Creature/LeashOwnership.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerLeash.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::LeashSystem;
using openblack::test::creature_fakes::FakeHand;
using openblack::test::creature_fakes::FakeLocomotion;
using Refusal = openblack::creature_leash::Refusal;

namespace
{
class LeashSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_hand = &static_cast<FakeHand&>(Locator::handSystem::emplace<FakeHand>());
		_locomotion = &static_cast<FakeLocomotion&>(Locator::creatureLocomotionSystem::emplace<FakeLocomotion>());
	}

	/// The first player's creature, the one they lead, knowing the learning leash
	entt::entity Leadable(glm::vec3 position = glm::vec3(100.0f, 0.0f, 100.0f))
	{
		const auto creature = test::creature_world::World::MakeCreature(position, PlayerNames::PLAYER_ONE);
		EXPECT_TRUE(_system.SetLeashable(creature, true));
		_system.SetKnown(creature, LeashType::Rope, true);
		return creature;
	}
	static CreatureLeash& LeashOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureLeash>(creature);
	}
	/// A player's entity on the land, which their refusals are kept on
	static entt::entity MakePlayer(PlayerNames name)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto player = registry.Create();
		registry.Assign<Player>(player, name);
		return player;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::handSystem> _restoreHand;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	FakeHand* _hand {nullptr};
	FakeLocomotion* _locomotion {nullptr};
	LeashSystem _system;
};
} // namespace

TEST_F(LeashSystemTest, WhoMayLeadWhichCreature)
{
	MakePlayer(PlayerNames::PLAYER_ONE);
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	// not yet the one its player leads
	EXPECT_FALSE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_EQ(_system.LastRefusal(PlayerNames::PLAYER_ONE)->why, Refusal::NotLeashable);
	ASSERT_TRUE(_system.SetLeashable(creature, true));
	// it must know the learning leash first
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_ONE, creature, LeashType::Rope), Refusal::DoesNotKnowLearningLeash);
	_system.SetKnown(creature, LeashType::Rope, true);
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_ONE, creature, LeashType::Rope), Refusal::None);
	// another player's, and what is no creature
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_TWO, creature, LeashType::Rope), Refusal::OwnedByAnother);
	EXPECT_EQ(_system.WhyNot(PlayerNames::PLAYER_ONE, test::creature_world::World::Registry().Create(), LeashType::Rope),
	          Refusal::NotACreature);
	EXPECT_EQ(_system.PlayersCreature(PlayerNames::PLAYER_ONE), std::optional(creature));
	// nobody's creature can't be the one anyone leads
	const auto wild = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::NEUTRAL);
	EXPECT_FALSE(_system.SetLeashable(wild, true));
	EXPECT_FALSE(_system.IsLeashable(wild));
}

TEST_F(LeashSystemTest, ARefusalIsKeptOnThePlayersEntity)
{
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	const auto storages = [&lookup]() {
		size_t count = 0;
		lookup.EachStorage([&count](entt::id_type, const auto&) { ++count; });
		return count;
	};
	// a player with no entity on the land keeps no refusal, and asking or refusing makes no storage
	const auto before = storages();
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_TWO, creature_leash::LeashKey::Leash));
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_EQ(storages(), before);

	// a player with no creature to lead: kept on their entity, with no creature
	const auto player = MakePlayer(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(_system.PressKey(PlayerNames::PLAYER_ONE, creature_leash::LeashKey::Leash));
	auto refused = _system.LastRefusal(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(refused.has_value());
	EXPECT_EQ(refused->player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(refused->creature, entt::entity {entt::null});
	EXPECT_EQ(refused->why, Refusal::NotACreature);
	ASSERT_NE(lookup.TryGet<const PlayerLeashRefusal>(player), nullptr);

	// the next refusal takes its place; the other player still has none
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_system.PutOn(creature, LeashType::Rope));
	refused = _system.LastRefusal(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(refused.has_value());
	EXPECT_EQ(refused->creature, creature);
	EXPECT_EQ(refused->why, Refusal::NotLeashable);
	EXPECT_FALSE(_system.LastRefusal(PlayerNames::PLAYER_TWO).has_value());
}

TEST_F(LeashSystemTest, OnOffToggleAndChange)
{
	const auto creature = Leadable();
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_TRUE(_system.IsLeashed(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Rope);
	// a leash it does not know can't be swapped in
	EXPECT_FALSE(_system.ChangeType(creature, LeashType::Evil));
	_system.SetKnown(creature, LeashType::Evil, true);
	EXPECT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Evil);
	// the leash key takes it off, and puts the picked one on again
	EXPECT_TRUE(_system.Toggle(creature));
	EXPECT_FALSE(_system.IsLeashed(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::None);
	EXPECT_TRUE(_system.Toggle(creature));
	EXPECT_EQ(_system.TypeOf(creature), LeashType::Evil);
	_system.TakeOff(creature);
	EXPECT_FALSE(_system.IsLeashed(creature));
}

TEST_F(LeashSystemTest, ThePickedLeashIsReportedWornOrNot)
{
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	const auto storages = [&lookup]() {
		size_t count = 0;
		lookup.EachStorage([&count](entt::id_type, const auto&) { ++count; });
		return count;
	};
	// a thing with no leashes has none picked, and asking makes no storage
	const auto thing = registry.Create();
	const auto before = storages();
	EXPECT_EQ(_system.Picked(thing), LeashType::None);
	EXPECT_EQ(storages(), before);

	const auto creature = Leadable();
	// nothing picked yet: none, though the hotkeys would put the rope on
	EXPECT_EQ(_system.Picked(creature), LeashType::None);
	_system.SetKnown(creature, LeashType::Evil, true);
	ASSERT_TRUE(_system.ChangeType(creature, LeashType::Evil));
	// picked but not put on: no leash is worn, the pick is still reported
	EXPECT_EQ(_system.TypeOf(creature), LeashType::None);
	EXPECT_EQ(_system.Picked(creature), LeashType::Evil);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	EXPECT_EQ(_system.Picked(creature), LeashType::Rope);
	_system.TakeOff(creature);
	EXPECT_EQ(_system.Picked(creature), LeashType::Rope);
}

TEST_F(LeashSystemTest, ATautRopePullsTheCreatureToTheHand)
{
	const auto creature = Leadable();
	_hand->position = glm::vec3(300.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	LeashOf(creature).worn->rope.tension = 1.0f;
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_TRUE(_locomotion->moves.front().lead);
	EXPECT_EQ(_locomotion->moves.front().point, glm::vec2(300.0f, 100.0f));
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::WalkingToHand);
}

TEST_F(LeashSystemTest, ALeashThatDoesNotWorkPullsNothing)
{
	const auto creature = Leadable();
	_hand->position = glm::vec3(300.0f, 0.0f, 100.0f);
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	_system.SetWorks(creature, false);
	LeashOf(creature).worn->rope.tension = 1.0f;
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	EXPECT_EQ(LeashOf(creature).control, CreatureLeash::Control::Idle);
}

TEST_F(LeashSystemTest, KeptNearHomeItWalksBack)
{
	const auto creature = Leadable();
	_system.ConfineToHome(creature, 20.0f);
	ASSERT_TRUE(LeashOf(creature).home.has_value());
	EXPECT_EQ(*LeashOf(creature).home, glm::vec3(100.0f, 0.0f, 100.0f));
	_system.ProcessTurn();
	EXPECT_TRUE(_locomotion->moves.empty());
	// strayed off, it walks back
	test::creature_world::World::Registry().Get<Transform>(creature).position = glm::vec3(200.0f, 0.0f, 100.0f);
	_system.ProcessTurn();
	ASSERT_EQ(_locomotion->moves.size(), 1u);
	EXPECT_FALSE(_locomotion->moves.front().lead);
	EXPECT_EQ(_locomotion->moves.front().point, glm::vec2(100.0f, 100.0f));
	EXPECT_TRUE(LeashOf(creature).returning);
	_system.ClearConfinement(creature);
	EXPECT_FALSE(LeashOf(creature).returning);
}

TEST_F(LeashSystemTest, TakingOffTheHeldLeashLeavesATiedOne)
{
	const auto creature = Leadable();
	auto& registry = test::creature_world::World::Registry();
	const auto post = registry.Create();
	registry.Assign<Transform>(post, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.PutOn(creature, LeashType::Rope));
	ASSERT_TRUE(_system.TieTo(creature, post));
	EXPECT_EQ(_system.TiedTo(creature), std::optional(post));
	EXPECT_FALSE(_system.TakeOffHeldLeash(PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(_system.IsLeashed(creature));
	_system.UntieToHand(creature);
	EXPECT_TRUE(_system.TakeOffHeldLeash(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_system.IsLeashed(creature));
	// another player has no leash to take off
	EXPECT_FALSE(_system.TakeOffHeldLeash(PlayerNames::PLAYER_TWO));
}

TEST_F(LeashSystemTest, ALandWithATempleGetsNoLeashPosts)
{
	auto& registry = test::creature_world::World::Registry();
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple, PlayerNames::PLAYER_ONE);
	registry.Assign<Transform>(temple, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto creature = Leadable();
	const auto& lookup = std::as_const(registry);
	const auto things = lookup.Size<Transform>();
	const auto storages = [&lookup]() {
		size_t count = 0;
		lookup.EachStorage([&count](entt::id_type, const auto&) { ++count; });
		return count;
	};
	const auto before = storages();
	_system.ProcessTurn();
	_system.Update(0.033f);
	EXPECT_EQ(lookup.Size<Transform>(), things);
	EXPECT_EQ(storages(), before);
	EXPECT_FALSE(_system.IsLeashed(creature));
}
