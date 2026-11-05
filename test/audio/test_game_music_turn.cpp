/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <Sound/GameMusic.h>
#include <Sound/GameMusicTurn.h>
#include <gtest/gtest.h>

using namespace openblack::audio;
using namespace openblack::audio::game_music_turn;

namespace
{

Place PlaceAt(glm::vec3 camera, bool inCitadel)
{
	return {
	    .turn = 100,
	    .camera = camera,
	    .groundHeight = 0.0f,
	    .inCitadel = inCitadel,
	    .alignment = 0.9f,
	    .playerAlignment = -0.9f,
	    .cinema = false,
	};
}

GameMusic::Town GreekTownAt(glm::vec3 position)
{
	return {.position = position, .tribe = 6, .id = 1};
}

} // namespace

TEST(GameMusicTurn, TheCinemaBarsHoldTheLandsMusicOnlyForAScriptOrASlide)
{
	EXPECT_FALSE(CinemaHoldsLandMusic({.on = false, .owner = 0, .transitionFinished = true}));
	// Bars that show with no script that holds them do not hold the music
	EXPECT_FALSE(CinemaHoldsLandMusic({.on = true, .owner = 0, .transitionFinished = true}));
	EXPECT_TRUE(CinemaHoldsLandMusic({.on = true, .owner = 7, .transitionFinished = true}));
	// A slide of the bars holds the music, with or without a script
	EXPECT_TRUE(CinemaHoldsLandMusic({.on = false, .owner = 0, .transitionFinished = false}));
	EXPECT_TRUE(CinemaHoldsLandMusic({.on = true, .owner = 7, .transitionFinished = false}));
}

TEST(GameMusicTurn, TheInputsKeepThePlaceAndStartWithNothingElse)
{
	auto place = PlaceAt({10.0f, 20.0f, 30.0f}, true);
	place.groundHeight = 5.0f;
	place.cinema = true;
	const auto inputs = InputsAt(place);
	EXPECT_EQ(inputs.turn, 100u);
	EXPECT_EQ(inputs.camera, glm::vec3(10.0f, 20.0f, 30.0f));
	EXPECT_FLOAT_EQ(inputs.groundHeight, 5.0f);
	EXPECT_TRUE(inputs.inCitadel);
	EXPECT_FLOAT_EQ(inputs.alignment, 0.9f);
	EXPECT_FLOAT_EQ(inputs.playerAlignment, -0.9f);
	EXPECT_TRUE(inputs.cinema);
	EXPECT_TRUE(inputs.towns.empty());
	EXPECT_FALSE(inputs.thingPosition);
	EXPECT_FALSE(inputs.creatureDance.has_value());
	EXPECT_FALSE(inputs.worshipChant.has_value());
	EXPECT_FALSE(inputs.creatureFight.has_value());
	EXPECT_FALSE(inputs.film);
	EXPECT_EQ(inputs.script.type, MusicType::None);
	// The town distances keep the values of the shipped info table until the caller reads the table
	EXPECT_FLOAT_EQ(inputs.townTriggerDistance, 300.0f);
	EXPECT_FLOAT_EQ(inputs.townTriggerOffDistance, 400.0f);
}

TEST(GameMusicTurn, TheCitadelFlagPicksTheCitadelsMusicOfThePlayersAlignment)
{
	// Inside the citadel the player's own alignment counts, not the place of the camera
	EXPECT_EQ(GameMusic::SelectCitadelType(InputsAt(PlaceAt({1000.0f, 100.0f, 1000.0f}, true))), MusicType::CitadelEvil);
	auto place = PlaceAt({0.0f, 0.0f, 0.0f}, true);
	place.playerAlignment = 0.9f;
	EXPECT_EQ(GameMusic::SelectCitadelType(InputsAt(place)), MusicType::CitadelGood);
}

TEST(GameMusicTurn, TheCameraPlacePicksTheTownOrTheLandsMusic)
{
	GameMusic music;
	auto nearTown = InputsAt(PlaceAt({1000.0f, 100.0f, 1000.0f}, false));
	nearTown.towns.push_back(GreekTownAt({1100.0f, 0.0f, 1000.0f}));
	// Outside the citadel the alignment of the place counts
	EXPECT_EQ(music.SelectLandType(nearTown), MusicType::GreekTownGood);

	auto farFromTown = InputsAt(PlaceAt({3000.0f, 100.0f, 1000.0f}, false));
	farFromTown.towns.push_back(GreekTownAt({1100.0f, 0.0f, 1000.0f}));
	EXPECT_EQ(music.SelectLandType(farFromTown), MusicType::GenericGood);

	// Too high above the land, the camera hears no town
	auto highAbove = InputsAt(PlaceAt({1000.0f, 500.0f, 1000.0f}, false));
	highAbove.towns.push_back(GreekTownAt({1100.0f, 0.0f, 1000.0f}));
	EXPECT_EQ(music.SelectLandType(highAbove), MusicType::GenericGood);
}
