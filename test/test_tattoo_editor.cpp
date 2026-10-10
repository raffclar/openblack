/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/TattooEditor.h"

using namespace openblack;
using namespace openblack::creature_tattoo_editor;
using creature_tattoo::k_NoSite;
using creature_tattoo::Slot;
using creature_tattoo::Slots;

namespace
{
constexpr glm::u8vec3 k_Red {255, 0, 0};
constexpr glm::u8vec3 k_Blue {0, 0, 255};

Slots Full()
{
	Slots slots {};
	for (uint8_t i = 0; i < slots.size(); ++i)
	{
		slots.at(i) = {.design = i, .site = i, .colour = k_Red};
	}
	return slots;
}
} // namespace

TEST(TattooEditor, OpeningKeepsTheTattoosToGoBackTo)
{
	auto slots = Full();
	const auto session = Open(slots);
	EXPECT_EQ(session.slots, slots);
	EXPECT_EQ(session.opened, slots);
	EXPECT_FALSE(session.changed);
}

TEST(TattooEditor, ASymbolDroppedOnAPlaceGoesInTheFirstEmptySlot)
{
	auto session = Open({});
	EXPECT_TRUE(Drop(session, 5, k_Red, 3));
	EXPECT_EQ(session.slots.at(0), (Slot {.design = 5, .site = 3, .colour = k_Red}));
	EXPECT_TRUE(session.changed);
	EXPECT_TRUE(Drop(session, 6, k_Blue, 4));
	EXPECT_EQ(session.slots.at(1), (Slot {.design = 6, .site = 4, .colour = k_Blue}));
}

TEST(TattooEditor, TheSameSymbolOnTheSamePlaceOnlyTakesTheNewColour)
{
	auto session = Open({});
	ASSERT_TRUE(Drop(session, 5, k_Red, 3));
	EXPECT_FALSE(Drop(session, 5, k_Blue, 3));
	EXPECT_EQ(session.slots.at(0), (Slot {.design = 5, .site = 3, .colour = k_Blue}));
	EXPECT_TRUE(session.slots.at(1).Empty());
}

TEST(TattooEditor, ANewSymbolOnAPlaceStaysWithAnotherWhileASlotIsEmpty)
{
	auto session = Open({});
	ASSERT_TRUE(Drop(session, 5, k_Red, 3));
	ASSERT_TRUE(Drop(session, 6, k_Red, 3));
	EXPECT_EQ(session.slots.at(0).site, 3);
	EXPECT_EQ(session.slots.at(1).site, 3);
}

TEST(TattooEditor, WithEverySlotFullASymbolReplacesWhatIsOnThePlace)
{
	auto session = Open(Full());
	EXPECT_TRUE(Drop(session, 12, k_Blue, 2));
	EXPECT_EQ(session.slots.at(2), (Slot {.design = 12, .site = 2, .colour = k_Blue}));
}

TEST(TattooEditor, DroppingAwayFromThePlacesChangesNothing)
{
	auto session = Open({});
	EXPECT_FALSE(Drop(session, 5, k_Red, std::nullopt));
	EXPECT_EQ(session.slots, Slots {});
	EXPECT_FALSE(session.changed);
}

TEST(TattooEditor, LiftingTakesTheLastTattooOnThePlaceKeepingItsSymbol)
{
	auto session = Open({});
	ASSERT_TRUE(Drop(session, 5, k_Red, 3));
	ASSERT_TRUE(Drop(session, 6, k_Blue, 3));
	session.changed = false;
	const auto lifted = Lift(session, 3);
	ASSERT_TRUE(lifted.has_value());
	EXPECT_EQ(*lifted, (Slot {.design = 6, .site = 3, .colour = k_Blue}));
	EXPECT_EQ(session.slots.at(1).site, k_NoSite);
	EXPECT_EQ(session.slots.at(1).design, 6);
	EXPECT_EQ(session.slots.at(0).site, 3);
	EXPECT_TRUE(session.changed);
	EXPECT_FALSE(Lift(session, 7).has_value());
}

TEST(TattooEditor, OkWarnsOnlyInANetworkGameAfterAChange)
{
	auto session = Open({});
	EXPECT_EQ(Ok(session, true), Accept::Close);
	ASSERT_TRUE(Drop(session, 1, k_Red, 0));
	EXPECT_EQ(Ok(session, false), Accept::Close);
	EXPECT_EQ(Ok(session, true), Accept::Warn);
}

TEST(TattooEditor, ThePaletteCellSpansTheWholeControl)
{
	EXPECT_EQ(PaletteCell({5, 35}), glm::uvec2(0, 0));
	// 52 wide across 32 columns, 519 high across 128 rows
	EXPECT_EQ(PaletteCell({5 + 26, 35 + 260}), glm::uvec2(16, 64));
	EXPECT_EQ(PaletteCell({56, 553}), glm::uvec2(31, 127));
	EXPECT_EQ(PaletteCell({-100, -100}), glm::uvec2(0, 0));
	EXPECT_EQ(PaletteCell({900, 900}), glm::uvec2(31, 127));
}

TEST(TattooEditor, SlidersFollowThePointerWithinTheirHeight)
{
	EXPECT_FLOAT_EQ(SliderPosition(35, k_BrightnessRect), 0.0f);
	EXPECT_FLOAT_EQ(SliderPosition(554, k_BrightnessRect), 1.0f);
	EXPECT_FLOAT_EQ(SliderPosition(0, k_BrightnessRect), 0.0f);
	EXPECT_FLOAT_EQ(SliderPosition(1000, k_BrightnessRect), 1.0f);
	EXPECT_NEAR(SliderPosition(294, k_BrightnessRect), 259.0f / 519.0f, 1e-6f);
}

TEST(TattooEditor, ThePlaceUnderThePointerIsTheNearestFacingOne)
{
	const std::array sites {
	    SiteOnScreen {.site = 0, .screen = {100, 100}, .facingCamera = 0.9f},
	    SiteOnScreen {.site = 1, .screen = {110, 100}, .facingCamera = -0.5f},
	    SiteOnScreen {.site = 2, .screen = {130, 100}, .facingCamera = -0.1f},
	};
	EXPECT_EQ(SiteUnderPointer(sites, {112, 100}), 0);
	EXPECT_EQ(SiteUnderPointer(sites, {125, 100}), 2);
	// No further than 64 pixels
	EXPECT_EQ(SiteUnderPointer(sites, {100, 164}), 0);
	EXPECT_EQ(SiteUnderPointer(sites, {100, 165}), std::nullopt);
	EXPECT_EQ(SiteUnderPointer({}, {0, 0}), std::nullopt);
}

TEST(TattooEditor, SteeringSpeedsTheViewUpByHowFarThePointerIs)
{
	Orbit orbit;
	Steer(orbit, {100, 100}, {90, 120}, 10.0f);
	EXPECT_FLOAT_EQ(orbit.turnSpeed, 10.0f * 10.0f * 0.001f * 0.2f);
	EXPECT_FLOAT_EQ(orbit.tipSpeed, 20.0f * 10.0f * 0.001f * 0.1f);
}

TEST(TattooEditor, TheViewTurnsRoundItsFocusAndSlowsDown)
{
	Orbit orbit {.eye = {10.0f, 5.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 1.0f, .tipSpeed = 0.0f};
	Turn(orbit, 100.0f);
	const auto angle = 1.0f * 100.0f * 0.0003f;
	EXPECT_NEAR(orbit.eye.x, 10.0f * std::cos(angle), 1e-5f);
	EXPECT_NEAR(orbit.eye.z, 10.0f * std::sin(angle), 1e-5f);
	EXPECT_NEAR(orbit.eye.y, 5.0f, 1e-5f);
	EXPECT_NEAR(orbit.turnSpeed, std::exp(100.0f * 0.0003f * -8.0f), 1e-6f);
}

TEST(TattooEditor, TheViewStaysBetweenAQuarterAndAsHighAsItIsFar)
{
	Orbit low {.eye = {10.0f, 0.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 0.0f, .tipSpeed = 0.0f};
	Turn(low, 10.0f);
	EXPECT_FLOAT_EQ(low.eye.y, 2.5f);
	Orbit high {.eye = {10.0f, 50.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 0.0f, .tipSpeed = 0.0f};
	Turn(high, 10.0f);
	EXPECT_FLOAT_EQ(high.eye.y, 10.0f);
	Orbit tipping {.eye = {10.0f, 5.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 0.0f, .tipSpeed = 1.0f};
	Turn(tipping, 100.0f);
	EXPECT_NEAR(tipping.eye.y, 5.0f + (10.0f * 100.0f * 0.0003f), 1e-5f);
}

TEST(TattooEditor, TheCaveLooksAtItsCreatureFromBesideAndAbove)
{
	const auto view = CaveView({10.0f, 0.0f, 20.0f}, 0.5f);
	EXPECT_EQ(view.focus, glm::vec3(10.0f, 3.75f, 20.0f));
	EXPECT_EQ(view.eye, glm::vec3(10.0f - 11.25f, 5.625f, 20.0f));
	EXPECT_EQ(CaveView({}, 0.01f).focus, glm::vec3(0.0f, 0.5f, 0.0f));
	EXPECT_EQ(CaveView({}, 5.0f).focus, glm::vec3(0.0f, 10.0f, 0.0f));
}
