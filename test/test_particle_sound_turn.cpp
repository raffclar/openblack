/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Particles/ParticleSoundTurn.h"

using namespace openblack::particles::sound_turn;

namespace
{
/// A game turn in seconds
constexpr float k_Turn = 0.1f;
} // namespace

// A looping sound asks for its sample in each turn while its particle lives, also while it waits
TEST(ParticleSoundTurn, ALoopingSoundStartsInEachTurn)
{
	float wait = 0.0f;
	for (int turn = 0; turn < 5; ++turn)
	{
		EXPECT_EQ(Next(true, true, false, wait, k_Turn), Step::Start);
	}
	wait = 1.0f;
	EXPECT_EQ(Next(true, true, false, wait, k_Turn), Step::Start);
	EXPECT_EQ(wait, 1.0f);
}

// When the particle goes, the game lets go of a sound that does not travel at the speed of sound
TEST(ParticleSoundTurn, TheGameLetsGoOfTheSoundWhenItsParticleGoes)
{
	float wait = 0.5f;
	EXPECT_EQ(Next(false, false, false, wait, k_Turn), Step::LetGo);
	EXPECT_EQ(wait, 0.5f);
	EXPECT_EQ(Next(false, true, false, wait, k_Turn), Step::LetGo);
}

// Thunder that waits keeps its wait when its bolt goes, and starts when the wait ends. The game lets go of it in the
// next turn. In the game, this thunder was never heard.
TEST(ParticleSoundTurn, ThunderOutlivesItsBolt)
{
	float wait = 0.25f;
	EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Nothing);
	EXPECT_EQ(Next(false, false, true, wait, k_Turn), Step::Nothing);
	EXPECT_EQ(Next(false, false, true, wait, k_Turn), Step::Start);
	EXPECT_LT(wait, 0.0f);
	EXPECT_EQ(Next(false, false, true, wait, k_Turn), Step::LetGo);
}

// With the thunder of the game, thunder that waits goes with its bolt, so it is never heard
TEST(ParticleSoundTurn, TheThunderOfTheGameGoesWithItsBolt)
{
	float wait = 0.25f;
	const bool wholeLand = WholeLandHears(true, false);
	EXPECT_EQ(Next(true, false, wholeLand, wait, k_Turn), Step::Nothing);
	EXPECT_EQ(Next(false, false, wholeLand, wait, k_Turn), Step::LetGo);
}

// Only thunder is heard by the whole land, and only when the debug menu asks for far thunder
TEST(ParticleSoundTurn, TheWholeLandHearsOnlyFarThunder)
{
	EXPECT_TRUE(WholeLandHears(true, true));
	EXPECT_FALSE(WholeLandHears(true, false));
	EXPECT_FALSE(WholeLandHears(false, true));
	EXPECT_FALSE(WholeLandHears(false, false));
}

// Thunder that played already, or that never waited, goes with its bolt
TEST(ParticleSoundTurn, ThunderThatDoesNotWaitGoesWithItsBolt)
{
	float played = -0.05f;
	EXPECT_EQ(Next(false, false, true, played, k_Turn), Step::LetGo);
	float none = 0.0f;
	EXPECT_EQ(Next(false, false, true, none, k_Turn), Step::LetGo);
}

// Thunder 80 m away waits 0.23 s: it starts in the third turn, when its wait goes below zero
TEST(ParticleSoundTurn, ThunderStartsWhenItsWaitGoesBelowZero)
{
	float wait = 80.0f / 347.0f;
	EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Nothing);
	EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Nothing);
	EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Start);
	// Then it does not start again
	EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Nothing);
	EXPECT_LT(wait, 0.0f);
}

// A wait that comes to zero exactly does not go below zero, so the sound never plays. A sound that did not wait (a sound
// that does not travel at the speed of sound, or thunder at the camera) does not start either.
TEST(ParticleSoundTurn, AWaitOfZeroNeverEnds)
{
	float wait = k_Turn;
	EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Nothing);
	EXPECT_EQ(wait, 0.0f);
	for (int turn = 0; turn < 5; ++turn)
	{
		EXPECT_EQ(Next(true, false, true, wait, k_Turn), Step::Nothing);
	}
	EXPECT_EQ(wait, 0.0f);
}

// The camera must be nearer than 1200 m. The distance comes as its square.
TEST(ParticleSoundTurn, TheSoundStartsOnlyNearerThan1200Metres)
{
	EXPECT_TRUE(WithinReplayReach(0.0f));
	EXPECT_TRUE(WithinReplayReach(1199.0f * 1199.0f));
	EXPECT_FALSE(WithinReplayReach(1200.0f * 1200.0f));
	EXPECT_FALSE(WithinReplayReach(1300.0f * 1300.0f));
}
