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
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // beat
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

TEST(DanceRules, SharedGroupsTakeNewcomersInTurnByTheirShares)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(TwoGroupDance()), dance::DanceResult::Success);
	rules::Groups groups;
	groups.all.resize(2);
	rules::ApplyKeyFramesUpTo(groups, file, 0.0f);
	// Half and half: one each in a round of two
	EXPECT_EQ(groups.shared, (std::vector<std::size_t> {0, 1}));
	EXPECT_EQ(groups.all[0].weight, 1u);
	EXPECT_EQ(groups.roundLength, 2u);
	EXPECT_EQ(rules::AddDancer(groups, Entity(1), 0, rules::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(groups, Entity(2), 0, rules::k_Women), 1u);
	EXPECT_EQ(rules::AddDancer(groups, Entity(3), 0, rules::k_Men), 0u);
	EXPECT_EQ(groups.dancers, 3u);
	// A dancer of another kind finds no group
	EXPECT_FALSE(rules::AddDancer(groups, Entity(4), 1, rules::k_Men).has_value());
}

TEST(DanceRules, AGroupTakingOneSexPassesItsTurnOnToTheNext)
{
	rules::Groups groups;
	groups.all.resize(2);
	groups.all[0].sexes = rules::k_Women;
	groups.shared = {0, 1};
	groups.all[0].quota = 50;
	groups.all[1].quota = 50;
	rules::SetWeights(groups);
	// A man on the women's turn goes to the second group, and the round moves on
	EXPECT_EQ(rules::AddDancer(groups, Entity(1), 0, rules::k_Men), 1u);
	EXPECT_EQ(groups.round, 1);
}

TEST(DanceRules, GroupsWithAFixedNumberAreFilledFirst)
{
	rules::Groups groups;
	groups.all.resize(2);
	groups.all[0].limited = true;
	groups.all[0].quota = 1;
	groups.limited = {0};
	groups.all[1].quota = 100;
	groups.shared = {1};
	rules::SetWeights(groups);
	EXPECT_EQ(rules::AddDancer(groups, Entity(1), 0, rules::k_Men), 0u);
	EXPECT_EQ(rules::AddDancer(groups, Entity(2), 0, rules::k_Men), 1u);
	rules::RemoveDancer(groups, 0, Entity(1));
	EXPECT_EQ(groups.all[0].limitedDancers, 0u);
	EXPECT_EQ(rules::AddDancer(groups, Entity(3), 0, rules::k_Men), 0u);
	EXPECT_EQ(rules::FirstDancer(groups, Entity(3)), Entity(2));
}

TEST(DanceRules, KeyFramesComeByTheHalfSecondAndTheBeatLoops)
{
	EXPECT_TRUE(rules::KeyFrameDue(20.0f, 24.0f, 10));
	EXPECT_FALSE(rules::KeyFrameDue(20.0f, 25.0f, 10));
	EXPECT_FLOAT_EQ(rules::NextBeat(5.0f, 1, 10), 6.0f);
	EXPECT_FLOAT_EQ(rules::NextBeat(599.0f, 1, 10), 0.0f);
	EXPECT_FLOAT_EQ(rules::NextBeat(599.0f, 2, 10), 600.0f);
}

TEST(DanceRules, ADanceStartsOnceItHasADancer)
{
	bool waiting = false;
	uint32_t since = 0;
	EXPECT_FALSE(rules::ReadyToStart(0, waiting, since, 100, 10));
	EXPECT_FALSE(waiting);
	EXPECT_TRUE(rules::ReadyToStart(1, waiting, since, 120, 10));
	EXPECT_EQ(since, 120u);
}

TEST(Dances, ADanceStopsForATurnWhenItsTimeIsUp)
{
	Registry registry;
	auto file = std::make_shared<dance::DanceFile>();
	ASSERT_EQ(file->Open(TwoGroupDance()), dance::DanceResult::Success);
	const auto entity =
	    dances::Create(registry, {.type = 14, .autostart = true, .durationTurns = 30, .madeByScript = true}, file);
	const auto dancer = registry.Create();
	ASSERT_TRUE(dances::AddDancer(registry, entity, dancer, rules::k_Men));
	EXPECT_EQ(dances::DanceOf(registry, dancer), entity);
	std::vector<entt::entity> finished;
	dances::TurnContext context {.turn = 1,
	                             .available = [](entt::entity) { return true; },
	                             .finished = [&finished](entt::entity e) { finished.push_back(e); }};
	dances::ProcessTurn(registry, entity, context);
	const auto& dance = registry.Get<components::Dance>(entity);
	EXPECT_TRUE(dance.dancing);
	EXPECT_FLOAT_EQ(dance.beat, 1.0f);
	// 31 turns on it stops, and starts again the turn after
	context.turn = 32;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_FALSE(registry.Get<components::Dance>(entity).dancing);
	context.turn = 33;
	dances::ProcessTurn(registry, entity, context);
	EXPECT_TRUE(registry.Get<components::Dance>(entity).dancing);
	// Its dancers are told when it goes
	dances::Destroy(registry, entity, context.finished);
	EXPECT_EQ(finished, std::vector<entt::entity> {dancer});
	EXPECT_FALSE(registry.AllOf<components::Dancer>(dancer));
}
