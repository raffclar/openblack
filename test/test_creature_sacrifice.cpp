/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;
namespace sacrifice = openblack::creature_sacrifice;

namespace
{
constexpr uint32_t k_Villager = 42;
constexpr glm::vec3 k_Altar {100.0f, 7.0f, -20.0f};
constexpr float k_Height = 15.0f;

uint32_t First(uint32_t /*range*/)
{
	return 0;
}

const plan_actions::Executor& Sacrifice()
{
	const auto* action = plan_actions::For("Sacrifice");
	EXPECT_NE(action, nullptr);
	static const plan_actions::Executor k_None {};
	return action != nullptr ? *action : k_None;
}

plan_actions::Situation WithAltar()
{
	plan_actions::Situation situation;
	situation.hasTemple = true;
	situation.sacrificeAltar = k_Altar;
	situation.height = k_Height;
	return situation;
}

void ExpectWalkTurnAndPutDown(const std::vector<Step>& agenda, size_t from)
{
	ASSERT_EQ(agenda.size(), from + 3);
	const glm::vec2 ground {k_Altar.x, k_Altar.z};
	const auto& walk = agenda.at(from);
	EXPECT_EQ(walk.kind, Step::Kind::Move);
	EXPECT_EQ(walk.movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(walk.movement.point, ground);
	EXPECT_FLOAT_EQ(walk.movement.maxDistance, 3.0f * k_Height);
	ASSERT_TRUE(walk.gaze.has_value());
	EXPECT_TRUE(walk.gaze->whileGoing);
	EXPECT_EQ(walk.face, creature_face::Cue::None);

	const auto& turn = agenda.at(from + 1);
	EXPECT_EQ(turn.kind, Step::Kind::Move);
	EXPECT_EQ(turn.movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(turn.movement.point, ground);
	ASSERT_TRUE(turn.gaze.has_value());
	EXPECT_EQ(turn.gaze->point, k_Altar);
	EXPECT_FALSE(turn.gaze->whileGoing);

	const auto& putDown = agenda.at(from + 2);
	EXPECT_EQ(putDown.kind, Step::Kind::Object);
	EXPECT_EQ(putDown.order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ(putDown.order.animation, creature_throw::k_PutDown);
	EXPECT_EQ(putDown.face, creature_face::Cue::None);
	EXPECT_FALSE(putDown.gaze.has_value());
	EXPECT_EQ(putDown.effect, Effect::None);
}
} // namespace

TEST(CreatureSacrifice, TheFirstPlaceWithASiteIsTheLowest)
{
	EXPECT_EQ(sacrifice::SitePlace(std::array {false, false, true, false, true, false}), 2u);
	EXPECT_EQ(sacrifice::SitePlace(std::array {true, true, true, true, true, true}), 0u);
	EXPECT_EQ(sacrifice::SitePlace(std::array {false, false, false, false, false, true}), 5u);
}

TEST(CreatureSacrifice, NoPlaceWithASiteGivesNoPlace)
{
	EXPECT_FALSE(sacrifice::SitePlace(std::array {false, false, false, false, false, false}).has_value());
}

TEST(CreatureSacrifice, PicksUpTheThingThenTakesItToTheAltar)
{
	const auto agenda = sacrifice::Sacrifice(k_Villager, k_Altar, k_Height);
	ASSERT_EQ(agenda.size(), 4u);
	const auto& pickUp = agenda.front();
	EXPECT_EQ(pickUp.kind, Step::Kind::Object);
	EXPECT_EQ(pickUp.order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(pickUp.order.object, k_Villager);
	EXPECT_EQ(pickUp.face, creature_face::Cue::Anger);
	ASSERT_TRUE(pickUp.gaze.has_value());
	EXPECT_EQ(pickUp.gaze->object, k_Villager);
	EXPECT_TRUE(pickUp.gaze->whileGoing);
	ExpectWalkTurnAndPutDown(agenda, 1);
}

TEST(CreatureSacrifice, AThingAlreadyHeldIsNotPickedUp)
{
	ExpectWalkTurnAndPutDown(sacrifice::Sacrifice(std::nullopt, k_Altar, k_Height), 0);
}

TEST(CreatureSacrifice, TheActionTakesALivingThing)
{
	const auto& action = Sacrifice();
	EXPECT_EQ(action.target, plan_actions::Target::AnyLiving);
	EXPECT_EQ(action.use, plan_actions::Target::None);
	EXPECT_FALSE(plan_actions::NeedsThingToUse("Sacrifice"));
}

TEST(CreatureSacrifice, OnlyACreatureWithATempleCanDoIt)
{
	auto situation = WithAltar();
	EXPECT_TRUE(plan_actions::Possible(Sacrifice(), situation));
	situation.hasTemple = false;
	EXPECT_FALSE(plan_actions::Possible(Sacrifice(), situation));
	EXPECT_FALSE(plan_actions::Agenda(Sacrifice(), k_Villager, {}, situation, First).has_value());
}

TEST(CreatureSacrifice, ATempleWithNoWorshipSiteGivesNoAgenda)
{
	auto situation = WithAltar();
	situation.sacrificeAltar.reset();
	EXPECT_FALSE(plan_actions::Agenda(Sacrifice(), k_Villager, {}, situation, First).has_value());
}

TEST(CreatureSacrifice, TheAgendaPicksUpUnlessTheHandHoldsALivingThing)
{
	auto situation = WithAltar();
	situation.handFull = true;
	auto agenda = plan_actions::Agenda(Sacrifice(), k_Villager, {}, situation, First);
	ASSERT_TRUE(agenda.has_value());
	EXPECT_EQ(agenda->size(), 4u);

	situation.holdingLiving = true;
	agenda = plan_actions::Agenda(Sacrifice(), k_Villager, {}, situation, First);
	ASSERT_TRUE(agenda.has_value());
	ExpectWalkTurnAndPutDown(*agenda, 0);
}
