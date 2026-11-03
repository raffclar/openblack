/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/ScriptWeatherRules.h"
#include "Sound/ScriptMusic.h"

using namespace openblack;

TEST(ScriptNativesRows5, TheMusicDistanceIsTheFarDistanceOfTheBank)
{
	audio::MusicTiming bank;
	bank.maxDistance = 250.0f;
	EXPECT_EQ(audio::MusicPlayDistance(&bank), 250.0f);
	bank.maxDistance = 0.0f;
	EXPECT_EQ(audio::MusicPlayDistance(&bank), 0.0f);
}

TEST(ScriptNativesRows5, TheMusicDistanceIsOneHundredWithoutABankOrAFarDistance)
{
	EXPECT_EQ(audio::MusicPlayDistance(nullptr), 100.0f);
	// A bank with no chunks, or a bank that gives a distance below 0
	audio::MusicTiming bank;
	EXPECT_EQ(audio::MusicPlayDistance(&bank), 100.0f);
	bank.maxDistance = -0.5f;
	EXPECT_EQ(audio::MusicPlayDistance(&bank), 100.0f);
	bank.maxDistance = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(audio::MusicPlayDistance(&bank), 100.0f);
}

TEST(ScriptNativesRows5, WindMagicIsAStormWithNoCloudsNearThePoint)
{
	namespace weather = ecs::script_weather;
	const glm::vec3 point {1000.0f, 20.0f, 2000.0f};
	// The edge of a storm of outer radius 40 is 5 into a circle of 25 round the point
	EXPECT_TRUE(weather::WindMagicNear({point.x + 60.0f, 0.0f, point.z}, 40.0f, 0, point, 25.0f));
	// A storm with clouds of its own is not wind magic
	EXPECT_FALSE(weather::WindMagicNear({point.x + 60.0f, 0.0f, point.z}, 40.0f, 8, point, 25.0f));
	// The edge is near the circle but not in it
	EXPECT_FALSE(weather::WindMagicNear({point.x, 0.0f, point.z - 65.5f}, 40.0f, 0, point, 25.0f));
	// Only the distance across the land counts, not the height
	EXPECT_TRUE(weather::WindMagicNear({point.x + 30.0f, 900.0f, point.z + 40.0f}, 25.5f, 0, point, 25.0f));
	EXPECT_FALSE(weather::WindMagicNear({point.x + 30.0f, 900.0f, point.z + 40.0f}, 24.5f, 0, point, 25.0f));
}

TEST(ScriptNativesRows5, EveryComputerOfANetworkGameHasTheBanksOfTheMusicDistances)
{
	// A lone computer with no sound device has no banks, so the distances read 100 there
	EXPECT_FALSE(audio::WorldHasMusic(false, false, false));
	EXPECT_TRUE(audio::WorldHasMusic(true, false, false));
	// In a network game and on the server, each computer reads the distances of the banks
	EXPECT_TRUE(audio::WorldHasMusic(false, true, false));
	EXPECT_TRUE(audio::WorldHasMusic(false, false, true));
}
