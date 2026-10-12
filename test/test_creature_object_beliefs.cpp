/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <set>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureObjectBeliefs.h"

using namespace openblack::creature_object_beliefs;

namespace
{
constexpr uint32_t k_Forest = 1;
constexpr uint32_t k_Temple = 2;
constexpr uint32_t k_Flock = 22;

entt::entity Thing(uint32_t n)
{
	return static_cast<entt::entity>(n);
}
} // namespace

TEST(CreatureObjectBeliefs, ItKeepsOneBeliefAboutEachThingWhereItLastCameAcrossIt)
{
	Store store;
	const auto first = store.Add(Thing(1), k_Forest, {1.0f, 0.0f, 2.0f});
	ASSERT_NE(first.belief, nullptr);
	EXPECT_TRUE(first.isNew);
	const auto again = store.Add(Thing(1), k_Forest, {5.0f, 0.0f, 6.0f});
	EXPECT_FALSE(again.isNew);
	EXPECT_EQ(store.Size(), 1u);
	ASSERT_NE(store.Find(Thing(1)), nullptr);
	EXPECT_EQ(store.Find(Thing(1))->place, glm::vec3(5.0f, 0.0f, 6.0f));
	EXPECT_EQ(store.Find(Thing(2)), nullptr);
}

TEST(CreatureObjectBeliefs, ItCountsTheBeliefsOfEachKind)
{
	Store store;
	store.Add(Thing(1), k_Temple, {});
	store.Add(Thing(2), k_Flock, {});
	store.Add(Thing(3), k_Flock, {});
	store.Add(Thing(4), k_Forest, {});
	store.Add(Thing(5), k_Forest, {});
	store.Add(Thing(6), k_Forest, {});
	EXPECT_EQ(store.Count(k_Temple), 1u);
	EXPECT_EQ(store.Count(k_Flock), 2u);
	EXPECT_EQ(store.Count(k_Forest), 3u);
	EXPECT_EQ(store.Count(0), 0u);
	// The game has no kinds beyond its 24
	EXPECT_EQ(store.Add(Thing(7), k_KindCount, {}).belief, nullptr);
	EXPECT_EQ(store.Count(k_KindCount), 0u);
}

TEST(CreatureObjectBeliefs, ItKeepsTwentyTemplesFortyFlocksAndFiftyForests)
{
	EXPECT_EQ(k_Capacity[k_Temple], 20u);
	EXPECT_EQ(k_Capacity[k_Flock], 40u);
	EXPECT_EQ(k_Capacity[k_Forest], 50u);
	Store store;
	for (uint32_t i = 0; i < 60; ++i)
	{
		store.Add(Thing(i), k_Forest, {});
	}
	EXPECT_EQ(store.Count(k_Forest), 50u);
}

TEST(CreatureObjectBeliefs, OnceFullEachNewBeliefReplacesTheOnePutAtTheOldestPlace)
{
	Store store;
	for (uint32_t i = 0; i < 20; ++i)
	{
		store.Add(Thing(i), k_Temple, {});
	}
	// The first temple's place is the oldest kept: the next temple takes it, and the one after takes it again
	EXPECT_TRUE(store.Add(Thing(100), k_Temple, {}).isNew);
	EXPECT_EQ(store.Find(Thing(0)), nullptr);
	EXPECT_NE(store.Find(Thing(100)), nullptr);
	store.Add(Thing(101), k_Temple, {});
	EXPECT_EQ(store.Find(Thing(100)), nullptr);
	EXPECT_NE(store.Find(Thing(1)), nullptr);
	EXPECT_EQ(store.Count(k_Temple), 20u);
}

TEST(CreatureObjectBeliefs, ForgettingTheBeliefAtTheOldestPlaceMovesTheMarkerOn)
{
	Store store;
	for (uint32_t i = 0; i < 20; ++i)
	{
		store.Add(Thing(i), k_Temple, {});
	}
	EXPECT_TRUE(store.Forget(Thing(0)));
	EXPECT_FALSE(store.Forget(Thing(0)));
	// A free place is taken first, then the marker's, which has moved on to the second temple's
	store.Add(Thing(100), k_Temple, {});
	EXPECT_NE(store.Find(Thing(1)), nullptr);
	store.Add(Thing(101), k_Temple, {});
	EXPECT_EQ(store.Find(Thing(1)), nullptr);
	EXPECT_NE(store.Find(Thing(100)), nullptr);
	EXPECT_NE(store.Find(Thing(2)), nullptr);
}

TEST(CreatureObjectBeliefs, TheMarkerGoesRoundFromTheLastPlaceToTheFirst)
{
	Store store;
	// Things 0 to 19 in places 0 to 19; forgetting 0 to 18 one by one moves the marker to the last place
	for (uint32_t i = 0; i < 20; ++i)
	{
		store.Add(Thing(i), k_Temple, {});
	}
	for (uint32_t i = 0; i < 19; ++i)
	{
		store.Forget(Thing(i));
	}
	// Forgetting the last place's belief with others still held takes the marker round to the first place
	for (uint32_t i = 0; i < 19; ++i)
	{
		store.Add(Thing(100 + i), k_Temple, {});
	}
	store.Forget(Thing(19));
	store.Add(Thing(200), k_Temple, {});
	store.Add(Thing(201), k_Temple, {});
	EXPECT_EQ(store.Find(Thing(100)), nullptr);
	EXPECT_NE(store.Find(Thing(200)), nullptr);
	EXPECT_NE(store.Find(Thing(101)), nullptr);
}

TEST(CreatureObjectBeliefs, ItForgetsBeliefsAboutThingsThatAreGone)
{
	Store store;
	store.Add(Thing(1), k_Forest, {});
	store.Add(Thing(2), k_Flock, {});
	store.Add(Thing(3), k_Forest, {});
	store.ForgetIf([](entt::entity thing) { return thing != Thing(2); });
	EXPECT_EQ(store.Size(), 1u);
	EXPECT_EQ(store.Count(k_Forest), 0u);
	EXPECT_EQ(store.Count(k_Flock), 1u);
	// With nothing of a kind held, the first place taken next is the oldest
	store.Add(Thing(4), k_Forest, {});
	EXPECT_EQ(store.All().size(), 2u);
}

TEST(CreatureObjectBeliefs, ItsEyesSweepASquareThatGrowsWithItsSize)
{
	// The odd number below the root of sixty times its size and a hundred and sixty, squared
	EXPECT_EQ(SweepCells(1.0f), 15u * 15u);
	EXPECT_EQ(SweepCells(0.22f), 13u * 13u);
	EXPECT_EQ(SweepCells(4.0f), 21u * 21u);
	EXPECT_EQ(SweepCells(0.0f), 13u * 13u);
}

TEST(CreatureObjectBeliefs, EachRingGoesRoundFromTheCornerAtTheLeastXAndZ)
{
	const glm::ivec2 centre {10, 10};
	EXPECT_EQ(RingCell(centre, 1, 0), centre);
	const std::vector<glm::ivec2> ring2 {{9, 9}, {10, 9}, {11, 9}, {11, 10}, {11, 11}, {10, 11}, {9, 11}, {9, 10}};
	for (uint32_t step = 0; step < 8; ++step)
	{
		EXPECT_EQ(RingCell(centre, 2, step), ring2[step]) << step;
	}
	EXPECT_EQ(RingCell(centre, 3, 0), glm::ivec2(8, 8));
	EXPECT_EQ(RingCell(centre, 3, 4), glm::ivec2(12, 8));
	EXPECT_EQ(RingCell(centre, 3, 15), glm::ivec2(8, 9));
}

TEST(CreatureObjectBeliefs, ASweepLooksAtEveryCellOnceANinthAtATimeThenStartsAgain)
{
	Sweep sweep;
	const glm::ivec2 centre {100, 100};
	std::set<std::pair<int, int>> seen;
	size_t looked = 0;
	for (int turn = 0; turn < 25; ++turn)
	{
		const auto cells = NextCells(sweep, centre, 1.0f);
		ASSERT_EQ(cells.size(), 9u) << turn;
		if (turn == 0)
		{
			EXPECT_EQ(cells[0], centre);
			EXPECT_EQ(cells[1], glm::ivec2(99, 99));
		}
		for (const auto cell : cells)
		{
			EXPECT_LE(std::abs(cell.x - centre.x), 7);
			EXPECT_LE(std::abs(cell.y - centre.y), 7);
			seen.insert({cell.x, cell.y});
		}
		looked += cells.size();
	}
	EXPECT_EQ(looked, 225u);
	EXPECT_EQ(seen.size(), 225u);
	EXPECT_EQ(sweep.left, 0u);
	// Then it starts again from its own cell
	EXPECT_EQ(NextCells(sweep, centre, 1.0f).front(), centre);
}

TEST(CreatureObjectBeliefs, TheLastTurnOfASweepLooksAtWhatIsLeft)
{
	// 169 cells, nine a turn: eighteen turns of nine and one of seven
	Sweep sweep;
	for (int turn = 0; turn < 18; ++turn)
	{
		EXPECT_EQ(NextCells(sweep, {50, 50}, 0.0f).size(), 9u);
	}
	EXPECT_EQ(NextCells(sweep, {50, 50}, 0.0f).size(), 7u);
	EXPECT_EQ(NextCells(sweep, {50, 50}, 0.0f).size(), 9u);
}

TEST(CreatureObjectBeliefs, ItSeesAThirdOfACircleEitherSideOfTheWayItLooks)
{
	EXPECT_TRUE(CanSee(0, 0, false));
	EXPECT_TRUE(CanSee(0, 0x2AA, false));
	EXPECT_FALSE(CanSee(0, 0x2AB, false));
	EXPECT_TRUE(CanSee(0, 0x556, false));
	EXPECT_FALSE(CanSee(0, 0x555, false));
	EXPECT_TRUE(CanSee(0x700, 0x100, false));
	EXPECT_FALSE(CanSee(0x400, 0, false));
	// Whatever is in its own cell it always sees
	EXPECT_TRUE(CanSee(0x400, 0, true));
}
