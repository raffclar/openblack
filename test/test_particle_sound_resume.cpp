/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Particles/ParticleSoundResume.h"

using namespace openblack::particles;

namespace
{
/// The vortex hum: maximum distance 400 m
constexpr float k_HumReach = 400.0f;
} // namespace

TEST(ParticleSoundResume, AHumThatStoppedFarAwayPlaysAgainWhenTheCameraComesNear)
{
	// The camera goes to 450 m: the audio stops the hum, and the sound notes that the listener is far
	auto step = sound_resume::Next(false, true, false, 450.0f, k_HumReach);
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
	// The camera stays far: nothing plays
	step = sound_resume::Next(step.far, true, false, 900.0f, k_HumReach);
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
	// The camera comes back to 150 m: the hum plays again, and the sound is near
	step = sound_resume::Next(step.far, true, false, 150.0f, k_HumReach);
	EXPECT_FALSE(step.far);
	EXPECT_TRUE(step.replay);
}

TEST(ParticleSoundResume, AtTheMaximumDistanceTheListenerIsFar)
{
	const auto step = sound_resume::Next(true, true, false, k_HumReach, k_HumReach);
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, AHumThatPlaysDoesNotStartAgain)
{
	const auto step = sound_resume::Next(true, true, true, 150.0f, k_HumReach);
	EXPECT_FALSE(step.far);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, ASoundThatStoppedNearDoesNotPlayAgain)
{
	// A sound with a higher priority took the voice while the listener was near
	const auto step = sound_resume::Next(false, true, false, 150.0f, k_HumReach);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, ASoundThatDoesNotLoopDoesNotPlayAgain)
{
	const auto step = sound_resume::Next(true, false, false, 150.0f, k_HumReach);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, WithoutAListenerOrAMaximumDistanceNothingChanges)
{
	EXPECT_EQ(sound_resume::Next(true, true, false, std::nullopt, k_HumReach).far, true);
	EXPECT_FALSE(sound_resume::Next(true, true, false, std::nullopt, k_HumReach).replay);
	EXPECT_FALSE(sound_resume::Next(true, true, false, 150.0f, 0.0f).replay);
}
