/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Audio/MixVolume.h"

using namespace openblack::audio::mix_volume;

// The slider value times 127, cut to a whole number: 12.7 gives 12 and 63.5 gives 63
TEST(MixVolume, TheSliderGivesWholeSteps)
{
	EXPECT_EQ(MasterVolumeOfSlider(0.0f), 0u);
	EXPECT_EQ(MasterVolumeOfSlider(0.1f), 12u);
	EXPECT_EQ(MasterVolumeOfSlider(0.5f), 63u);
	EXPECT_EQ(MasterVolumeOfSlider(0.99f), 125u);
	EXPECT_EQ(MasterVolumeOfSlider(1.0f), 127u);
	// A value just under a whole step stays in the step below it
	EXPECT_EQ(MasterVolumeOfSlider(2.0f / 127.0f), 2u);
	EXPECT_EQ(MasterVolumeOfSlider(1.9999f / 127.0f), 1u);
	// The slider gives only 0 to 1
	EXPECT_EQ(MasterVolumeOfSlider(-0.5f), 0u);
	EXPECT_EQ(MasterVolumeOfSlider(1.5f), 127u);
}

// The voice's volume and the master volume divide by 127 before the mixer's steps of 258
TEST(MixVolume, SampleVoicesMixInWholeSteps)
{
	EXPECT_EQ(SampleMixerVolume(127, 127), 32766u);
	EXPECT_EQ(SampleMixerVolume(127, 12), 12u * 258u);
	EXPECT_EQ(SampleMixerVolume(50, 63), 24u * 258u);
	EXPECT_EQ(SampleMixerVolume(1, 126), 0u);
	EXPECT_EQ(SampleMixerVolume(127, 0), 0u);
	// A voice louder than 127 plays at 127
	EXPECT_EQ(SampleMixerVolume(200, 127), 32766u);
}

// The music multiplies the channel and bank volumes in mixer steps, divides by 127, then mixes the master volume
TEST(MixVolume, MusicChannelsMixInWholeSteps)
{
	EXPECT_EQ(MusicMixerVolume(127, 127, 127), 32766u);
	EXPECT_EQ(MusicMixerVolume(127, 127, 12), 3096u);
	// 80 * 100 * 258 / 127 = 16251, then 16251 * 63 / 127 = 8061
	EXPECT_EQ(MusicMixerVolume(80, 100, 63), 8061u);
	// The master volume is at most 127
	EXPECT_EQ(MusicMixerVolume(127, 127, 300), 32766u);
	EXPECT_EQ(MusicMixerVolume(127, 127, 0), 0u);
}

TEST(MixVolume, TheMixerIsLinear)
{
	EXPECT_FLOAT_EQ(MixerGain(0), 0.0f);
	EXPECT_FLOAT_EQ(MixerGain(32767), 1.0f);
	EXPECT_FLOAT_EQ(MixerGain(12u * 258u), 3096.0f / 32767.0f);
}
