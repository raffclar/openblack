/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <limits>

#include <gtest/gtest.h>

#include "Audio/Engine/SamplePlay.h"
#include "Audio/Services/AtmosBanks.h"
#include "ECS/AudioQueries.h"

using namespace openblack::audio;

// The expected values come from an emulation of the original audio DLLs: the mixer's volume and distance laws and
// the conversion of a relative position to the mixer's axes.

TEST(AudioLaws, QMixerVolume)
{
	// floor(127 * v / 127) * 258 / 32767
	EXPECT_NEAR(sample_play::QMixerGain(127), 32766.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(60), 15480.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(20), 5160.0f / 32767.0f, 1e-6f);
	EXPECT_NEAR(sample_play::QMixerGain(1), 258.0f / 32767.0f, 1e-6f);
	EXPECT_EQ(sample_play::QMixerGain(0), 0.0f);
	EXPECT_NEAR(sample_play::QMixerGain(200), 32766.0f / 32767.0f, 1e-6f);
}

TEST(AudioLaws, QMixerDistance)
{
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 0), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 50), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 60), 0.5555556f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 100), 0.2f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(50, 160, 4, 160), 0.1020408f, 1e-6f);
	EXPECT_EQ(sample_play::DistanceGain(50, 160, 4, 161), 0.0f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 2, 2.828427f), 0.2147372f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 1, 5), 0.2f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(1, 9999, 0, 10), 1.0f, 1e-6f);
	EXPECT_NEAR(sample_play::DistanceGain(60, 180, 0.3f, 200), 0.0f, 1e-6f);
}

TEST(AudioLaws, RelativeAxes)
{
	// LHaudio's relative (x, y, z) -> QMixer's (right, up, ahead)
	struct Case
	{
		glm::vec3 lh;
		glm::vec3 heard;
	};
	const Case cases[] = {
	    {{2, 2, 0}, {1.9992f, 0.0f, 2.0008f}},
	    {{-2, 1, 0}, {-1.9998f, 0.0f, 1.0005f}},
	    {{0, 4, 0}, {0, 0, 4}},
	    {{0, -4, 0}, {0, 0, -4}},
	    {{4, 0, 0}, {4, 0, 0}},
	    {{-4, 0, 0}, {-4, 0, 0}},
	    {{-5, -5, 0}, {-4.9980f, 0.0f, -5.0020f}},
	    {{1, -2, 0}, {0.9989f, 0.0f, -2.0006f}},
	    {{2, -1.5f, 0}, {2.2358f, 0.0f, -1.1186f}},
	    {{0, 0, 3}, {0, 3, 0}},
	    {{1, 1, 1}, {0.9994f, 1.0004f, 1.0002f}},
	};
	for (const auto& c : cases)
	{
		const auto p = sample_play::PolarRelative(c.lh);
		EXPECT_NEAR(p.x, c.heard.x, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.y, c.heard.y, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.z, c.heard.z, 2e-4f) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
	}
}

TEST(AudioLaws, RelativeAxesExact)
{
	// both halves emulated with the original float code: the audio library's with the FPU at 24 bits as on the game
	// thread, the mixer's at 53 bits as on its timer thread, all digits. The doubles 180 * 0.31847133757961782,
	// the float angles and range, pi * 0.0055555557f and the float az / flat / up of the mixer.
	struct Case
	{
		glm::vec3 lh;
		glm::vec3 heard;
	};
	const Case cases[] = {
	    {{2, 2, 0}, {1.99920309f, 0.0f, 2.00079656f}},
	    {{-2, 1, 0}, {-1.99976468f, 0.0f, 1.00047064f}},
	    {{-5, -5, 0}, {-4.99800825f, 0.0f, -5.0019908f}},
	    {{1, -2, 0}, {0.998876214f, 0.0f, -2.00056148f}},
	    {{2, -1.5f, 0}, {2.2358048f, 0.0f, -1.11855996f}},
	    {{1, 1, 1}, {0.999380827f, 1.00044143f, 1.00017738f}},
	    {{123.25f, -48.5f, 7.75f}, {123.410568f, 7.75392675f, -48.0893211f}},
	    {{-300.5f, 210.25f, -15.5f}, {-300.269836f, -15.5078573f, 210.578003f}},
	    // |y| truncated to 0: the azimuth is 90
	    {{0.75f, -0.25f, 2.5f}, {0.788965642f, 2.50050664f, -3.4486785e-08f}},
	};
	for (const auto& c : cases)
	{
		const auto p = sample_play::PolarRelative(c.lh);
		const auto tolerance = [](float v) { return std::abs(v) * 2e-7f + 1e-6f; };
		EXPECT_NEAR(p.x, c.heard.x, tolerance(c.heard.x)) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.y, c.heard.y, tolerance(c.heard.y)) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
		EXPECT_NEAR(p.z, c.heard.z, tolerance(c.heard.z)) << c.lh.x << "," << c.lh.y << "," << c.lh.z;
	}
}

TEST(AudioLaws, AlignmentCurve)
{
	// x clamped to 0..1 (a NaN is 0), 2 - 2 (1 - x) - 1
	using openblack::ecs::audio_queries::AudioAlignmentValue;
	EXPECT_EQ(AudioAlignmentValue(0.0f), -1.0f);
	EXPECT_EQ(AudioAlignmentValue(0.5f), 0.0f);
	EXPECT_EQ(AudioAlignmentValue(1.0f), 1.0f);
	EXPECT_EQ(AudioAlignmentValue(0.25f), -0.5f);
	EXPECT_EQ(AudioAlignmentValue(0.9f), 2.0f - ((1.0f - 0.9f) + (1.0f - 0.9f)) - 1.0f);
	EXPECT_EQ(AudioAlignmentValue(-3.0f), -1.0f);
	EXPECT_EQ(AudioAlignmentValue(2.0f), 1.0f);
	EXPECT_EQ(AudioAlignmentValue(std::numeric_limits<float>::quiet_NaN()), -1.0f);
}

TEST(AudioLaws, AtmosGroupByAlignment)
{
	// ProcessAtmosBanks: the float alignment against the double -0.59999999999999998; at or below it -> 2
	EXPECT_EQ(atmos_banks::GroupFor(-1.0f), 2u);
	EXPECT_EQ(atmos_banks::GroupFor(-0.6f), 2u); // the float -0.6 is -0.60000002384, below the double
	EXPECT_EQ(atmos_banks::GroupFor(std::nextafter(-0.6f, 0.0f)), 1u);
	EXPECT_EQ(atmos_banks::GroupFor(0.0f), 1u);
	EXPECT_EQ(atmos_banks::GroupFor(1.0f), 1u);
	EXPECT_EQ(atmos_banks::GroupFor(std::numeric_limits<float>::quiet_NaN()), 2u);
}
