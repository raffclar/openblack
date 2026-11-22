/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>

#include <gtest/gtest.h>

#include "Online/GodsRingMap.h"
#include "Simulation/TurnBesideDraw.h"

namespace turn_beside_draw = openblack::turn_beside_draw;
using turn_beside_draw::Mode;

TEST(TurnBesideDraw, TheSwitchReadsItsThreeValues)
{
	EXPECT_EQ(turn_beside_draw::ModeOf("auto"), Mode::ByLand);
	EXPECT_EQ(turn_beside_draw::ModeOf("on"), Mode::On);
	EXPECT_EQ(turn_beside_draw::ModeOf("off"), Mode::Off);
	EXPECT_FALSE(turn_beside_draw::ModeOf("").has_value());
	EXPECT_FALSE(turn_beside_draw::ModeOf("yes").has_value());
	EXPECT_FALSE(turn_beside_draw::ModeOf("ON").has_value());
}

TEST(TurnBesideDraw, OnlyTheMapFor32GodsPlaysBesideTheDraw)
{
	EXPECT_TRUE(turn_beside_draw::LandPlaysBeside(std::filesystem::path(openblack::gods_ring_map::k_File)));
	EXPECT_FALSE(turn_beside_draw::LandPlaysBeside(std::filesystem::path("Scripts/Land1.txt")));
	EXPECT_FALSE(turn_beside_draw::LandPlaysBeside(std::filesystem::path("Scripts/Playgrounds/TwoGods.txt")));
	EXPECT_FALSE(turn_beside_draw::LandPlaysBeside(std::filesystem::path()));
}

TEST(TurnBesideDraw, TheDeveloperSwitchWinsOverTheLand)
{
	EXPECT_TRUE(turn_beside_draw::PlaysBeside(Mode::ByLand, true));
	EXPECT_FALSE(turn_beside_draw::PlaysBeside(Mode::ByLand, false));
	EXPECT_TRUE(turn_beside_draw::PlaysBeside(Mode::On, false));
	EXPECT_TRUE(turn_beside_draw::PlaysBeside(Mode::On, true));
	EXPECT_FALSE(turn_beside_draw::PlaysBeside(Mode::Off, true));
	EXPECT_FALSE(turn_beside_draw::PlaysBeside(Mode::Off, false));
}

TEST(TurnBesideDraw, TheGameDefaultIsByLand)
{
	static_assert(turn_beside_draw::ModeOf("auto") == Mode::ByLand);
	static_assert(!turn_beside_draw::PlaysBeside(Mode::ByLand, false));
}
