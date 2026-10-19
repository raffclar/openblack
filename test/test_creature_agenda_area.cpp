/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureAgendaArea.h"

using namespace openblack;
using namespace openblack::creature_agenda_area;
using creature_mind::Movement;
using creature_mind::Step;

namespace
{
constexpr creature_leash::AgendaArea k_Area {.radius = 10.0f, .centre = {100.0f, 200.0f}};
constexpr glm::vec2 k_Creature {100.0f, 200.0f};

Step WalkTo(glm::vec2 point, bool main = false)
{
	Step step {.kind = Step::Kind::Move, .main = main};
	step.movement = {.kind = Movement::Kind::ToPoint, .point = point};
	return step;
}

Step Wait(bool main = false)
{
	return Step {.kind = Step::Kind::Wait, .seconds = 1.0f, .main = main};
}
} // namespace

TEST(CreatureAgendaArea, TheAreaRefusesOnlyAWalkToAPointOutsideIt)
{
	EXPECT_TRUE(RefusesStep(k_Area, k_Creature, WalkTo({120.0f, 200.0f})));
	EXPECT_FALSE(RefusesStep(k_Area, k_Creature, WalkTo({105.0f, 200.0f})));
	// A walk to a thing is never refused
	auto toThing = WalkTo({120.0f, 200.0f});
	toThing.movement.object = 7u;
	EXPECT_FALSE(RefusesStep(k_Area, k_Creature, toThing));
	// A walk near the creature starts from the creature
	Step nearby {.kind = Step::Kind::Move};
	nearby.movement = {.kind = Movement::Kind::Nearby, .point = {6.0f, 0.0f}};
	EXPECT_FALSE(RefusesStep(k_Area, k_Creature, nearby));
	EXPECT_TRUE(RefusesStep(k_Area, {105.0f, 200.0f}, nearby));
	EXPECT_FALSE(RefusesStep(k_Area, k_Creature, Wait()));
}

TEST(CreatureAgendaArea, AnAgendaOfAnActionFailsWhenOneWalkLeavesTheArea)
{
	const std::vector agenda {Wait(), WalkTo({105.0f, 200.0f}), WalkTo({130.0f, 200.0f})};
	EXPECT_TRUE(RefusesAgenda(k_Area, k_Creature, agenda));
	EXPECT_FALSE(RefusesAgenda(k_Area, k_Creature, std::vector {Wait(), WalkTo({105.0f, 200.0f})}));
	// A creature that walks back into its area has no walk refused
	auto back = k_Area;
	back.walkingBack = true;
	EXPECT_FALSE(RefusesAgenda(back, k_Creature, agenda));
}

TEST(CreatureAgendaArea, AForcedPlanWithItsOwnStepsOnlyLosesTheRefusedWalks)
{
	const auto kept =
	    WithoutRefusedWalks(k_Area, k_Creature, {Wait(), WalkTo({130.0f, 200.0f}), WalkTo({105.0f, 200.0f}), Wait()});
	ASSERT_EQ(kept.size(), 3u);
	EXPECT_EQ(kept[0].kind, Step::Kind::Wait);
	EXPECT_EQ(kept[1].kind, Step::Kind::Move);
	EXPECT_EQ(kept[2].kind, Step::Kind::Wait);
}

TEST(CreatureAgendaArea, ARefusedMainWalkMakesTheStepBeforeItTheMainStep)
{
	const auto kept = WithoutRefusedWalks(k_Area, k_Creature, {Wait(), WalkTo({130.0f, 200.0f}, true), Wait()});
	ASSERT_EQ(kept.size(), 2u);
	EXPECT_TRUE(kept[0].main);
	EXPECT_FALSE(kept[1].main);
	EXPECT_EQ(creature_mind::MainStep(kept), 0u);
}
