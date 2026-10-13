/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
