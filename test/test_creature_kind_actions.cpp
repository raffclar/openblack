/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>

#include <gtest/gtest.h>

#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
using creature_object_actions::Thing;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t KindActionsFirst(uint32_t /*range*/)
{
	return 0;
}
} // namespace

TEST(CreatureKindActions, LivingThingsAreVillagersAnimalsBirdsAndCreatures)
{
	for (const auto thing : {Thing::Villager, Thing::Animal, Thing::Bird, Thing::Creature})
	{
		EXPECT_TRUE(creature_object_actions::IsLiving(thing));
	}
	for (const auto thing : {Thing::Tree, Thing::Rock, Thing::MobileObject, Thing::Fixture, Thing::Other})
	{
		EXPECT_FALSE(creature_object_actions::IsLiving(thing));
	}
}

TEST(CreatureKindActions, StonesKnockDownTheLivingAndWhatIsSetIntoTheLand)
{
	EXPECT_TRUE(creature_object_actions::CanBeStoned(Thing::Villager, false));
	EXPECT_FALSE(creature_object_actions::CanBeStoned(Thing::Villager, true));
	EXPECT_TRUE(creature_object_actions::CanBeStoned(Thing::Creature, false));
	for (const auto thing :
	     {Thing::Fixture, Thing::Rock, Thing::Bonfire, Thing::DeadTree, Thing::Static, Thing::MagicMushroom, Thing::Shield})
	{
		EXPECT_TRUE(creature_object_actions::CanBeStoned(thing, false));
	}
	for (const auto thing : {Thing::Tree, Thing::Lantern, Thing::Vortex, Thing::TeleportStone, Thing::MobileObject, Thing::Pot,
	                         Thing::Pile, Thing::Ball, Thing::Creed, Thing::Other})
	{
		EXPECT_FALSE(creature_object_actions::CanBeStoned(thing, false));
	}
}

TEST(CreatureKindActions, OnlyStaticsOtherThanToysDoDamageWhenThrown)
{
	for (const auto thing : {Thing::Rock, Thing::Bonfire, Thing::DeadTree, Thing::Static, Thing::Vortex, Thing::TeleportStone})
	{
		EXPECT_TRUE(creature_object_actions::DoesThrowingDamage(thing, false));
		EXPECT_FALSE(creature_object_actions::DoesThrowingDamage(thing, true));
	}
	for (const auto thing : {Thing::Villager, Thing::Tree, Thing::MobileObject, Thing::Ball, Thing::Lantern, Thing::Shield})
	{
		EXPECT_FALSE(creature_object_actions::DoesThrowingDamage(thing, false));
	}
}

TEST(CreatureKindActions, WhatLiesAboutStaticsAndTreesGoIntoTheSea)
{
	for (const auto thing : {Thing::MobileObject, Thing::Ball, Thing::Creed, Thing::Pot, Thing::Pile, Thing::Rock,
	                         Thing::DeadTree, Thing::Static, Thing::Tree})
	{
		EXPECT_TRUE(creature_object_actions::CanBeThrownInTheSea(thing));
	}
	for (const auto thing :
	     {Thing::Villager, Thing::Animal, Thing::Creature, Thing::MagicMushroom, Thing::Fixture, Thing::Lantern, Thing::Shield})
	{
		EXPECT_FALSE(creature_object_actions::CanBeThrownInTheSea(thing));
	}
}

TEST(CreatureKindActions, TheThingUsedIsTheNearestOtherThanTheGoal)
{
	const std::array objects {
	    creature_planner::ObjectCandidate {.id = 1, .distance = 10.0f},
	    creature_planner::ObjectCandidate {.id = 2, .distance = 5.0f},
	    creature_planner::ObjectCandidate {.id = 3, .distance = 5.0f},
	};
	EXPECT_EQ(creature_planner::ChooseUsed(objects, 0.4f, std::nullopt), 2u);
	// Never the goal; of equals the first
	EXPECT_EQ(creature_planner::ChooseUsed(objects, 0.4f, 2u), 3u);
	// Something held comes first however far
	const std::array held {
	    creature_planner::ObjectCandidate {.id = 1, .distance = 1.0f},
	    creature_planner::ObjectCandidate {.id = 2, .distance = 150.0f, .held = true},
	};
	EXPECT_EQ(creature_planner::ChooseUsed(held, 0.4f, std::nullopt), 2u);
	// What the distance weighs fully against counts for nothing
	const std::array far {creature_planner::ObjectCandidate {.id = 1, .distance = 250.0f}};
	EXPECT_EQ(creature_planner::ChooseUsed(far, 1.0f, std::nullopt), std::nullopt);
	EXPECT_EQ(creature_planner::ChooseUsed({}, 0.4f, std::nullopt), std::nullopt);
}

TEST(CreatureKindActions, GivingWoodPullsUpTheTreeAndThrowsItIntoTheStore)
{
	const auto agenda = GiveWoodToStore(TreeToTake {.tree = 7, .point = {3.0f, 4.0f}, .radius = 2.0f}, 9, 5.0f, 15.0f);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(agenda[0].movement.point, glm::vec2(3.0f, 4.0f));
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, (2.0f + 5.0f) * 1.2f);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 7u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[2].movement.maxDistance, 15.0f);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[3].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 0.1f);
	EXPECT_EQ(agenda[3].effect, Effect::Completed);
	EXPECT_EQ(agenda[4].order.kind, ObjectOrder::Kind::ThrowInStore);
	EXPECT_EQ(agenda[4].order.object, 9u);
	// Holding a tree, it gives that
	EXPECT_EQ(GiveWoodToStore(std::nullopt, 9, 5.0f, 15.0f).size(), 3);
}

TEST(CreatureKindActions, GivingWoodWithNoTreeIdles)
{
	const auto* give = plan_actions::For("GiveWoodFromTreeToStoragePit");
	ASSERT_NE(give, nullptr);
	EXPECT_EQ(give->target, plan_actions::Target::StoragePit);
	EXPECT_EQ(give->use, plan_actions::Target::PickableTree);
	const auto idle = plan_actions::Agenda(*give, 9u, {0.0f, 0.0f}, {}, KindActionsFirst);
	ASSERT_TRUE(idle.has_value());
	EXPECT_EQ(idle->front().kind, Step::Kind::Wait);
	plan_actions::Situation holding;
	holding.holdingTree = true;
	const auto given = plan_actions::Agenda(*give, 9u, {0.0f, 0.0f}, holding, KindActionsFirst);
	ASSERT_TRUE(given.has_value());
	EXPECT_EQ(given->back().order.kind, ObjectOrder::Kind::ThrowInStore);
}

TEST(CreatureKindActions, PlantingATreePutsItDownRightByTheThing)
{
	const auto agenda = PlantTreeBy(7u, 9u);
	ASSERT_EQ(agenda.size(), 4);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].order.object, 7u);
	EXPECT_EQ(agenda[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ(agenda[1].movement.object, 9u);
	EXPECT_FLOAT_EQ(agenda[1].movement.maxDistance, 0.0f);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(agenda[3].order.animation, creature_throw::k_PutDown);
	EXPECT_EQ(PlantTreeBy(std::nullopt, 9u).size(), 3);

	const auto* plant = plan_actions::For("PlantTree");
	ASSERT_NE(plant, nullptr);
	EXPECT_EQ(plant->use, plan_actions::Target::PlantableTree);
	EXPECT_TRUE(plant->notWhatItHolds);
	// Nothing to plant, nothing done
	EXPECT_FALSE(plan_actions::Agenda(*plant, 9u, {0.0f, 0.0f}, {}, KindActionsFirst).has_value());
}

TEST(CreatureKindActions, ThrowingToImpressTakesAnythingButTheLivingThatIsntTooTall)
{
	EXPECT_TRUE(creature_object_actions::CanBeThrownToImpress(Thing::Rock, 5.0f, 100.0f));
	EXPECT_TRUE(creature_object_actions::CanBeThrownToImpress(Thing::Tree, 63.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::Tree, 64.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::Villager, 1.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::Vortex, 1.0f, 100.0f));
	EXPECT_FALSE(creature_object_actions::CanBeThrownToImpress(Thing::TeleportStone, 1.0f, 100.0f));
}

TEST(CreatureKindActions, PractisingAThrowThrowsOffToOneSide)
{
	const auto agenda = PracticeThrow(7u, {100.0f, 200.0f}, 10.0f);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[0].order.object, 7u);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(130.0f, 230.0f));
	// Holding something, it throws that
	EXPECT_EQ(PracticeThrow(std::nullopt, {0.0f, 0.0f}, 10.0f).size(), 1);

	const auto* around = plan_actions::For("ThrowAround");
	ASSERT_NE(around, nullptr);
	EXPECT_EQ(around->target, plan_actions::Target::AnyLiving);
	EXPECT_EQ(around->build, plan_actions::Build::PracticeThrow);
	const auto* practice = plan_actions::For("PracticeThrow");
	ASSERT_NE(practice, nullptr);
	EXPECT_EQ(practice->target, plan_actions::Target::Pickable);
}

TEST(CreatureKindActions, ThrowingAToyAtAThing)
{
	const auto agenda = ThrowToyAt(4u, 9u, {10.0f, 20.0f}, 15.0f, KindActionsFirst);
	ASSERT_EQ(agenda.size(), 5);
	EXPECT_EQ(agenda[0].animation, creature_layers::animations::k_Happy);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.object, 4u);
	EXPECT_EQ(agenda[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Anger);
	EXPECT_EQ(agenda[3].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[3].order.object, 9u);
	EXPECT_EQ(agenda[4].animation, creature_layers::animations::k_Happy);
	EXPECT_EQ(ThrowToyAt(4u, 9u, {10.0f, 20.0f}, 15.0f, [](uint32_t range) { return range - 1; }).size(), 3);
}

TEST(CreatureKindActions, HurlingAndThrowingToImpressNeedSomethingToThrow)
{
	for (const auto name : {"Hurl", "ThrowToImpress"})
	{
		const auto* hurl = plan_actions::For(name);
		ASSERT_NE(hurl, nullptr);
		EXPECT_TRUE(hurl->notWhatItHolds);
		EXPECT_FALSE(plan_actions::Agenda(*hurl, 9u, {0.0f, 0.0f}, {}, KindActionsFirst).has_value());
		plan_actions::Situation with;
		with.instrument = 4u;
		const auto agenda = plan_actions::Agenda(*hurl, 9u, {0.0f, 0.0f}, with, KindActionsFirst);
		ASSERT_TRUE(agenda.has_value());
		EXPECT_EQ(agenda->back().order.kind, ObjectOrder::Kind::Throw);
	}
	EXPECT_EQ(plan_actions::For("Hurl")->target, plan_actions::Target::Stonable);
	EXPECT_EQ(plan_actions::For("Hurl")->use, plan_actions::Target::ThrowingDamage);
	EXPECT_EQ(plan_actions::For("ThrowToImpress")->use, plan_actions::Target::ThrowableToImpress);
}

TEST(CreatureKindActions, ThrowingIntoTheSeaChoosesWhatMayGoInTheSea)
{
	const auto* sea = plan_actions::For("ThrowInTheSea");
	ASSERT_NE(sea, nullptr);
	EXPECT_EQ(sea->target, plan_actions::Target::SeaThrowable);
}

TEST(CreatureKindActions, GettingHighEatsTheThingPickedUpUnlessAMushroomIsHeld)
{
	const auto agenda = GetHigh(7u, 7u);
	ASSERT_EQ(agenda.size(), 2);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ(agenda[1].effect, Effect::Eat);
	EXPECT_EQ(GetHigh(std::nullopt, 7u).size(), 1);
	const auto* high = plan_actions::For("GetHigh");
	ASSERT_NE(high, nullptr);
	plan_actions::Situation holding;
	holding.holdingMushroom = true;
	EXPECT_EQ(plan_actions::Agenda(*high, 7u, {0.0f, 0.0f}, holding, KindActionsFirst)->size(), 1);
}

TEST(CreatureKindActions, BeingHighWithoutSittingDown)
{
	bool looked = false;
	const auto agenda = BecomeHigh(
	    [&looked]() {
		    looked = true;
		    return std::optional<HighSeat> {};
	    },
	    [](uint32_t range) { return range - 1; }, [](float) { return 0.0f; });
	EXPECT_FALSE(looked);
	ASSERT_EQ(agenda.size(), 11);
	EXPECT_EQ(agenda[0].kind, Step::Kind::WatchCamera);
	EXPECT_FLOAT_EQ(agenda[0].seconds, 3.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Amazed);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Overlay);
	EXPECT_EQ(agenda[1].animation, k_Talk);
	EXPECT_FLOAT_EQ(agenda[2].seconds, 2.0f);
	EXPECT_EQ(agenda[2].heldEyes, Eyes::Stoned);
	EXPECT_EQ(agenda[3].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[3].seconds, 3.1f);
	EXPECT_EQ(agenda[4].animation, k_HighDazed);
	EXPECT_EQ(agenda[5].animation, k_HighConfused);
	EXPECT_FLOAT_EQ(agenda[6].seconds, 1.1f);
	EXPECT_EQ(agenda[7].animation, k_HighConfused);
	EXPECT_EQ(agenda[8].kind, Step::Kind::WatchCamera);
	EXPECT_EQ(agenda[9].kind, Step::Kind::Overlay);
	EXPECT_EQ(agenda[10].animation, k_HighDazed);
}

TEST(CreatureKindActions, BeingHighSittingDownWhereItIsClear)
{
	const auto agenda = BecomeHigh([]() { return std::optional(HighSeat {.place = {5.0f, 6.0f}, .radius = 3.0f}); },
	                               KindActionsFirst, [](float most) { return most; });
	ASSERT_EQ(agenda.size(), 18);
	EXPECT_EQ(agenda[5].animation, creature_layers::animations::k_Happy);
	EXPECT_EQ(agenda[8].animation, k_HighDazedAgain);
	EXPECT_EQ(agenda[10].kind, Step::Kind::Move);
	EXPECT_EQ(agenda[10].movement.point, glm::vec2(5.0f, 6.0f));
	EXPECT_FLOAT_EQ(agenda[10].movement.maxDistance, 3.0f);
	EXPECT_EQ(agenda[11].kind, Step::Kind::Static);
	EXPECT_FLOAT_EQ(agenda[11].seconds, 15.0f);
	EXPECT_EQ(agenda[11].sequence[1], creature_layers::animations::k_Sit);
	EXPECT_EQ(agenda[11].face, creature_face::Cue::Sad);
	EXPECT_EQ(agenda[12].animation, k_HighSitDazed);
	EXPECT_EQ(agenda[13].animation, k_HighFrightened);
	EXPECT_EQ(agenda[13].heldEyes, Eyes::Wide);
	EXPECT_EQ(agenda[14].animation, k_HighSitDazed);
	EXPECT_EQ(agenda.back().animation, k_HighDazed);
}

TEST(CreatureKindActions, KickingABallAboutKicksItTwiceAtTheLeast)
{
	// Every chance comes up nought: it points after each kick, kicks twice and jumps for joy
	const auto agenda = KickBallAround(7, KindActionsFirst);
	ASSERT_EQ(agenda.size(), 11u);
	for (size_t kick = 0; kick < 2; ++kick)
	{
		const auto* steps = &agenda[kick * 5];
		EXPECT_EQ(steps[0].movement.kind, Movement::Kind::GoNearObject);
		EXPECT_EQ(steps[0].movement.object, 7u);
		EXPECT_FLOAT_EQ(steps[0].movement.maxDistance, 0.0f);
		EXPECT_EQ(steps[1].movement.kind, Movement::Kind::TurnToFaceObject);
		EXPECT_FLOAT_EQ(steps[1].seconds, 0.1f);
		EXPECT_EQ(steps[2].movement.kind, Movement::Kind::GoNearObject);
		EXPECT_EQ(steps[3].order.kind, ObjectOrder::Kind::Kick);
		EXPECT_EQ(steps[3].order.object, 7u);
		EXPECT_EQ(steps[4].order.kind, ObjectOrder::Kind::PointAt);
		EXPECT_EQ(steps[4].order.seconds, 1.0f);
	}
	EXPECT_EQ(agenda[10].animation, creature_layers::animations::k_Happy);
}

TEST(CreatureKindActions, KickingABallAboutWaitsAndDrawsTheCountAfreshEachKick)
{
	// Answers in turn: no point (1), wait 4 + 5 turns, count 2 + 2 (go on); no point, wait 4, count 2 + 0 (stop at 2);
	// no jump
	const std::array<uint32_t, 7> answers {1, 5, 2, 1, 0, 0, 1};
	size_t next = 0;
	const auto agenda = KickBallAround(3, [&](uint32_t) { return answers.at(next++); });
	ASSERT_EQ(agenda.size(), 10u);
	EXPECT_EQ(agenda[4].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[4].seconds, 0.9f);
	EXPECT_EQ(agenda[9].kind, Step::Kind::Wait);
	EXPECT_FLOAT_EQ(agenda[9].seconds, 0.4f);
	EXPECT_EQ(next, answers.size());
}

TEST(CreatureKindActions, AKickFliesHalfUpHalfAheadOnFlatLand)
{
	const auto lean = creature_object_actions::LeanOf(5.0f, 5.0f, 5.0f, 5.0f, 2.0f);
	EXPECT_EQ(lean, glm::vec2(0.0f));
	const auto velocity = creature_object_actions::KickVelocity({0.0f, -1.0f}, lean, 2.0f);
	const float component = 90.0f / std::sqrt(2.0f);
	EXPECT_NEAR(velocity.x, 0.0f, 1e-4f);
	EXPECT_NEAR(velocity.y, component, 1e-3f);
	EXPECT_NEAR(velocity.z, -component, 1e-3f);
}

TEST(CreatureKindActions, AKickTipsUpWithTheLandAheadRisingAndTheLeanIsCapped)
{
	// The land rises 1 over 4 along x: 1 x 0.6 / 4 = 0.15; along z it rises 10, capped at 0.3
	const auto lean = creature_object_actions::LeanOf(6.0f, 5.0f, 15.0f, 5.0f, 2.0f);
	EXPECT_NEAR(lean.x, 0.15f, 1e-6f);
	EXPECT_NEAR(lean.y, 0.3f, 1e-6f);
	EXPECT_NEAR(creature_object_actions::LeanOf(0.0f, 10.0f, 0.0f, 0.0f, 1.0f).x, -0.3f, 1e-6f);
	// Facing +x, the up part is 1 + 0.15
	const auto velocity = creature_object_actions::KickVelocity({1.0f, 0.0f}, lean, 1.0f);
	const auto length = std::sqrt(1.0f + (1.15f * 1.15f));
	EXPECT_NEAR(velocity.x, 45.0f / length, 1e-3f);
	EXPECT_NEAR(velocity.y, 45.0f * 1.15f / length, 1e-3f);
	EXPECT_NEAR(velocity.z, 0.0f, 1e-4f);
}

TEST(CreatureKindActions, BreakingARockThrowsItOffToOneSideAndBreaksItAsItFlies)
{
	const auto agenda = BreakRock(7u, 7u, {100.0f, 200.0f}, 10.0f);
	ASSERT_EQ(agenda.size(), 3);
	EXPECT_EQ(agenda[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(agenda[1].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(agenda[1].order.point, glm::vec2(130.0f, 230.0f));
	EXPECT_EQ(agenda[2].kind, Step::Kind::Shatter);
	EXPECT_EQ(agenda[2].object, 7u);
	// Holding something else, it throws that and breaks the rock where it lies
	const auto holding = BreakRock(std::nullopt, 9u, {0.0f, 0.0f}, 10.0f);
	ASSERT_EQ(holding.size(), 2);
	EXPECT_EQ(holding[1].object, 9u);

	const auto* breakRock = plan_actions::For("BreakRock");
	ASSERT_NE(breakRock, nullptr);
	EXPECT_EQ(breakRock->target, plan_actions::Target::PickableRock);
	EXPECT_EQ(breakRock->build, plan_actions::Build::BreakRock);
}
