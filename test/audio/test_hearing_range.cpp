/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
	EXPECT_TRUE(IsWithinStartRange(150.0f, SoundEffectStartRange(150.0f, k_VoiceDefault)));
	EXPECT_FALSE(IsWithinStartRange(150.5f, SoundEffectStartRange(150.0f, k_VoiceDefault)));
}

TEST(HearingRange, SoundEffectWithoutABankDistanceFallsBackOnThePlays)
{
	// Most samples keeping the default distances have none in their bank: they are heard from as far as the voice
	EXPECT_FLOAT_EQ(SoundEffectStartRange(0.0f, k_VoiceDefault), k_VoiceDefault);
	EXPECT_TRUE(IsWithinStartRange(400.0f, SoundEffectStartRange(0.0f, k_VoiceDefault)));
	// A play that gives its own maximum distance is held to it
	EXPECT_FLOAT_EQ(SoundEffectStartRange(0.0f, 60.0f), 60.0f);
	EXPECT_FALSE(IsWithinStartRange(61.0f, SoundEffectStartRange(0.0f, 60.0f)));
}

TEST(HearingRange, AnimEffectHasNoFallback)
{
	EXPECT_TRUE(IsWithinStartRange(149.0f, AnimEffectStartRange(150.0f)));
	EXPECT_FALSE(IsWithinStartRange(154.0f, AnimEffectStartRange(150.0f)));
	// No bank distance: only heard from right where it plays
	EXPECT_TRUE(IsWithinStartRange(0.0f, AnimEffectStartRange(0.0f)));
	EXPECT_FALSE(IsWithinStartRange(1.0f, AnimEffectStartRange(0.0f)));
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
