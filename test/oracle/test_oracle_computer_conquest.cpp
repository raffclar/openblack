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
#include <bit>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMindConquestRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

namespace
{

constexpr size_t k_MostTowns = 3;
constexpr size_t k_MostIcons = 3;
constexpr size_t k_MostPlayers = 3;
constexpr uint32_t k_NoTown = 0xFFFFFFFF;
constexpr uint32_t k_NoPlayer = 0xFF;

/// The column of the name, which the test needs
size_t Need(const oracle::Table& table, const std::string& name)
{
	const auto column = table.Column(name);
	EXPECT_TRUE(column.has_value()) << name;
	return column.value_or(0);
}

map_coords::MapCoords Coords(uint32_t x, uint32_t z)
{
	return {.x = static_cast<int32_t>(x), .z = static_cast<int32_t>(z), .altitude = 0.0f};
}

} // namespace

// The town of another player that the god wants most, and how much, with the game's belief, hold, distance and sigmoid
TEST(OracleComputerConquest, TownWorthAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_conquest_town_worth.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto& t = *table;
	size_t different = 0;
	for (size_t row = 0; row < t.Rows(); ++row)
	{
		const bool neutral = t.Bits(row, Need(t, "neutral")) != 0;
		const float ownerPower = t.Float(row, Need(t, "owner_power"));
		const float godPower = t.Float(row, Need(t, "god_power"));
		const float godPeople = PeopleCount(t.Bits(row, Need(t, "god_people")));
		const auto god = static_cast<size_t>(t.Bits(row, Need(t, "god")));
		const auto owner = static_cast<size_t>(t.Bits(row, Need(t, "owner")));
		const auto hand = Coords(t.Bits(row, Need(t, "hand_x")), t.Bits(row, Need(t, "hand_z")));
		const auto towns = t.Bits(row, Need(t, "towns"));
		std::vector<float> worths;
		for (size_t town = 0; town < towns && town < k_MostTowns; ++town)
		{
			const auto field = [&](const std::string& name) { return Need(t, "t" + std::to_string(town) + "_" + name); };
			const auto icons = t.Bits(row, field("icons"));
			const auto enabled = t.Bits(row, field("enabled"));
			const auto requested = t.Bits(row, field("requested"));
			std::vector<TownMiracleFacts> facts;
			for (uint32_t icon = 0; icon < icons && icon < k_MostIcons; ++icon)
			{
				facts.push_back({.enabled = ((enabled >> icon) & 1u) != 0, .requested = ((requested >> icon) & 1u) != 0});
			}
			std::array<float, k_BeliefPlayers> belief {};
			for (size_t player = 0; player < k_BeliefPlayers; ++player)
			{
				belief.at(player) = t.Float(row, field("b" + std::to_string(player)));
			}
			const bool godOwns = t.Bits(row, field("godowns")) != 0;
			const auto townOwner = godOwns ? god : owner;
			const float wish = TownMiracleWish(facts);
			const float share = TownPeopleShare(t.Bits(row, field("adults")) + t.Bits(row, field("children")), godPeople, wish);
			const float cut = RivalPowerCut(neutral, ownerPower, godPower);
			const auto place = Coords(t.Bits(row, field("x")), t.Bits(row, field("z")));
			const float near = TownNearness(gutils::GetDistanceInMetres(hand, place));
			const float hold = TownHold(RivalBelief(belief, god, townOwner), townOwner == god);
			worths.push_back(TownWorth(share, wish, hold, cut, near));
		}
		const auto best = MostWantedTown(worths);
		const float ours = best.has_value() ? CappedWorth(worths[*best]) : 0.0f;
		const uint32_t index = best.has_value() ? static_cast<uint32_t>(*best) : k_NoTown;
		const auto theirs = t.Bits(row, Need(t, "worth"));
		const auto theirIndex = t.Bits(row, Need(t, "index"));
		if (std::bit_cast<uint32_t>(ours) != theirs || index != theirIndex)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(t.Float(row, Need(t, "worth"))) << " town "
				              << theirIndex << ", ours gives " << oracle::Hex(ours) << " town " << index;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// How much the god wants to meet the needs of a town, from the strongest belief of a rival that is not an ally
TEST(OracleComputerConquest, FeedFactorAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_conquest_feed_factor.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto& t = *table;
	size_t different = 0;
	for (size_t row = 0; row < t.Rows(); ++row)
	{
		const bool own = t.Bits(row, Need(t, "own")) != 0;
		const auto god = t.Bits(row, Need(t, "god"));
		std::array<float, k_BeliefPlayers> belief {};
		for (size_t player = 0; player < k_BeliefPlayers; ++player)
		{
			belief.at(player) = t.Float(row, Need(t, "b" + std::to_string(player)));
		}
		float strongest = 0.0f;
		for (size_t player = 0; player < k_MostPlayers; ++player)
		{
			const auto number = t.Bits(row, Need(t, "p" + std::to_string(player)));
			const bool ally = t.Bits(row, Need(t, "a" + std::to_string(player))) != 0;
			if (number == k_NoPlayer || number == god || ally)
			{
				continue;
			}
			if (belief.at(number) > strongest)
			{
				strongest = belief.at(number);
			}
		}
		const float ours = FeedFactor(own, own ? RivalBeliefRatio(strongest, belief.at(god)) : 0.0f);
		if (std::bit_cast<uint32_t>(ours) != t.Bits(row, Need(t, "factor")))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(t.Float(row, Need(t, "factor")))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// How much the god wants its creature to go to a thing, from the leash, the distance, the size and the rest
TEST(OracleComputerConquest, CreatureHelpAsInTheGame)
{
	const auto table = oracle::Table::Load("computer_conquest_creature_help.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto& t = *table;
	// The kinds of tie in the table: to the target, to a thing of the target's town
	constexpr uint32_t k_TiedToTarget = 1;
	constexpr uint32_t k_TiedInTown = 2;
	size_t different = 0;
	for (size_t row = 0; row < t.Rows(); ++row)
	{
		const bool creature = t.Bits(row, Need(t, "creature")) != 0;
		const auto tied = t.Bits(row, Need(t, "tied"));
		const bool sameTown = t.Bits(row, Need(t, "same_town")) != 0;
		float ours = 0.0f;
		if (creature && tied != k_TiedToTarget && !(sameTown && tied == k_TiedInTown))
		{
			const auto from = Coords(t.Bits(row, Need(t, "cx")), t.Bits(row, Need(t, "cz")));
			const auto to = Coords(t.Bits(row, Need(t, "tx")), t.Bits(row, Need(t, "tz")));
			ours = CreatureHelpWish(t.Float(row, Need(t, "freedom")), t.Float(row, Need(t, "exhaustion")),
			                        gutils::GetDistanceInMetres(from, to), t.Float(row, Need(t, "size")));
		}
		if (std::bit_cast<uint32_t>(ours) != t.Bits(row, Need(t, "wish")))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(t.Float(row, Need(t, "wish")))
				              << ", ours gives " << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
