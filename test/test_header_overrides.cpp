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

#include "Sound/HeaderOverrides.h"

using namespace openblack::audio;
using openblack::pack::AudioBankOverride;

namespace
{
constexpr uint32_t Bit(AudioBankOverride setting)
{
	return static_cast<uint32_t>(setting);
}

// A fake bank header with every play setting, and a mid random number
constexpr HeaderPlaySettings k_FullHeader {
    .overrideFlags = Bit(AudioBankOverride::Volume) | Bit(AudioBankOverride::Pitch) | Bit(AudioBankOverride::MinDist) |
                     Bit(AudioBankOverride::MaxDist) | Bit(AudioBankOverride::Scale),
    .volume = 50,
    .pitchPercent = 150,
    .pitchDeviation = 0,
    .minDistance = 15.0f,
    .maxDistance = 150.0f,
    .distanceScale = 0.5f,
};
constexpr CallerPlaySettings k_Caller {
    .volume = 85,
    .pitchPercent = 120,
    .minDistance = 66.6f,
    .maxDistance = 200.0f,
};
} // namespace

TEST(HeaderOverrides, HeaderSettingsWinOverTheCaller)
{
	const auto start = ResolveStartSettings(k_FullHeader, k_Caller, 0);
	EXPECT_EQ(start.volume, 50u);
	EXPECT_EQ(start.pitchPercent, 150u);
	EXPECT_EQ(start.minDistance, 15.0f);
	EXPECT_EQ(start.maxDistance, 150.0f);
	EXPECT_EQ(start.distanceScale, 0.5f);
	EXPECT_EQ(start.headerSet, k_FullHeader.overrideFlags);
}

TEST(HeaderOverrides, CallerSettingsApplyWithoutTheHeaderBits)
{
	const auto start = ResolveStartSettings({.volume = 50, .pitchPercent = 150, .minDistance = 15.0f}, k_Caller, 0);
	EXPECT_EQ(start.volume, 85u);
	EXPECT_EQ(start.pitchPercent, 120u);
	EXPECT_EQ(start.minDistance, 66.6f);
	EXPECT_EQ(start.maxDistance, 200.0f);
	EXPECT_EQ(start.distanceScale, k_PlayDefaultDistanceScale);
	EXPECT_EQ(start.headerSet, 0u);
}

TEST(HeaderOverrides, EmptyCallerValuesKeepTheDefaults)
{
	const auto start = ResolveStartSettings({}, {}, 0);
	EXPECT_EQ(start, (StartPlaySettings {.volume = 127,
	                                     .pan = 90,
	                                     .pitchPercent = 100,
	                                     .minDistance = 1.0f,
	                                     .maxDistance = 9999.0f,
	                                     .distanceScale = 0.3f,
	                                     .headerSet = 0}));
}

TEST(HeaderOverrides, VolumeIsClampedAfterTheOverride)
{
	EXPECT_EQ(ResolveStartSettings({}, {.volume = 200}, 0).volume, 127u);
	EXPECT_EQ(ResolveStartSettings({.overrideFlags = Bit(AudioBankOverride::Volume), .volume = 300}, {.volume = 10}, 0).volume,
	          127u);
	EXPECT_EQ(ResolveStartSettings({.overrideFlags = Bit(AudioBankOverride::Volume), .volume = 0}, {.volume = 10}, 0).volume,
	          0u);
}

TEST(HeaderOverrides, PitchOfZeroPlaysAtTheNormalRate)
{
	EXPECT_EQ(ResolveStartSettings({}, {.pitchPercent = 0}, 0).pitchPercent, 100u);
	EXPECT_EQ(
	    ResolveStartSettings({.overrideFlags = Bit(AudioBankOverride::Pitch), .pitchPercent = 0}, {.pitchPercent = 140}, 0)
	        .pitchPercent,
	    100u);
}

TEST(HeaderOverrides, DeviationAlsoChangesTheCallersPitch)
{
	const HeaderPlaySettings header {.pitchDeviation = 10};
	const CallerPlaySettings caller {.pitchPercent = 150};
	// 15 down at the lowest random number, 15 up at the highest
	EXPECT_EQ(ResolveStartSettings(header, caller, 0).pitchPercent, 135u);
	EXPECT_EQ(ResolveStartSettings(header, caller, k_PlayRandomMax).pitchPercent, 165u);
	// 16384 * 30 / 32767 = 15 (integer division)
	EXPECT_EQ(ResolveStartSettings(header, caller, 0x4000).pitchPercent, 150u);
	// 1 * 30 / 32767 = 0
	EXPECT_EQ(ResolveStartSettings(header, caller, 1).pitchPercent, 135u);
	EXPECT_EQ(ResolveStartSettings(header, {}, 0).pitchPercent, 90u);
	// 10% of 105 is 10 (integer division)
	EXPECT_EQ(ResolveStartSettings({.pitchDeviation = 10}, {.pitchPercent = 105}, 0).pitchPercent, 95u);
}

TEST(HeaderOverrides, DeviationAppliesToTheHeadersPitch)
{
	const HeaderPlaySettings header {.overrideFlags = Bit(AudioBankOverride::Pitch), .pitchPercent = 200, .pitchDeviation = 5};
	EXPECT_EQ(ResolveStartSettings(header, {.pitchPercent = 100}, 0).pitchPercent, 190u);
	EXPECT_EQ(ResolveStartSettings(header, {.pitchPercent = 100}, k_PlayRandomMax).pitchPercent, 210u);
}

TEST(HeaderOverrides, CallerDistancesCanWin)
{
	auto caller = k_Caller;
	caller.callerWins = k_CallerDistancesWin;
	const auto start = ResolveStartSettings(k_FullHeader, caller, 0);
	EXPECT_EQ(start.minDistance, 66.6f);
	EXPECT_EQ(start.maxDistance, 200.0f);
	// Only the distances: the volume, the pitch and the scale stay the header's
	EXPECT_EQ(start.volume, 50u);
	EXPECT_EQ(start.pitchPercent, 150u);
	EXPECT_EQ(start.distanceScale, 0.5f);
	EXPECT_EQ(start.headerSet, Bit(AudioBankOverride::Volume) | Bit(AudioBankOverride::Pitch) | Bit(AudioBankOverride::Scale));
}

TEST(HeaderOverrides, LaterChangesDoNothingOnHeaderSettings)
{
	EXPECT_TRUE(VolumeChangeApplies(0));
	EXPECT_TRUE(VolumeChangeApplies(Bit(AudioBankOverride::Pitch)));
	EXPECT_FALSE(VolumeChangeApplies(Bit(AudioBankOverride::Volume)));
	EXPECT_TRUE(PitchChangeApplies(Bit(AudioBankOverride::Volume), 120));
	EXPECT_FALSE(PitchChangeApplies(Bit(AudioBankOverride::Pitch), 120));
	EXPECT_FALSE(PitchChangeApplies(0, 0));
}

TEST(HeaderOverrides, TheHeaderPanWinsOverTheCallerPan)
{
	const HeaderPlaySettings header {.overrideFlags = static_cast<uint32_t>(AudioBankOverride::Pan), .pan = 30};
	EXPECT_EQ(ResolveStartSettings(header, {.pan = 150}, 0).pan, 30u);
	EXPECT_EQ(ResolveStartSettings({.pan = 30}, {.pan = 150}, 0).pan, 150u);
	EXPECT_EQ(ResolveStartSettings({}, {}, 0).pan, k_PlayCentrePan);
}

TEST(HeaderOverrides, ANegativeHeaderPanIsReadAsAnUnsignedNumber)
{
	const HeaderPlaySettings header {.overrideFlags = static_cast<uint32_t>(AudioBankOverride::Pan), .pan = -1};
	EXPECT_EQ(ResolveStartSettings(header, {}, 0).pan, 0xFFFFFFFFu);
}
