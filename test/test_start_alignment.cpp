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
#include <limits>
#include <map>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::profile_alignment;

namespace
{

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

entt::entity MakePlayer(ecs::Registry& registry, PlayerNames name, float alignment, float pending = 0.0f)
{
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Player>(entity, name);
	registry.Assign<ecs::components::Alignment>(entity, alignment, pending);
	return entity;
}

} // namespace

// The start of a network game sends the alignment of the profile as the nearest float, no further than good or evil
TEST(StartAlignment, TheProfileSendsItsAlignmentAsTheNearestFloat)
{
	EXPECT_EQ(Bits(Sent(0.0)), Bits(0.0f));
	EXPECT_EQ(Bits(Sent(-0.5)), Bits(-0.5f));
	EXPECT_EQ(Bits(Sent(0.7)), 0x3F333333u);
	EXPECT_EQ(Bits(Sent(0.002555664163082838)), Bits(static_cast<float>(0.002555664163082838)));
	EXPECT_EQ(Bits(Sent(1.0)), Bits(1.0f));
	EXPECT_EQ(Bits(Sent(-1.0)), Bits(-1.0f));
	EXPECT_EQ(Bits(Sent(1.0000001)), Bits(1.0f));
	EXPECT_EQ(Bits(Sent(-7.0)), Bits(-1.0f));
	// The limit comes before the change to a float: a value just under 1 goes to the float 1
	EXPECT_EQ(Bits(Sent(0.99999999999)), Bits(1.0f));
	EXPECT_EQ(Bits(Sent(std::numeric_limits<double>::infinity())), Bits(1.0f));
	EXPECT_EQ(Bits(Sent(-std::numeric_limits<double>::infinity())), Bits(-1.0f));
	EXPECT_EQ(Bits(Sent(std::numeric_limits<double>::quiet_NaN())), Bits(-1.0f));
}

TEST(StartAlignment, AGodWithNoSentAlignmentStartsAtNeitherGoodNorEvil)
{
	EXPECT_EQ(Bits(StartsAt(std::nullopt)), Bits(0.0f));
	EXPECT_EQ(Bits(StartsAt(0.7f)), Bits(0.7f));
	EXPECT_EQ(Bits(StartsAt(-0.5f)), Bits(-0.5f));
	EXPECT_EQ(Bits(StartsAt(2.0f)), Bits(1.0f));
	EXPECT_EQ(Bits(StartsAt(-2.0f)), Bits(-1.0f));
	EXPECT_EQ(Bits(StartsAt(std::numeric_limits<float>::quiet_NaN())), Bits(-1.0f));
}

// Each god starts at what its player sent, a slot that the computer filled at 0, and the neutral player stays
TEST(StartAlignment, EachGodStartsAtTheAlignmentThatItsPlayerSent)
{
	ecs::Registry registry;
	const auto first = MakePlayer(registry, PlayerNames::PLAYER_ONE, 0.25f, 0.125f);
	const auto second = MakePlayer(registry, PlayerNames::PLAYER_TWO, 0.0f);
	const auto filled = MakePlayer(registry, PlayerNames::PLAYER_THREE, 0.5f);
	const auto neutral = MakePlayer(registry, PlayerNames::NEUTRAL, 0.75f);
	const std::map<PlayerNames, float> sent {{PlayerNames::PLAYER_ONE, -0.5f}, {PlayerNames::PLAYER_TWO, 0.7f}};
	StartGods(registry, [&sent](PlayerNames god) -> std::optional<float> {
		const auto found = sent.find(god);
		return found != sent.end() ? std::optional<float>(found->second) : std::nullopt;
	});
	using ecs::components::Alignment;
	EXPECT_EQ(registry.Get<Alignment>(first).value, -0.5f);
	// A change still to come stays
	EXPECT_EQ(registry.Get<Alignment>(first).pending, 0.125f);
	EXPECT_EQ(registry.Get<Alignment>(second).value, 0.7f);
	EXPECT_EQ(registry.Get<Alignment>(filled).value, 0.0f);
	EXPECT_EQ(registry.Get<Alignment>(neutral).value, 0.75f);
}
