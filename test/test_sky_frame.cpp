/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "3D/SkyFrame.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

// 2026-01-01
constexpr int64_t k_Date = 1767225600;

sky_frame::Inputs Frame(float hour, std::optional<float> overcast = 0.0f, bool fog = true)
{
	return {.scriptHour = hour, .unixTime = k_Date, .overcast = overcast, .fog = fog, .moonColour = 0x336699};
}

} // namespace

TEST(SkyFrame, AtNoonTheSunIsUpAndTheMoonDown)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	sky_frame::Update(Frame(12.0f), dome, sun, moon);

	const auto placement = graphics::sun::Place(12.0f);
	ASSERT_TRUE(placement.has_value());
	ASSERT_TRUE(sun.placement.has_value());
	EXPECT_EQ(sun.placement->position, placement->position);
	EXPECT_EQ(sun.strength, std::trunc(placement->alpha));
	EXPECT_FALSE(moon.placement.has_value());
	EXPECT_EQ(moon.strength, 0.0f);
}

TEST(SkyFrame, AtMidnightTheMoonIsUpWithItsPhaseAndColour)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	sky_frame::Update(Frame(0.0f), dome, sun, moon);

	const auto placement = graphics::moon::Place(0.0f);
	ASSERT_TRUE(placement.has_value());
	ASSERT_TRUE(moon.placement.has_value());
	EXPECT_EQ(moon.placement->offset, placement->offset);
	EXPECT_EQ(moon.strength, std::trunc(placement->alpha));
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Date));
	EXPECT_EQ(moon.colour, glm::vec3(0x33, 0x66, 0x99) / 255.0f);
	EXPECT_FALSE(sun.placement.has_value());
	EXPECT_EQ(sun.strength, 0.0f);
}

TEST(SkyFrame, AFullOvercastLeavesANinthOnlyWithTheFogSetting)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	sky_frame::Update(Frame(12.0f, 1.0f), dome, sun, moon);
	EXPECT_EQ(dome.overcast, 1.0f);
	EXPECT_EQ(sun.strength, std::trunc(std::trunc(graphics::sun::Place(12.0f)->alpha) / 9.0f));

	sky_frame::Update(Frame(12.0f, 1.0f, false), dome, sun, moon);
	EXPECT_EQ(sun.strength, std::trunc(graphics::sun::Place(12.0f)->alpha));
}

TEST(SkyFrame, WithoutTheLightPaletteTheLastOvercastIsKept)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	sky_frame::Update(Frame(12.0f, 0.5f), dome, sun, moon);
	sky_frame::Update(Frame(12.0f, std::nullopt), dome, sun, moon);
	EXPECT_EQ(dome.overcast, 0.5f);
	EXPECT_EQ(sun.strength, sky_dome::ThroughOvercast(graphics::sun::Place(12.0f)->alpha, 0.5f, true));
}

TEST(SkyFrame, MoonAtIsThePlaceAndPhaseAlone)
{
	const auto moon = sky_frame::MoonAt(1.0f, k_Date);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Date));
	ASSERT_TRUE(moon.placement.has_value());
	EXPECT_EQ(moon.placement->offset, graphics::moon::Place(1.0f)->offset);
	EXPECT_FALSE(sky_frame::MoonAt(12.0f, k_Date).placement.has_value());
}

TEST(SkyFrame, TheDomeFollowsOnlyWhileDrawn)
{
	SkyDome dome {.meshId = 0, .textureId = 0, .follow = sky_dome::Follow(2.0f)};
	// Not drawn: nothing to blend, and the follower waits
	sky_frame::AdvanceDome(dome, 2.0f, false);
	EXPECT_TRUE(dome.frameRows.Get().empty());

	// Drawn: the whole dome first, as a fresh follower gives it
	sky_dome::Follow expected(2.0f);
	sky_frame::AdvanceDome(dome, 2.0f, true);
	const auto rows = expected.Advance(2.0f);
	ASSERT_EQ(dome.frameRows.Get().size(), rows.Get().size());
	EXPECT_EQ(dome.frameRows.Get()[0], rows.Get()[0]);
}

TEST(SkyFrame, ColourIsOfTheThreeBytes)
{
	EXPECT_EQ(sky_frame::Colour(0xFF8000), glm::vec3(1.0f, 128.0f / 255.0f, 0.0f));
}
