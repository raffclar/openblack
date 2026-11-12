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
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/PlayerColour.h"
#include "Particles/PlayerSparkles.h"

using namespace openblack;

namespace
{
/// The game's tint of a sparkle as packed integer sums on the half colour, kept as the game computes it, to check the
/// channel rules against
uint32_t PackedTint(uint32_t colour, uint32_t tint)
{
	uint32_t half = (colour & 0xfefefeu) >> 1u;
	switch (tint)
	{
	case 0:
		half = ((((half & 0xff00u) - 0x9000u & 0x1ffffffu) >> 1u) - 0x7000u & 0xff00u) |
		       ((((half & 0xff0000u) + 0x1010000u & 0x1ffffffu) >> 1u) - 0x10000u & 0xff0000u) |
		       ((((half & 0xffu) - 0x90u & 0x1ffffffu) >> 1u) - 0x70u & 0xffu);
		break;
	case 1:
		half = ((((half & 0xff00u) - 0xff00u & 0x1ffffffu) >> 1u) - 0x100u & 0xff00u) |
		       ((((half & 0xff0000u) + 0x1010000u & 0x1ffffffu) >> 1u) - 0x10000u & 0xff0000u) |
		       ((((half & 0xffu) - 0x90u & 0x1ffffffu) >> 1u) - 0x70u & 0xffu);
		break;
	case 2:
		half = ((((half & 0xff00u) - 0xff00u & 0x1ffffffu) >> 1u) - 0x100u & 0xff00u) |
		       ((((half & 0xff0000u) + 0x1700000u & 0x1ffffffu) >> 1u) - 0x700000u & 0xff0000u) |
		       ((((half & 0xffu) - 0x90u & 0x1ffffffu) >> 1u) - 0x70u & 0xffu);
		break;
	case 3:
		half = ((half & 0xff0000u) * 3u & 0x3fc0003u | (half & 0xff00u) * 3u & 0x3fc00u | (half & 0xffu) * 3u & 0x3fcu) >> 2u;
		break;
	default:
		break;
	}
	return half + 0x80000000u;
}
} // namespace

TEST(PlayerSparkles, EachTintGivesTheGamesColourForEveryChannelValue)
{
	for (uint32_t tint = 0; tint < static_cast<uint32_t>(player_sparkles::k_Tints); ++tint)
	{
		for (uint32_t value = 0; value < 0x100u; ++value)
		{
			for (const uint32_t colour : {value << 16u | 0x00347Fu, 0xC800FFu | value << 8u, 0x01FE00u | value,
			                              value * 0x010101u, 0xFF000000u | value * 0x010101u})
			{
				EXPECT_EQ(player_sparkles::Colour(colour, tint), PackedTint(colour, tint))
				    << "tint " << tint << " colour " << std::hex << colour;
			}
		}
	}
}

TEST(PlayerSparkles, TheFirstPlayersRedSparklesAtHalfAlpha)
{
	const auto red = player_colour::OnLand(PlayerNames::PLAYER_ONE, 0);
	EXPECT_EQ(red, 0xFFFF4646u);
	// Tints 4 to 6 keep the half colour
	EXPECT_EQ(player_sparkles::Colour(red, 5), 0x807F2323u);
	EXPECT_EQ(player_sparkles::Colour(red, 0), 0x80BF5959u);
	EXPECT_EQ(player_sparkles::Colour(red, 3), 0x805F1A1Au);
}

TEST(PlayerSparkles, TheKindIsAReversedMoteForARollOfNothing)
{
	EXPECT_EQ(player_sparkles::KindOf(0), liquid_particles::Kind::MoteReversed);
	EXPECT_EQ(player_sparkles::KindOf(1), liquid_particles::Kind::Mote);
}

TEST(PlayerColour, SomeLandsGiveTheSecondAndThirdPlayersOtherColours)
{
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_TWO, 1), 0xFFFFA247u);
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_TWO, 2), 0xFFFFFD47u);
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_THREE, 2), 0xFF4777FFu);
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_TWO, 3), 0xFF4777FFu);
	// A network land keeps each player's own colour
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_TWO, 0), 0xFF47FF54u);
	EXPECT_EQ(player_colour::OnLand(PlayerNames::PLAYER_THREE, 0), 0xFFE347FFu);
}

TEST(PlayerSparkles, AComputerGodShowsASparkleAtTheCameraOfItsMindOnlyWhileItsMindPlays)
{
	const glm::vec3 mind {10.0f, 20.0f, 30.0f};
	const glm::vec3 reported {1.0f, 2.0f, 3.0f};
	player_sparkles::Watched god {
	    .kind = player_kind::Kind::Computer, .mindOn = true, .mindCamera = mind, .reportedCamera = reported};
	EXPECT_EQ(player_sparkles::PlaceOf(god), mind);
	// A stopped mind shows no sparkle, also with hands and a reported camera
	god.mindOn = false;
	EXPECT_FALSE(player_sparkles::PlaceOf(god).has_value());
}

TEST(PlayerSparkles, AHumanShowsASparkleAtItsReportedCameraWhileInTheGame)
{
	const glm::vec3 reported {1.0f, 2.0f, 3.0f};
	player_sparkles::Watched person {.kind = player_kind::Kind::Human, .reportedCamera = reported};
	EXPECT_EQ(player_sparkles::PlaceOf(person), reported);
	// A mind that a script switched on for a human player does not move the sparkle
	person.mindOn = true;
	EXPECT_EQ(player_sparkles::PlaceOf(person), reported);
	person.left = true;
	EXPECT_FALSE(player_sparkles::PlaceOf(person).has_value());
	// No sparkle before the first report
	EXPECT_FALSE(player_sparkles::PlaceOf({.kind = player_kind::Kind::Human}).has_value());
}

TEST(PlayerSparkles, APlayerThatNobodyPlaysShowsNoSparkle)
{
	EXPECT_FALSE(
	    player_sparkles::PlaceOf(
	        {.kind = player_kind::Kind::None, .mindOn = true, .mindCamera = glm::vec3(1.0f), .reportedCamera = glm::vec3(2.0f)})
	        .has_value());
}

TEST(PlayerSparkles, TheSymbolShowsOverTheSparklesOfAPlayingMindOrAPersonInTheGame)
{
	EXPECT_TRUE(player_sparkles::ShowsSymbol({.kind = player_kind::Kind::Computer, .mindOn = true}));
	EXPECT_FALSE(player_sparkles::ShowsSymbol({.kind = player_kind::Kind::Computer, .mindOn = false}));
	EXPECT_TRUE(player_sparkles::ShowsSymbol({.kind = player_kind::Kind::Human}));
	EXPECT_FALSE(player_sparkles::ShowsSymbol({.kind = player_kind::Kind::Human, .left = true}));
	// The sparkles of a player that nobody plays still show the symbol until they go
	EXPECT_TRUE(player_sparkles::ShowsSymbol({.kind = player_kind::Kind::None}));
}

TEST(PlayerSparkles, TheSymbolTurnsByTheMillisecondsAndStaysBetweenMinusPiAndPi)
{
	// Twenty milliseconds turn it by a hundredth of a radian, in the game's floats
	EXPECT_EQ(player_sparkles::SymbolAngle(0.0f, 20), 20.0f * std::bit_cast<float>(0x3A03126Fu));
	const float pi = std::bit_cast<float>(0x40490FDBu);
	const float twoPi = std::bit_cast<float>(0x40C90FDBu);
	const float past = 10.0f * std::bit_cast<float>(0x3A03126Fu) + 3.14f;
	EXPECT_EQ(player_sparkles::SymbolAngle(3.14f, 10), past - twoPi);
	EXPECT_GT(player_sparkles::SymbolAngle(3.14f, 10), -pi);
	// A turn back stays in the same span
	EXPECT_EQ(player_sparkles::SymbolAngle(-3.14f, -10), -10.0f * std::bit_cast<float>(0x3A03126Fu) - 3.14f + twoPi);
	EXPECT_EQ(player_sparkles::SymbolAngle(1.0f, 0), 1.0f);
}

TEST(PlayerSparkles, TheSymbolsTurnInTheOrderTheyDrawTheFarthestFirst)
{
	const glm::vec3 camera {0.0f, 10.0f, 0.0f};
	const std::array points {glm::vec3(5.0f, 10.0f, 0.0f), glm::vec3(50.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 20.0f)};
	EXPECT_EQ(player_sparkles::SymbolDrawOrder(points, camera), (std::vector<size_t> {1, 2, 0}));
	// Of two pools at the same distance, the pool of the later interface draws first
	const std::array same {glm::vec3(5.0f, 10.0f, 0.0f), glm::vec3(-5.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 1.0f)};
	EXPECT_EQ(player_sparkles::SymbolDrawOrder(same, camera), (std::vector<size_t> {1, 0, 2}));
	EXPECT_TRUE(player_sparkles::SymbolDrawOrder({}, camera).empty());
}
