/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ECS/CreatureAttitudeRules.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace attitude = openblack::ecs::creature_attitude;
using attitude::Attitude;

namespace
{
constexpr uint32_t k_TurnsPerSecond = 10;

attitude::Watch TownWatch(Attitude feeling)
{
	return {.withinReach = true,
	        .hasTown = true,
	        .attitude = feeling,
	        .sameOwner = true,
	        .creaturePriority = 100,
	        .fleePriority = 90,
	        .followPriority = 60};
}
} // namespace

// Throwing, the destructive miracles and kicking frighten a town; gifts, help and kind miracles earn its respect; eating
// frightens it only when one of its people is eaten; walking about shows nothing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, ActionsShowFearRespectOrNothing)
{
	EXPECT_EQ(attitude::AttitudeShown(15, false), Attitude::Fear);    // hurl
	EXPECT_EQ(attitude::AttitudeShown(46, false), Attitude::Fear);    // fireball
	EXPECT_EQ(attitude::AttitudeShown(236, false), Attitude::Fear);   // kick
	EXPECT_EQ(attitude::AttitudeShown(34, false), Attitude::Respect); // help build a house
	EXPECT_EQ(attitude::AttitudeShown(38, false), Attitude::Respect); // stroke
	EXPECT_EQ(attitude::AttitudeShown(327, false), Attitude::Respect);
	EXPECT_EQ(attitude::AttitudeShown(11, true), Attitude::Fear); // eating one of the people alive
	EXPECT_EQ(attitude::AttitudeShown(11, false), std::nullopt);
	EXPECT_EQ(attitude::AttitudeShown(276, true), Attitude::Fear);
	EXPECT_EQ(attitude::AttitudeShown(1, false), std::nullopt); // walking somewhere
	EXPECT_EQ(attitude::AttitudeShown(0, true), std::nullopt);
}

// A town is curious about a creature for 15 whole seconds, then indifferent
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, CuriosityLapsesAfterFifteenSeconds)
{
	auto feeling = attitude::StartWatching(100);
	EXPECT_FALSE(attitude::Turn(feeling, std::nullopt, 259, k_TurnsPerSecond));
	EXPECT_EQ(feeling.attitude, Attitude::Curious);
	attitude::Turn(feeling, std::nullopt, 260, k_TurnsPerSecond);
	EXPECT_EQ(feeling.attitude, Attitude::Indifferent);
	EXPECT_EQ(feeling.lapsedTurn, 260u);
}

// Fear and respect replace each other and are told; fear shown again starts its 30 seconds afresh, respect shown again
// doesn't, and respect lapses after 10
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, FearRefreshesRespectLapses)
{
	auto feeling = attitude::StartWatching(0);
	EXPECT_TRUE(attitude::Turn(feeling, Attitude::Respect, 10, k_TurnsPerSecond));
	EXPECT_FALSE(attitude::Turn(feeling, Attitude::Respect, 100, k_TurnsPerSecond));
	EXPECT_EQ(feeling.shownTurn, 10u);
	attitude::Turn(feeling, std::nullopt, 120, k_TurnsPerSecond);
	EXPECT_EQ(feeling.attitude, Attitude::Indifferent);

	EXPECT_TRUE(attitude::Turn(feeling, Attitude::Fear, 200, k_TurnsPerSecond));
	EXPECT_FALSE(attitude::Turn(feeling, Attitude::Fear, 450, k_TurnsPerSecond));
	EXPECT_EQ(feeling.shownTurn, 450u);
	attitude::Turn(feeling, std::nullopt, 759, k_TurnsPerSecond);
	EXPECT_EQ(feeling.attitude, Attitude::Fear);
	attitude::Turn(feeling, std::nullopt, 760, k_TurnsPerSecond);
	EXPECT_EQ(feeling.attitude, Attitude::Indifferent);
}

// Curious villagers heed a creature half as much again as its row says, frightened ones twice the running away row,
// indifferent ones not at all; dancers, an invisible creature and one out of reach not at all
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, VillagerPriorityFollowsTheTownsFeeling)
{
	EXPECT_EQ(attitude::VillagerPriority(TownWatch(Attitude::Curious)), 150);
	EXPECT_EQ(attitude::VillagerPriority(TownWatch(Attitude::Fear)), 180);
	EXPECT_EQ(attitude::VillagerPriority(TownWatch(Attitude::Indifferent)), 0);
	auto watch = TownWatch(Attitude::Curious);
	watch.dancing = true;
	EXPECT_EQ(attitude::VillagerPriority(watch), 0);
	watch = TownWatch(Attitude::Curious);
	watch.creatureInvisible = true;
	EXPECT_EQ(attitude::VillagerPriority(watch), 0);
	watch = TownWatch(Attitude::Curious);
	watch.withinReach = false;
	EXPECT_EQ(attitude::VillagerPriority(watch), 0);
	watch = TownWatch(Attitude::Indifferent);
	watch.hasTown = false;
	EXPECT_EQ(attitude::VillagerPriority(watch), 100);
	// The byte wraps as the game's does
	watch = TownWatch(Attitude::Fear);
	watch.fleePriority = 200;
	EXPECT_EQ(attitude::VillagerPriority(watch), 144);
}

// Respect draws a villager by the follow row's priority unless it is a disciple or busy with something its town wants;
// another player's creature draws it whatever it does
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, RespectDrawsVillagersWithNothingBetterToDo)
{
	auto watch = TownWatch(Attitude::Respect);
	EXPECT_EQ(attitude::VillagerPriority(watch), 60);
	watch.wanted = 0.0f;
	EXPECT_EQ(attitude::VillagerPriority(watch), 60);
	watch.wanted = 0.25f;
	EXPECT_EQ(attitude::VillagerPriority(watch), 0);
	watch.wanted.reset();
	watch.disciple = true;
	EXPECT_EQ(attitude::VillagerPriority(watch), 0);
	watch.sameOwner = false;
	EXPECT_EQ(attitude::VillagerPriority(watch), 60);
}

// Reacting lasts longer the nearer and more impressive the creature, and four times as long when it is feared; waiting
// to react again is shorter, and none while the town fears or respects it or the creature means something impressive
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, TimingsByNearnessImpressivenessAndFeeling)
{
	attitude::Timing timing {.tableTurns = 100,
	                         .maxReactionDistance = 30.0f,
	                         .howImportantIsDistance = 0.5f,
	                         .distance = 20.0f,
	                         .attitude = Attitude::Curious};
	// (40 - 20) * 0.5 / 40 + 0.5 = 0.75
	EXPECT_EQ(attitude::TurnsToReact(timing), 75u);
	timing.impressive = ImpressiveType::CreatureQuiteImpressive;
	EXPECT_EQ(attitude::TurnsToReact(timing), 150u);
	timing.attitude = Attitude::Fear;
	EXPECT_EQ(attitude::TurnsToReact(timing), 600u);

	timing.impressive = ImpressiveType::CreatureNotImpressive;
	timing.attitude = Attitude::Curious;
	// 0.5 * 20 / 40 + 0.5 = 0.75
	EXPECT_EQ(attitude::TurnsBeforeReactingAgain(timing, false), 75u);
	timing.impressive = ImpressiveType::CreatureVeryImpressive;
	EXPECT_EQ(attitude::TurnsBeforeReactingAgain(timing, false), 25u);
	EXPECT_EQ(attitude::TurnsBeforeReactingAgain(timing, true), 0u);
	timing.attitude = Attitude::Respect;
	EXPECT_EQ(attitude::TurnsBeforeReactingAgain(timing, false), 0u);
}

// A curious villager steps towards the creature by the whole metres it is too far; within a metre of where it means to
// be it steps no more
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, StepsCloserByWholeMetres)
{
	const auto step = attitude::StepCloser({100.0f, 100.0f}, {130.6f, 100.0f}, 20.0f);
	ASSERT_TRUE(step.has_value());
	EXPECT_NEAR(step->x, 110.0f, 0.01f);
	EXPECT_NEAR(step->y, 100.0f, 0.01f);
	EXPECT_FALSE(attitude::StepCloser({100.0f, 100.0f}, {120.9f, 100.0f}, 20.0f).has_value());
}

// A frightened villager runs straight away; one bowing turns again only past an eighth of a turn, the angles taken as they
// are
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureAttitudeRules, FleesStraightAwayAndBowsFacingTheCreature)
{
	const auto to = attitude::FleePoint({100.0f, 100.0f}, {100.0f, 90.0f}, 36.0f);
	EXPECT_NEAR(to.x, 100.0f, 0.001f);
	EXPECT_NEAR(to.y, 136.0f, 0.001f);
	EXPECT_FALSE(attitude::DriftedFrom(1000, 1256));
	EXPECT_TRUE(attitude::DriftedFrom(1000, 1257));
	EXPECT_TRUE(attitude::DriftedFrom(2040, 10));
	EXPECT_EQ(attitude::TurnsIn(1.5f, k_TurnsPerSecond), 15);
}
