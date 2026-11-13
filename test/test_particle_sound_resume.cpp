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

using namespace openblack::particles::sound_resume;

namespace
{
/// The vortex hum: maximum distance 400 m
constexpr float k_HumReach = 400.0f;

/// A turn of a looping sound with the maximum distance of the hum
Turn Hum(bool playing, float distance, bool listenerLeft = false)
{
	return {.looping = true, .playing = playing, .listenerLeft = listenerLeft, .distance = distance, .maxDistance = k_HumReach};
}
} // namespace

TEST(ParticleSoundResume, AHumThatStoppedFarAwayPlaysAgainWhenTheCameraComesNear)
{
	// The hum plays near the camera
	auto step = Next(false, Hum(true, 150.0f));
	EXPECT_FALSE(step.far);
	EXPECT_FALSE(step.replay);
	// The camera goes to 450 m, and the audio stops the hum for the distance
	step = Next(step.far, Hum(false, 450.0f, true));
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
	// The camera stays far: nothing plays
	step = Next(step.far, Hum(false, 900.0f));
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
	// The camera comes back to 150 m: the hum plays again
	step = Next(step.far, Hum(false, 150.0f));
	EXPECT_FALSE(step.far);
	EXPECT_TRUE(step.replay);
}

TEST(ParticleSoundResume, AfterTheTempleTheHumPlaysAgain)
{
	// The particle turns do not run in the temple. The audio stopped the hum there, because it hears from the camera of
	// the temple. The first turn after the temple sees the camera near the vortex again.
	const auto step = Next(false, Hum(false, 375.0f, true));
	EXPECT_FALSE(step.far);
	EXPECT_TRUE(step.replay);
}

TEST(ParticleSoundResume, AHumThatTheAudioDidNotStartFarAwayPlaysWhenTheCameraComesNear)
{
	// The audio did not start the hum far away, so the hum starts as far
	auto step = Next(true, Hum(false, 600.0f));
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
	step = Next(step.far, Hum(false, 150.0f));
	EXPECT_TRUE(step.replay);
}

TEST(ParticleSoundResume, AtTheMaximumDistanceTheListenerIsFar)
{
	const auto step = Next(true, Hum(false, k_HumReach));
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, AHumThatPlaysDoesNotStartAgain)
{
	const auto step = Next(true, Hum(true, 150.0f));
	EXPECT_FALSE(step.far);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, ASoundThatStoppedForADifferentReasonDoesNotPlayAgain)
{
	// A sound with a higher priority took the voice while the listener was near
	auto step = Next(false, Hum(false, 150.0f));
	EXPECT_FALSE(step.far);
	EXPECT_FALSE(step.replay);
	// The camera goes far and comes back: the distance did not stop this sound
	step = Next(step.far, Hum(false, 900.0f));
	EXPECT_FALSE(step.far);
	step = Next(step.far, Hum(false, 150.0f));
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, ASoundThatDoesNotLoopDoesNotPlayAgain)
{
	const auto step =
	    Next(true, {.looping = false, .playing = false, .listenerLeft = true, .distance = 150.0f, .maxDistance = k_HumReach});
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, WithoutAListenerTheSoundStaysFar)
{
	const auto step = Next(
	    false, {.looping = true, .playing = false, .listenerLeft = true, .distance = std::nullopt, .maxDistance = k_HumReach});
	EXPECT_TRUE(step.far);
	EXPECT_FALSE(step.replay);
}

TEST(ParticleSoundResume, WithoutAMaximumDistanceNothingPlays)
{
	const auto step =
	    Next(true, {.looping = true, .playing = false, .listenerLeft = false, .distance = 150.0f, .maxDistance = 0.0f});
	EXPECT_FALSE(step.replay);
}
