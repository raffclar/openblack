/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "LivingTurnOrder.h"

using namespace openblack;
using living_turn::Kind;

namespace
{

using Living = living_turn::Living<uint32_t>;

/// What the walk did, in its order: "c" and the id for a creature's turn, else the id of the living thing
std::vector<std::string> WalkOf(const std::vector<Living>& list)
{
	std::vector<std::string> done;
	living_turn::Walk(std::span<const Living>(list), [&done](const Living& living) {
		done.push_back((living.kind == Kind::Creature ? "c" : "") + std::to_string(living.id));
	});
	return done;
}

} // namespace

TEST(LivingTurnOrder, TheLastMadeTakesItsTurnFirst)
{
	std::vector<Living> list {
	    {.id = 10, .made = 0, .kind = Kind::Villager},
	    {.id = 11, .made = 2, .kind = Kind::Animal},
	    {.id = 12, .made = 1, .kind = Kind::Villager},
	};
	living_turn::NewestFirst(list);
	EXPECT_EQ(list.at(0).id, 11u);
	EXPECT_EQ(list.at(1).id, 12u);
	EXPECT_EQ(list.at(2).id, 10u);
}

TEST(LivingTurnOrder, ThingsWithTheSamePlaceGoByTheirIdsTheLargerFirst)
{
	std::vector<Living> list {
	    {.id = 3, .made = 0, .kind = Kind::Villager},
	    {.id = 7, .made = 0, .kind = Kind::Animal},
	    {.id = 5, .made = 4, .kind = Kind::Animal},
	};
	living_turn::NewestFirst(list);
	EXPECT_EQ(list.at(0).id, 5u);
	EXPECT_EQ(list.at(1).id, 7u);
	EXPECT_EQ(list.at(2).id, 3u);
}

TEST(LivingTurnOrder, VillagersAndAnimalsTakeTheirTurnsInTheOrderOfTheList)
{
	std::vector<Living> list {
	    {.id = 1, .made = 0, .kind = Kind::Animal},
	    {.id = 2, .made = 1, .kind = Kind::Villager},
	    {.id = 3, .made = 2, .kind = Kind::Animal},
	    {.id = 4, .made = 3, .kind = Kind::Villager},
	};
	living_turn::NewestFirst(list);
	EXPECT_EQ(WalkOf(list), (std::vector<std::string> {"4", "3", "2", "1"}));
}

TEST(LivingTurnOrder, EachCreatureTakesItsTurnAtItsOwnPlace)
{
	std::vector<Living> list {
	    {.id = 1, .made = 0, .kind = Kind::Villager}, {.id = 2, .made = 1, .kind = Kind::Creature},
	    {.id = 3, .made = 2, .kind = Kind::Animal},   {.id = 4, .made = 3, .kind = Kind::Creature},
	    {.id = 5, .made = 4, .kind = Kind::Villager},
	};
	living_turn::NewestFirst(list);
	EXPECT_EQ(WalkOf(list), (std::vector<std::string> {"5", "c4", "3", "c2", "1"}));
}

TEST(LivingTurnOrder, AnEmptyListHasNoTurns)
{
	EXPECT_TRUE(WalkOf({}).empty());
}

TEST(LivingTurnOrder, EachLivingThingDrawsItsNumbersInTheOrderOfTheList)
{
	// Two villagers and an animal each draw two numbers in their own turn. Made in the order 1, 2, 3, the third made
	// draws first, and its two draws come before the next one's.
	std::vector<Living> list {
	    {.id = 1, .made = 0, .kind = Kind::Villager},
	    {.id = 2, .made = 1, .kind = Kind::Animal},
	    {.id = 3, .made = 2, .kind = Kind::Villager},
	};
	living_turn::NewestFirst(list);
	uint32_t seed = game_random::k_InitialSeed;
	std::vector<std::pair<uint32_t, uint32_t>> draws;
	living_turn::Walk(std::span<const Living>(list), [&seed, &draws](const Living& living) {
		draws.emplace_back(living.id, game_random::LHRand(100, seed));
		draws.emplace_back(living.id, game_random::LHRand(100, seed));
	});

	uint32_t expected = game_random::k_InitialSeed;
	const std::vector<uint32_t> owners {3, 3, 2, 2, 1, 1};
	ASSERT_EQ(draws.size(), owners.size());
	for (size_t i = 0; i < owners.size(); ++i)
	{
		EXPECT_EQ(draws.at(i).first, owners.at(i));
		EXPECT_EQ(draws.at(i).second, game_random::LHRand(100, expected));
	}
}

TEST(LivingTurnOrder, TwoCreaturesDrawTheirNumbersEachAtItsOwnPlace)
{
	// Two creatures with a villager between them. Each creature draws in two steps of its own turn. All the draws of
	// one creature come before those of the villager, and those of the other creature after them.
	std::vector<Living> list {
	    {.id = 1, .made = 0, .kind = Kind::Creature},
	    {.id = 2, .made = 1, .kind = Kind::Villager},
	    {.id = 3, .made = 2, .kind = Kind::Creature},
	};
	living_turn::NewestFirst(list);
	uint32_t seed = game_random::k_InitialSeed;
	std::vector<std::pair<uint32_t, uint32_t>> draws;
	living_turn::Walk(std::span<const Living>(list), [&seed, &draws](const Living& living) {
		draws.emplace_back(living.id, game_random::LHRand(100, seed));
		// The second step of a creature's turn
		if (living.kind == Kind::Creature)
		{
			draws.emplace_back(living.id, game_random::LHRand(100, seed));
		}
	});

	uint32_t expected = game_random::k_InitialSeed;
	const std::vector<uint32_t> owners {3, 3, 2, 1, 1};
	ASSERT_EQ(draws.size(), owners.size());
	for (size_t i = 0; i < owners.size(); ++i)
	{
		EXPECT_EQ(draws.at(i).first, owners.at(i));
		EXPECT_EQ(draws.at(i).second, game_random::LHRand(100, expected));
	}
}
