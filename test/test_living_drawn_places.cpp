/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The frames draw the animals and the souls of the dead from the turns' state, and the turns never read what the frames
// draw: an animal's drawn place comes only from its last two turns, and a soul goes at a turn that only the turns count

#include <cstdint>

#include <array>
#include <numbers>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Animals/AnimalRules.h"
#include "ECS/VillagerDeathRules.h"

using namespace openblack;

namespace
{

constexpr uint32_t k_TurnMs = 100;

/// One soul drawn with frames of one length: the frame time at each turn, as the game plays the turns of a frame
/// before its effects. Returns whether the frames drew the clip to its end before the turn that removes the soul.
bool FramesShowTheSoulOut(uint32_t frameMs, uint32_t clipMs, uint32_t riseOffsetMs)
{
	// The game's clock, the next turn's time and the turns that the soul has lived
	uint32_t clock = riseOffsetMs;
	uint32_t nextTurn = k_TurnMs;
	uint32_t turns = 0;
	uint32_t soulMs = 0;
	bool shownOut = false;
	bool risen = false;
	for (int frame = 0; frame < 100000; ++frame)
	{
		clock += frameMs;
		while (clock >= nextTurn)
		{
			nextTurn += k_TurnMs;
			if (!risen)
			{
				risen = true;
				continue;
			}
			++turns;
			if (villager_death::SoulRemoved(turns, k_TurnMs, clipMs))
			{
				return shownOut;
			}
		}
		if (risen && !shownOut)
		{
			soulMs += frameMs;
			shownOut = villager_death::SoulGone(soulMs, clipMs);
		}
	}
	return false;
}

} // namespace

TEST(LivingDrawnPlaces, AnAnimalIsDrawnBetweenItsLastTwoTurns)
{
	const glm::vec3 from {10.0f, 2.0f, 30.0f};
	const glm::vec3 to {14.0f, 4.0f, 22.0f};
	const auto start = animals::DrawnAt(from, to, 0.5f, 1.5f, 0.0f);
	EXPECT_EQ(start.position, from);
	EXPECT_FLOAT_EQ(start.heading, 0.5f);
	const auto end = animals::DrawnAt(from, to, 0.5f, 1.5f, 1.0f);
	EXPECT_EQ(end.position, to);
	EXPECT_FLOAT_EQ(end.heading, 1.5f);
	const auto half = animals::DrawnAt(from, to, 0.5f, 1.5f, 0.5f);
	EXPECT_EQ(half.position, glm::vec3(12.0f, 3.0f, 26.0f));
	EXPECT_FLOAT_EQ(half.heading, 1.0f);
}

TEST(LivingDrawnPlaces, TheDrawnPartOfATurnStaysBetweenItsEnds)
{
	const glm::vec3 from {0.0f};
	const glm::vec3 to {8.0f, 0.0f, 0.0f};
	EXPECT_EQ(animals::DrawnAt(from, to, 0.0f, 0.0f, -0.5f).position, from);
	EXPECT_EQ(animals::DrawnAt(from, to, 0.0f, 0.0f, 1.7f).position, to);
}

TEST(LivingDrawnPlaces, AnAnimalTurnsTheShortWayRound)
{
	constexpr float k_Pi = std::numbers::pi_v<float>;
	// From just under a half turn one way to just under a half turn the other way it turns through the half turn
	const auto half = animals::DrawnAt(glm::vec3(0.0f), glm::vec3(0.0f), k_Pi - 0.1f, -(k_Pi - 0.1f), 0.5f);
	EXPECT_NEAR(half.heading, k_Pi, 1e-5f);
}

TEST(LivingDrawnPlaces, TheTurnsRemoveASoulTwoTurnsAfterItsClipByTheirClock)
{
	constexpr uint32_t k_Clip = 3000;
	// The frames stop at 2891 ms into the clip. The turns' clock passes that at 29 turns, and they remove it 2 turns
	// later.
	EXPECT_FALSE(villager_death::SoulRemoved(0, k_TurnMs, k_Clip));
	EXPECT_FALSE(villager_death::SoulRemoved(30, k_TurnMs, k_Clip));
	EXPECT_TRUE(villager_death::SoulRemoved(31, k_TurnMs, k_Clip));
	// A clip shorter than the soul's early end goes at the second turn
	EXPECT_FALSE(villager_death::SoulRemoved(1, k_TurnMs, 50));
	EXPECT_TRUE(villager_death::SoulRemoved(2, k_TurnMs, 50));
}

TEST(LivingDrawnPlaces, TheFramesDrawASoulToItsEndAtAnyFrameTimeUpToATurn)
{
	constexpr auto k_FrameTimes = std::to_array<uint32_t>({1, 7, 16, 17, 33, 50, 99, 100});
	constexpr auto k_Clips = std::to_array<uint32_t>({600, 2891, 3000, 3133});
	for (const auto frameMs : k_FrameTimes)
	{
		for (const auto clipMs : k_Clips)
		{
			for (uint32_t offset = 0; offset < k_TurnMs; offset += 13)
			{
				EXPECT_TRUE(FramesShowTheSoulOut(frameMs, clipMs, offset))
				    << "frame " << frameMs << " ms, clip " << clipMs << " ms, offset " << offset;
			}
		}
	}
}
