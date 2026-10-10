/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <memory>
#include <vector>

#include <DanceFile.h>
#include <gtest/gtest.h>

#include "ECS/Components/Dance.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Dance;
using openblack::ecs::components::DanceGroup;
namespace rules = openblack::ecs::dance_rules;

namespace
{
void Put(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutAction(std::vector<uint8_t>& bytes, std::vector<uint32_t> groups, uint32_t type, uint32_t first, uint32_t second)
{
	Put(bytes, static_cast<uint32_t>(groups.size()));
	for (const auto group : groups)
	{
		Put(bytes, group);
	}
	Put(bytes, type);
	Put(bytes, first);
	Put(bytes, second);
	for (int i = 2; i < 14; ++i)
	{
		Put(bytes, 0);
	}
}

/// A dance of two groups named M and W, sharing their dancers half and half from its first key frame
std::vector<uint8_t> TwoGroupDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 1); // version
	Put(bytes, 2); // key frames
	Put(bytes, std::bit_cast<uint32_t>(0.0f));
	Put(bytes, 1);
	Put(bytes, 2); // actions
	PutAction(bytes, {0}, 6, 0, 50);
	PutAction(bytes, {1}, 6, 0, 50);
	Put(bytes, std::bit_cast<uint32_t>(20.0f));
	Put(bytes, 0);
	Put(bytes, 1);
	PutAction(bytes, {1}, 16, 2, 0);
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // clock
	Put(bytes, 2);                             // groups
	Put(bytes, 14);
	Put(bytes, 0);
	Put(bytes, 1); // loops
	for (const char name : {'M', 'W'})
	{
		Put(bytes, 1);
		bytes.push_back(static_cast<uint8_t>(name));
	}
	return bytes;
}

entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}
} // namespace

TEST(DanceRules, TheRateGoesInStepsOfFourTenths)
{
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.5f), 2.0f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.25f), 0.8f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(0.99f), 3.6f);
	EXPECT_FLOAT_EQ(rules::RateForSpeed(1.0f), 4.0f);
}

TEST(DanceRules, AWorshipSitesDanceIsDancedWhileItsDancersChant)
{
	Dance dance;
	// As it is made: at a quarter speed, stopped
	rules::SetSpeed(dance, rules::k_MadeSpeed);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_FLOAT_EQ(dance.dancingRate, 1.0f);
	// Then set going at half speed: danced, starting over at the new rate
	dance.clock = 12.0f;
	rules::SetWorshipSpeed(dance, 0.5f);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.dancingRate, 2.0f);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// The same rate again goes on where it was
	dance.clock = 5.0f;
	rules::SetWorshipSpeed(dance, 0.55f);
	EXPECT_FLOAT_EQ(dance.clock, 5.0f);
	rules::SetWorshipSpeed(dance, 0.0f);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
}

TEST(DanceRules, ADanceStartsOnceMoreThanHalfOfThoseOnTheirWayHaveComeOrAfterTheLongestWait)
{
	Dance dance;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	EXPECT_FALSE(dance.firstDancerTurn.has_value());
	dance.dancers = 2;
	dance.onTheirWay = 4;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	EXPECT_EQ(dance.firstDancerTurn, 100u);
	dance.dancers = 3;
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 101));
	dance.dancers = 1;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100 + 899));
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 100 + 900));
}

TEST(DanceRules, AScriptsDanceStartsOnceItHasADancer)
{
	// None are ever on their way to a script's dance, so its first dancer will do
	Dance dance;
	EXPECT_FALSE(rules::HasProperlyStarted(dance, 100));
	dance.dancers = 1;
	EXPECT_TRUE(rules::HasProperlyStarted(dance, 120));
	EXPECT_EQ(dance.firstDancerTurn, 120u);
}

TEST(DanceRules, TheClockRunsWhileItIsDancedAndStartsOverAfterItsLoop)
{
	Dance dance {.autostart = true, .loopLength = 1};
	EXPECT_EQ(rules::LoopTurns(dance), 600u);
	dance.dancers = 1;
	rules::ProcessTurn(dance, 50);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_EQ(dance.startTurn, 50u);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	dance.clock = 598.0f;
	rules::ProcessTurn(dance, 51);
	EXPECT_FLOAT_EQ(dance.clock, 599.0f);
	rules::ProcessTurn(dance, 52);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// No dancers, no clock
	dance.dancers = 0;
	rules::ProcessTurn(dance, 53);
	EXPECT_FLOAT_EQ(dance.clock, 0.0f);
	// One that doesn't start by itself waits to be started
	Dance waiting {.loopLength = 1, .dancers = 5};
	rules::ProcessTurn(waiting, 10);
	EXPECT_EQ(waiting.state, Dance::State::Stopped);
}

TEST(DanceRules, KeyFramesComeByTheHalfSecondAndTheClockLoops)
{
	EXPECT_TRUE(rules::KeyFrameDue(20.0f, 24.0f));
	EXPECT_FALSE(rules::KeyFrameDue(20.0f, 25.0f));
	EXPECT_FLOAT_EQ(rules::NextClock(5.0f, 1), 6.0f);
	EXPECT_FLOAT_EQ(rules::NextClock(599.0f, 1), 0.0f);
	EXPECT_FLOAT_EQ(rules::NextClock(599.0f, 2), 600.0f);
}

TEST(DanceRules, SharedGroupsTakeNewcomersInTurnByTheirShares)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(TwoGroupDance()), dance::DanceResult::Success);
	Dance dance;
	dance.groups.all.resize(2);
	rules::ApplyKeyFramesUpTo(dance.groups, file, 0.0f);
	// Half and half: one each in a round of two
	EXPECT_EQ(dance.groups.shared, (std::vector<std::size_t> {0, 1}));
	EXPECT_EQ(dance.groups.all[0].weight, 1u);
	EXPECT_EQ(dance.groups.roundLength, 2u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(2), 0, DanceGroup::k_Women), 1u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(3), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(dance.dancers, 3u);
	// A dancer of another kind finds no group
	EXPECT_FALSE(rules::AddDancer(dance, Entity(4), 1, DanceGroup::k_Men).has_value());
}

TEST(DanceRules, AGroupTakingOneSexPassesItsTurnOnToTheNext)
{
	Dance dance;
	auto& groups = dance.groups;
	groups.all.resize(2);
	groups.all[0].sexes = DanceGroup::k_Women;
	groups.shared = {0, 1};
	groups.all[0].quota = 50;
	groups.all[1].quota = 50;
	rules::SetWeights(groups);
	// A man on the women's turn goes to the second group, and the round moves on
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 1u);
	EXPECT_EQ(groups.round, 1);
}

TEST(DanceRules, GroupsWithAFixedNumberAreFilledFirst)
{
	Dance dance;
	auto& groups = dance.groups;
	groups.all.resize(2);
	groups.all[0].limited = true;
	groups.all[0].quota = 1;
	groups.limited = {0};
	groups.all[1].quota = 100;
	groups.shared = {1};
	rules::SetWeights(groups);
	EXPECT_EQ(rules::AddDancer(dance, Entity(1), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(2), 0, DanceGroup::k_Men), 1u);
	rules::RemoveDancer(dance, 0, Entity(1));
	EXPECT_EQ(groups.all[0].limitedDancers, 0u);
	EXPECT_EQ(dance.dancers, 1u);
	EXPECT_EQ(rules::AddDancer(dance, Entity(3), 0, DanceGroup::k_Men), 0u);
	EXPECT_EQ(rules::FirstDancer(groups, Entity(3)), Entity(2));
}

TEST(Dances, ADanceIsMadeStoppedAtAQuarterSpeedWithItsFilesGroups)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity = dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true}, file);
	const auto& dance = registry.Get<const Dance>(entity);
	EXPECT_EQ(dance.state, Dance::State::Stopped);
	EXPECT_FLOAT_EQ(dance.speed, 0.25f);
	EXPECT_FLOAT_EQ(dance.rate, 0.8f);
	EXPECT_EQ(dance.loopLength, 1u);
	ASSERT_EQ(dance.groups.all.size(), 2u);
	EXPECT_EQ(dance.groups.all[0].name, "M");
	EXPECT_EQ(dance.groups.all[1].name, "W");
}

TEST(Dances, ADanceStopsForATurnWhenItsTimeIsUp)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity = dances::Create(
	    registry, {.type = DanceInfo::NewDanceAroundPerson, .autostart = true, .duration = 30, .madeByScript = true}, file);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, DanceGroup::k_Men));
	EXPECT_EQ(dances::DanceOf(registry, dancer), entity);
	std::vector<entt::entity> finished;
	dances::TurnContext context {.turn = 1,
	                             .available = [](entt::entity) { return true; },
	                             .finished = [&finished](entt::entity e) { finished.push_back(e); }};
	dances::ProcessTurn(registry, entity, context);
	const auto& dance = registry.Get<Dance>(entity);
	EXPECT_EQ(dance.state, Dance::State::Dancing);
	EXPECT_FLOAT_EQ(dance.clock, 1.0f);
	// 31 turns on it stops, and starts again the turn after
	context.turn = 32;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(registry.Get<Dance>(entity).state, Dance::State::Stopped);
	context.turn = 33;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_EQ(registry.Get<Dance>(entity).state, Dance::State::Dancing);
	// Its dancers are told when it goes
	dances::Destroy(registry, entity, context.finished);
	EXPECT_EQ(finished, std::vector<entt::entity> {dancer});
	EXPECT_FALSE(registry.AllOf<components::Dancer>(dancer));
}

TEST(Dances, ADanceGoesWhenWhatItIsDancedForGoes)
{
	Registry registry;
	const auto owner = registry.Create();
	const auto entity = dances::Create(registry, {.type = DanceInfo::CitadelDance_1, .owner = owner}, nullptr);
	dances::TurnContext context {.turn = 1, .available = [](entt::entity) { return false; }, .finished = {}};
	dances::ProcessTurn(registry, entity, context);
	EXPECT_FALSE(registry.Valid(entity));
}
