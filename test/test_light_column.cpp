/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "Graphics/LightColumn.h"

using namespace openblack;
using namespace openblack::graphics::light_column;

TEST(LightColumn, ShowsOnlyForAPlayerWhoseBorderShows)
{
	EXPECT_TRUE(Shown(PlayerNames::PLAYER_ONE, true));
	EXPECT_FALSE(Shown(PlayerNames::PLAYER_ONE, false));
	EXPECT_FALSE(Shown(PlayerNames::NEUTRAL, true));
}

TEST(LightColumn, TheTempleKeepsItsColumnForTheFirstPartOfItsDestruction)
{
	EXPECT_TRUE(TempleShown(false, 30.0f));
	EXPECT_TRUE(TempleShown(true, 21.7f));
	EXPECT_FALSE(TempleShown(true, 21.8f));
}

TEST(LightColumn, TheTemplesStrengthBeatsFrom55To110RoundedDown)
{
	EXPECT_EQ(TempleAlpha(0.0f), 55);
	EXPECT_EQ(TempleAlpha(1.0f), 110);
	EXPECT_EQ(TempleAlpha(0.5f), 82);
	EXPECT_EQ(TempleAlpha(-1.0f), 55);
	EXPECT_EQ(TempleAlpha(2.0f), 110);
	EXPECT_EQ(k_TownAlpha, 20);
}

TEST(LightColumn, TintIsTheOwnersColourAtItsStrength)
{
	const auto tint = Tint(0xFFFF4646u, 20);
	EXPECT_FLOAT_EQ(tint.r, 1.0f);
	EXPECT_FLOAT_EQ(tint.g, 70.0f / 255.0f);
	EXPECT_FLOAT_EQ(tint.b, 70.0f / 255.0f);
	EXPECT_FLOAT_EQ(tint.a, 20.0f / 255.0f);
}

TEST(LightColumn, ItStandsAtItsPointFacingTheEyeThreeTimesItsSizeAndStretchedUpright)
{
	const glm::vec3 position {100.0f, 20.0f, 200.0f};
	const glm::vec3 eye {100.0f, 50.0f, 100.0f};
	const auto model = Model(position, eye);
	EXPECT_EQ(glm::vec3(model[3]), position);
	EXPECT_NEAR(glm::length(glm::vec3(model[0])), 3.0f, 1e-5f);
	EXPECT_NEAR(glm::length(glm::vec3(model[1])), 9.0f, 1e-5f);
	EXPECT_NEAR(glm::length(glm::vec3(model[2])), 3.0f, 1e-5f);
	// Its depth points away from the eye, across the ground, and its side is square to that
	const auto away = glm::normalize(glm::vec3(model[2]));
	EXPECT_NEAR(away.x, 0.0f, 1e-5f);
	EXPECT_NEAR(away.y, 0.0f, 1e-5f);
	EXPECT_NEAR(away.z, 1.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(glm::vec3(model[0]), glm::vec3(model[2])), 0.0f, 1e-4f);
}
