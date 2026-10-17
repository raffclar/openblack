/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>

#include <gtest/gtest.h>

#include "Audio/HearingRange.h"

using namespace openblack::audio;

namespace
{
constexpr float k_VoiceDefault = 9999.0f;
} // namespace

TEST(HearingRange, SoundEffectUsesTheBanksMaximumDistance)
{
	// A crash whose bank says 150 m starts at 150 m and not a step further, whatever the play asks
	EXPECT_FLOAT_EQ(SoundEffectStartRange(150.0f, k_VoiceDefault), 150.0f);
	EXPECT_FLOAT_EQ(SoundEffectStartRange(150.0f, 40.0f), 150.0f);
	EXPECT_TRUE(IsWithinStartRange(150.0f * 150.0f, SoundEffectStartRange(150.0f, k_VoiceDefault)));
	EXPECT_FALSE(IsWithinStartRange(150.5f * 150.5f, SoundEffectStartRange(150.0f, k_VoiceDefault)));
}

TEST(HearingRange, SoundEffectWithoutABankDistanceFallsBackOnThePlays)
{
	// Most samples keeping the default distances have none in their bank: they are heard from as far as the voice
	EXPECT_FLOAT_EQ(SoundEffectStartRange(0.0f, k_VoiceDefault), k_VoiceDefault);
	EXPECT_TRUE(IsWithinStartRange(400.0f * 400.0f, SoundEffectStartRange(0.0f, k_VoiceDefault)));
	// A play that gives its own maximum distance is held to it
	EXPECT_FLOAT_EQ(SoundEffectStartRange(0.0f, 60.0f), 60.0f);
	EXPECT_FALSE(IsWithinStartRange(61.0f * 61.0f, SoundEffectStartRange(0.0f, 60.0f)));
}

TEST(HearingRange, AnimEffectHasNoFallback)
{
	EXPECT_TRUE(IsWithinStartRange(149.0f * 149.0f, AnimEffectStartRange(150.0f)));
	EXPECT_FALSE(IsWithinStartRange(154.0f * 154.0f, AnimEffectStartRange(150.0f)));
	// No bank distance: only heard from right where it plays
	EXPECT_TRUE(IsWithinStartRange(0.0f, AnimEffectStartRange(0.0f)));
	EXPECT_FALSE(IsWithinStartRange(1.0f, AnimEffectStartRange(0.0f)));
}

TEST(HearingRange, TheStartTestUsesTheSquaresOfTheDistanceInTheGamesOrder)
{
	// The height first, then up, then across
	const glm::vec3 camera {0.1f, 0.2f, 0.3f};
	const glm::vec3 point {1000.7f, -3.9f, 77.1f};
	const glm::vec3 way = camera - point;
	EXPECT_EQ(DistanceSquared(camera, point), (way.z * way.z + way.y * way.y) + way.x * way.x);
	// Not a number never stops a start: only a square more than the range's does
	EXPECT_TRUE(IsWithinStartRange(std::numeric_limits<float>::quiet_NaN(), 10.0f));
	EXPECT_TRUE(IsWithinStartRange(100.0f, 10.0f));
	EXPECT_FALSE(IsWithinStartRange(100.0001f, 10.0f));
}

TEST(HearingRange, ADelayedSoundTravelsAtTheSpeedOfSoundAndStartsOnlyInsideItsRange)
{
	// Ten turns of 100 milliseconds: one second, 347 metres
	EXPECT_EQ(SoundTravelled(10, 100), 347.0f * ((10.0f * 100.0f) * 0.001f));
	EXPECT_FALSE(SoundArrived(SoundTravelled(1, 100), 35.0f * 35.0f));
	EXPECT_TRUE(SoundArrived(SoundTravelled(2, 100), 69.0f * 69.0f));
	// Arrived, it starts only nearer than its range
	EXPECT_TRUE(ArrivedSoundStarts(99.0f * 99.0f, 100.0f));
	EXPECT_FALSE(ArrivedSoundStarts(100.0f * 100.0f, 100.0f));
	// A range of nothing never starts, not even on top of the camera
	EXPECT_FALSE(ArrivedSoundStarts(0.0f, 0.0f));
}

TEST(HearingRange, PlayingSoundStopsAtItsVoiceMaximumDistance)
{
	// The vortex's hum: a voice maximum of 400 m
	EXPECT_TRUE(IsWithinPlayRange(399.9f, 400.0f));
	EXPECT_FALSE(IsWithinPlayRange(400.0f, 400.0f));
	EXPECT_FALSE(IsWithinPlayRange(1200.0f, 400.0f));
	// A voice that keeps the default maximum plays on from far away
	EXPECT_TRUE(IsWithinPlayRange(1200.0f, k_VoiceDefault));
}

TEST(HearingRange, DistanceGainFollowsTheGamesMapping)
{
	// The vortex's hum: full volume to 200 m, roll-off 2, silent past 400 m
	const DistanceSettings hum {.minDistance = 200.0f, .maxDistance = 400.0f, .rolloff = 2.0f};
	EXPECT_FLOAT_EQ(DistanceGain(100.0f, hum), 1.0f);
	EXPECT_FLOAT_EQ(DistanceGain(150.0f, hum), 1.0f);
	EXPECT_FLOAT_EQ(DistanceGain(200.0f, hum), 1.0f);
	EXPECT_FLOAT_EQ(DistanceGain(250.0f, hum), 200.0f / 300.0f);
	EXPECT_FLOAT_EQ(DistanceGain(300.0f, hum), 0.5f);
	EXPECT_FLOAT_EQ(DistanceGain(400.0f, hum), 200.0f / 600.0f);
	EXPECT_FLOAT_EQ(DistanceGain(400.5f, hum), 0.0f);
	EXPECT_TRUE(IsBeyondMaxDistance(400.5f, hum.maxDistance));
	EXPECT_FALSE(IsBeyondMaxDistance(400.0f, hum.maxDistance));
}

TEST(HearingRange, DistanceGainWithoutRolloffOrWithRolloffOne)
{
	// No roll-off: full volume up to the maximum distance
	EXPECT_FLOAT_EQ(DistanceGain(300.0f, {.minDistance = 10.0f, .maxDistance = 400.0f, .rolloff = 0.0f}), 1.0f);
	// A roll-off of 1: the minimum over the distance
	EXPECT_FLOAT_EQ(DistanceGain(40.0f, {.minDistance = 10.0f, .maxDistance = 400.0f, .rolloff = 1.0f}), 0.25f);
	// The default voice: 1 m, roll-off 0.3, 9999 m
	EXPECT_FLOAT_EQ(DistanceGain(101.0f, {.minDistance = 1.0f, .maxDistance = 9999.0f, .rolloff = 0.3f}), 1.0f / 31.0f);
}
