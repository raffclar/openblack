/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureTeleportAgendas.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t First(uint32_t /*range*/)
{
	return 0;
}

void ExpectWalkTo(const Step& step, glm::vec2 point)
{
	EXPECT_EQ(step.kind, Step::Kind::Move);
	EXPECT_EQ(step.movement.kind, Movement::Kind::ToPoint);
	EXPECT_EQ(step.movement.point, point);
	EXPECT_FALSE(step.movement.object.has_value());
	EXPECT_FALSE(step.movement.run);
	EXPECT_FLOAT_EQ(step.movement.minDistance, 0.0f);
	EXPECT_FLOAT_EQ(step.movement.maxDistance, 5.0f);
	EXPECT_TRUE(step.movement.giveUpIfUnreachable);
	ASSERT_TRUE(step.gaze.has_value());
	EXPECT_TRUE(step.gaze->pointOnGround);
	ASSERT_TRUE(step.gaze->point.has_value());
	EXPECT_EQ(step.gaze->point->x, point.x);
	EXPECT_EQ(step.gaze->point->z, point.y);
}
} // namespace

TEST(CreatureTeleport, TheCreatureWalksPuzzledToTheStoneAndAsksForTheJump)
{
	// The walk goes to the stone's map position in metres: 12.34567 m is not a whole map unit
	const glm::vec2 stone {12.34567f, -40.5f};
	const auto agenda = GoToTeleport(9u, stone);
	ASSERT_EQ(agenda.size(), 2u);
	const glm::vec2 onMap {map_coords::ToMetres(map_coords::ToFixed(stone.x)),
	                       map_coords::ToMetres(map_coords::ToFixed(stone.y))};
	EXPECT_NE(onMap.x, stone.x);
	ExpectWalkTo(agenda[0], onMap);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Puzzled);
	EXPECT_FALSE(agenda[0].main);
	// The jump is the main step. No wait comes after it.
	EXPECT_EQ(agenda[1].kind, Step::Kind::Teleport);
	EXPECT_EQ(agenda[1].object, std::optional<uint32_t>(9u));
	EXPECT_TRUE(agenda[1].main);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::None);
	EXPECT_EQ(MainStep(agenda), 1u);
}

TEST(CreatureTeleport, AfterTheJumpTheStoneCarriesTheCreatureThenItWalksOn)
{
	const glm::vec2 destination {30.0f, 60.0f};
	const auto agenda = AfterTeleport(destination);
	ASSERT_EQ(agenda.size(), 3u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Transported);
	ExpectWalkTo(agenda[1], destination);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::None);
	EXPECT_EQ(agenda[2].kind, Step::Kind::StopTeleportControl);

	// The area that holds the creature refused the walk: the stone still lets the creature go
	const auto refused = AfterTeleport(std::nullopt);
	ASSERT_EQ(refused.size(), 2u);
	EXPECT_EQ(refused[0].kind, Step::Kind::Transported);
	EXPECT_EQ(refused[1].kind, Step::Kind::StopTeleportControl);
}

TEST(CreatureTeleport, TheGameActionBuildsTheWalkToTheStone)
{
	const auto* action = plan_actions::For(k_GoToTeleportAction);
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->build, plan_actions::Build::GoToTeleport);
	const plan_actions::Situation situation;
	// Only a stone starts the action: with no stone there is no agenda
	EXPECT_FALSE(plan_actions::Agenda(*action, std::nullopt, {}, situation, First).has_value());
	const auto agenda = plan_actions::Agenda(*action, 4u, {10.0f, 20.0f}, situation, First);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 2u);
	EXPECT_EQ((*agenda)[1].object, std::optional<uint32_t>(4u));
}

TEST(CreatureTeleport, TheJumpStartsWhileTheBodyActsAndDoesNotEndByItself)
{
	IdleMind mind;
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Teleport, .object = 3u, .main = true}});
	auto commands = Think(mind, {.seconds = 0.1f, .bodyBusy = true}, First);
	EXPECT_EQ(commands.teleport, std::optional<uint32_t>(3u));
	EXPECT_TRUE(mind.stepStarted);
	// A jump replaces the agenda, and a refused jump gives it up: the step itself waits
	commands = Think(mind, {.seconds = 0.1f}, First);
	EXPECT_FALSE(commands.teleport.has_value());
	EXPECT_EQ(mind.step, 0u);
	EXPECT_EQ(HandsNeededFor(Step {.kind = Step::Kind::Teleport}), HandsNeed::Any);
}

TEST(CreatureTeleport, TheCarryStartsThenMovesOnEachTurnUntilTheStoneSaysItIsDone)
{
	IdleMind mind;
	Plan(mind, Activity::Told, AfterTeleport(glm::vec2 {1.0f, 2.0f}));
	auto commands = Think(mind, {.seconds = 0.1f, .bodyBusy = true}, First);
	EXPECT_EQ(commands.carry, Commands::Carry::Start);
	for (int turn = 0; turn < 3; ++turn)
	{
		commands = Think(mind, {.seconds = 0.1f}, First);
		EXPECT_EQ(commands.carry, Commands::Carry::Turn);
		EXPECT_EQ(mind.step, 0u);
	}
	CarryDone(mind);
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(mind.stepStarted);
	// The walk on starts on the next turn
	commands = Think(mind, {.seconds = 0.1f}, First);
	EXPECT_EQ(commands.carry, Commands::Carry::None);
	ASSERT_TRUE(commands.move.has_value());
	// It is done once it has arrived. Then the stone lets it go, and the agenda is over.
	commands = Think(mind, {.seconds = 0.1f, .moving = false}, First);
	EXPECT_EQ(mind.step, 2u);
	commands = Think(mind, {.seconds = 0.1f, .bodyBusy = true}, First);
	EXPECT_TRUE(commands.stopTeleportControl);
	EXPECT_EQ(mind.step, 3u);
}

TEST(CreatureTeleport, ACarryDoneOutsideTheCarryChangesNothing)
{
	IdleMind mind;
	Plan(mind, Activity::Told, AfterTeleport(glm::vec2 {1.0f, 2.0f}));
	// Not started yet
	CarryDone(mind);
	EXPECT_EQ(mind.step, 0u);
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Wait, .seconds = 1.0f}});
	static_cast<void>(Think(mind, {.seconds = 0.1f}, First));
	CarryDone(mind);
	EXPECT_EQ(mind.step, 0u);
}

TEST(CreatureTeleport, TheStoneControlsTheCreatureOnlyWhileItsPlanHasAStepOfTheTeleportLeft)
{
	IdleMind mind;
	EXPECT_FALSE(StoneControls(mind));
	Plan(mind, Activity::Told, GoToTeleport(9u, glm::vec2 {1.0f, 2.0f}));
	EXPECT_TRUE(StoneControls(mind));
	// After the jump: the carry, the walk on and the last step
	Plan(mind, Activity::Told, AfterTeleport(glm::vec2 {1.0f, 2.0f}));
	EXPECT_TRUE(StoneControls(mind));
	mind.step = 2;
	EXPECT_TRUE(StoneControls(mind));
	// The last step is done: the stone lets the creature go
	mind.step = 3;
	EXPECT_FALSE(StoneControls(mind));
	// A new plan with no teleport step stops the control at that time
	Plan(mind, Activity::Told, AfterTeleport(std::nullopt));
	EXPECT_TRUE(StoneControls(mind));
	Plan(mind, Activity::Planned, {{.kind = Step::Kind::Wait, .seconds = 1.0f}});
	EXPECT_FALSE(StoneControls(mind));
	// A plan given up or let go of has no step left
	Plan(mind, Activity::Told, GoToTeleport(9u, glm::vec2 {1.0f, 2.0f}));
	mind.agenda.clear();
	mind.step = 0;
	EXPECT_FALSE(StoneControls(mind));
}
