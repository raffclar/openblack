/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/LandLightTable.h"
#include "3D/SkyFrame.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

// 2026-01-01
constexpr int64_t k_Date = 1767225600;

/// A palette whose moon row is 0x336699 at every time of day and alignment, everything else black
const LandLightPalette& FakePalette()
{
	static const LandLightPalette palette = []() {
		constexpr size_t k_MoonRow = 5;
		std::vector<uint8_t> bytes(LandLightPalette::k_Side * LandLightPalette::k_Side * 4, 0);
		for (size_t column = 0; column < LandLightPalette::k_Side; ++column)
		{
			const auto at = (k_MoonRow * LandLightPalette::k_Side + column) * 4;
			bytes[at] = 0x33;
			bytes[at + 1] = 0x66;
			bytes[at + 2] = 0x99;
			bytes[at + 3] = 0xFF;
		}
		return LandLightPalette(bytes);
	}();
	return palette;
}

/// A frame with the land's light palette and the given overcast, or without the palette for none
sky_frame::Inputs Frame(float hour, std::optional<float> overcast = 0.0f, bool fog = true)
{
	return {
	    .scriptHour = hour,
	    .unixTime = k_Date,
	    .ticks = 100000,
	    .landLight = {.skyType = 0.5f, .alignment = -0.25f, .overcast = overcast.value_or(0.75f), .flash = 3},
	    .palette = overcast.has_value() ? &FakePalette() : nullptr,
	    .fog = fog,
	};
}

/// A clear midnight at a date and a time of the machine's clock
sky_frame::Inputs Midnight(int64_t date, uint32_t ticks)
{
	auto frame = Frame(0.0f);
	frame.unixTime = date;
	frame.ticks = ticks;
	return frame;
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

TEST(SkyFrame, TheMoonsColourIsThePalettesForTheLandsLight)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	const auto frame = Frame(0.0f, 0.25f);
	sky_frame::Update(frame, dome, sun, moon);
	EXPECT_EQ(moon.colour, sky_frame::Colour(LandLightTable::GetMoonColour(FakePalette(), frame.landLight.skyType,
	                                                                       frame.landLight.alignment)));

	// White without the palette
	sky_frame::Update(Frame(0.0f, std::nullopt), dome, sun, moon);
	EXPECT_EQ(moon.colour, glm::vec3(1.0f));
}

TEST(SkyFrame, TheDomeKeepsTheFramesLandLightForTheLandsLightTable)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	sky_frame::Update(Frame(12.0f, 0.25f), dome, sun, moon);
	EXPECT_EQ(dome.landLight.skyType, 0.5f);
	EXPECT_EQ(dome.landLight.alignment, -0.25f);
	EXPECT_EQ(dome.landLight.overcast, 0.25f);
	EXPECT_EQ(dome.landLight.flash, 3);

	// Without the palette too, though the sun and moon keep the overcast they last showed through
	sky_frame::Update(Frame(12.0f, std::nullopt), dome, sun, moon);
	EXPECT_EQ(dome.landLight.overcast, 0.75f);
	EXPECT_EQ(dome.overcast, 0.25f);
}

TEST(SkyFrame, TheMoonTakesItsPhaseOnlyWhileItShows)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	sky_frame::Update(Frame(12.0f), dome, sun, moon);
	EXPECT_EQ(moon.phase, 0.0f);
	EXPECT_EQ(moon.date, 0);

	sky_frame::Update(Frame(0.0f), dome, sun, moon);
	const float shown = moon.phase;
	EXPECT_EQ(shown, graphics::moon::Phase(k_Date));

	// By day it keeps the phase it last showed with, whatever the date
	auto noon = Frame(12.0f);
	noon.unixTime = k_Date + (10 * 86400);
	noon.ticks += 60000;
	sky_frame::Update(noon, dome, sun, moon);
	EXPECT_EQ(moon.phase, shown);

	// A full overcast with the fog setting leaves it a ninth: still showing
	sky_frame::Update(Frame(0.0f, 1.0f), dome, sun, moon);
	EXPECT_GT(moon.strength, 0.0f);
}

TEST(SkyFrame, TheDateIsReadAtMostOnceEveryTwoSeconds)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	constexpr int64_t k_Later = k_Date + (7 * 86400);
	sky_frame::Update(Midnight(k_Date, 5000), dome, sun, moon);
	EXPECT_EQ(moon.date, k_Date);
	EXPECT_EQ(moon.dateReadAt, 5000u);

	// Two seconds exactly is not yet more than two seconds: the date read is kept
	sky_frame::Update(Midnight(k_Later, 7000), dome, sun, moon);
	EXPECT_EQ(moon.date, k_Date);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Date));

	sky_frame::Update(Midnight(k_Later, 7001), dome, sun, moon);
	EXPECT_EQ(moon.date, k_Later);
	EXPECT_EQ(moon.dateReadAt, 7001u);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Later));
}

TEST(SkyFrame, TheMachinesClockWrapsBetweenReads)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	moon.dateReadAt = 0xFFFFFF00u;
	moon.date = k_Date;
	// 0x100 + 1000 milliseconds after the last read, across the wrap: the date is kept
	sky_frame::Update(Midnight(k_Date + 86400, 1000), dome, sun, moon);
	EXPECT_EQ(moon.date, k_Date);
}

TEST(SkyFrame, ADateOverrideGivesThePhase)
{
	SkyDome dome {};
	Sun sun;
	Moon moon;
	constexpr int64_t k_Override = k_Date + (12 * 86400);
	moon.dateOverride = k_Override;
	sky_frame::Update(Frame(0.0f), dome, sun, moon);
	EXPECT_EQ(sky_frame::MoonDate(moon), k_Override);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Override));
	// The computer's date is still read and kept for when the override goes
	EXPECT_EQ(moon.date, k_Date);
	moon.dateOverride.reset();
	EXPECT_EQ(sky_frame::MoonDate(moon), k_Date);
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
