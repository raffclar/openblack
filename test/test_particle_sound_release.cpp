/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <utility>

#include <gtest/gtest.h>

#include "Particles/ParticleSoundRelease.h"
#include "Particles/ParticleSounds.h"

using namespace openblack::particles;

TEST(ParticleSoundRelease, ASoftReleasedLoopPlaysToTheEndOfItsPassFadingByItsStep)
{
	// The lightning's hand crackle: a looping sample the file soft-releases with a fade step of 20
	const auto letGo = LetGoOf(true, true, 20);
	EXPECT_TRUE(letGo.releaseLoop);
	EXPECT_EQ(letGo.fadeStep, 20);
	EXPECT_FALSE(letGo.stopNow);
}

TEST(ParticleSoundRelease, ASoundTheFileDoesNotSoftReleaseIsStoppedAtOnce)
{
	// The spiritual shield's hum: a looping sample the file doesn't soft-release
	EXPECT_EQ(LetGoOf(false, true, 0), (SoundLetGo {.stopNow = true}));
	// Whatever its fade step, and whether its sample loops or not
	EXPECT_EQ(LetGoOf(false, true, 40), (SoundLetGo {.stopNow = true}));
	EXPECT_EQ(LetGoOf(false, false, 0), (SoundLetGo {.stopNow = true}));
	EXPECT_EQ(LetGoOf(false, false, 20), (SoundLetGo {.stopNow = true}));
}

TEST(ParticleSoundRelease, ASoftReleasedSampleThatPlaysOncePlaysOut)
{
	const auto letGo = LetGoOf(true, false, 0);
	EXPECT_EQ(letGo, (SoundLetGo {.releaseLoop = false, .fadeStep = 0}));
	// With a step it still fades as it plays out
	EXPECT_EQ(LetGoOf(true, false, 40), (SoundLetGo {.releaseLoop = false, .fadeStep = 40}));
	// A soft-released loop without a step plays to the end of its pass at its volume
	EXPECT_EQ(LetGoOf(true, true, 0), (SoundLetGo {.releaseLoop = true, .fadeStep = 0}));
}

TEST(ParticleSoundRelease, TheVolumeFallsByTheStepEachTurnToSilence)
{
	uint32_t volume = k_MaxSoundVolume;
	int turns = 0;
	while (volume > 0)
	{
		volume = FadedVolume(volume, 20);
		++turns;
	}
	// 127 in steps of 20 is silent on the seventh turn, counting the turn it is let go on
	EXPECT_EQ(turns, 7);
	EXPECT_EQ(FadedVolume(50, 0), 50u);
	EXPECT_EQ(FadedVolume(10, -5), 10u);
}

TEST(ParticleSoundRelease, AThrowsSoundSizeComparesItsShareWithTheGamesDoubles)
{
	// Above six tenths of the fastest throw the sound is large, above three tenths medium, else small
	EXPECT_EQ(SoundSizeFromThrow(1.0f), 1);
	EXPECT_EQ(SoundSizeFromThrow(0.45f), 2);
	EXPECT_EQ(SoundSizeFromThrow(0.1f), 3);
	// The float nearest to six tenths is a little above the double six tenths, so a share of it is large
	EXPECT_EQ(SoundSizeFromThrow(0.6f), 1);
	EXPECT_EQ(SoundSizeFromThrow(0.3f), 2);
	// The float below it is small enough
	EXPECT_EQ(SoundSizeFromThrow(std::nextafter(0.6f, 0.0f)), 2);
	EXPECT_EQ(SoundSizeFromThrow(std::nextafter(0.3f, 0.0f)), 3);
	// A throw at 120 of the fastest 200 is large
	EXPECT_EQ(SoundSizeFromThrow(120.0f / 200.0f), 1);
}

namespace
{
/// A land whose height at a map position is that position, so that a test sees which position the sound asked for
class MapPositionLand final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return -1.0f; }
	[[nodiscard]] float LandHeightAtMap(int32_t x, int32_t z) const override
	{
		asked = {x, z};
		return static_cast<float>(x) + static_cast<float>(z);
	}
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }

	mutable std::pair<int32_t, int32_t> asked {0, 0};
};
} // namespace

// A sound on the land asks for the height at the map position of its point: 6553.6 fixed units a metre, cut off
TEST(ParticleSoundRelease, ASoundOnTheLandTakesTheHeightAtItsMapPosition)
{
	const MapPositionLand land;
	EXPECT_EQ(SoundHeightOnLand(land, 10.0f, 1.0f), 65536.0f + 6553.0f);
	EXPECT_EQ(land.asked, (std::pair<int32_t, int32_t> {65536, 6553}));
	// A negative position is cut off towards zero
	static_cast<void>(SoundHeightOnLand(land, -1.0f, 0.5f));
	EXPECT_EQ(land.asked, (std::pair<int32_t, int32_t> {-6553, 3276}));
}
