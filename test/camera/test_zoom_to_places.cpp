/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ZoomToPlaces.h"

using namespace openblack;
using namespace openblack::zoom_to;

namespace
{
constexpr float k_Tolerance = 1e-3f;
constexpr CameraView k_Start {.origin = {1000.0f, 200.0f, 700.0f}, .focus = {1000.0f, 0.0f, 1000.0f}};
constexpr glm::vec2 k_Temple {2000.0f, 2500.0f};
constexpr glm::vec2 k_Centre {2560.0f, 2400.0f};

/// Flat land 10 metres high
[[nodiscard]] float FlatLand(int32_t /*mapX*/, int32_t /*mapZ*/)
{
	return 10.0f;
}
} // namespace

TEST(ZoomToPlaces, TheConstantsAreTheGamesBits)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_TemplePitch), 0x3F490FD8u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_TapPitch), 0x3F060A90u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_RealmPitch), 0x3EC90FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_PlaceRise), 0x3ECCCCCDu);
	EXPECT_EQ(k_TempleDistance, 130.0f);
	EXPECT_EQ(k_RealmDistance, 2200.0f);
	EXPECT_EQ(k_TapDistance, 50.0f);
}

TEST(ZoomToPlaces, ASingleTapLooksAtTheLandAheadFromAPleasingView)
{
	ZoomToPlaces keys;
	const auto flight = keys.Act(TempleKey(10000, k_Start, k_Temple, k_Centre, 100.0f), FlatLand);
	ASSERT_TRUE(flight.has_value());
	EXPECT_FALSE(flight->toPlace);
	EXPECT_EQ(flight->rise, k_TapRise);
	// 100 metres along the line of sight, over the land
	const auto ahead = AlongView(k_Start, 100.0f);
	EXPECT_NEAR(flight->view.focus.x, ahead.x, k_Tolerance);
	EXPECT_NEAR(flight->view.focus.z, ahead.z, k_Tolerance);
	EXPECT_NEAR(flight->view.focus.y, 10.0f + k_FocusAboveGround, k_Tolerance);
	EXPECT_NEAR(glm::distance(flight->view.origin, flight->view.focus), k_TapDistance, k_Tolerance);
	EXPECT_FALSE(keys.GetState().flownTo);
	EXPECT_EQ(keys.GetState().lastTap, 10000u);
}

TEST(ZoomToPlaces, ASingleTapWithAFightInViewStartsNoFlightButCounts)
{
	ZoomToPlaces keys;
	EXPECT_FALSE(keys.Act(TempleKey(10000, k_Start, k_Temple, k_Centre, std::nullopt), FlatLand).has_value());
	EXPECT_FALSE(keys.IsSingleTap(10500));
}

TEST(ZoomToPlaces, ADoubleTapFliesToTheTempleAndAnotherFliesBack)
{
	ZoomToPlaces keys;
	std::ignore = keys.Act(TempleKey(10000, k_Start, k_Temple, k_Centre, 100.0f), FlatLand);
	const auto there = keys.Act(TempleKey(10500, k_Start, k_Temple, k_Centre, 100.0f), FlatLand);
	ASSERT_TRUE(there.has_value());
	EXPECT_TRUE(there->toPlace);
	EXPECT_EQ(there->rise, k_PlaceRise);
	EXPECT_EQ(there->view.focus, glm::vec3(k_Temple.x, 10.0f + k_FocusAboveGround, k_Temple.y));
	EXPECT_NEAR(glm::distance(there->view.origin, there->view.focus), k_TempleDistance, k_Tolerance);
	EXPECT_TRUE(keys.GetState().flownTo);
	EXPECT_EQ(keys.GetState().lastTap, 0u);

	// Still looking at the temple: a single tap first, then the double tap flies back to the first view
	std::ignore = keys.Act(TempleKey(20000, there->view, k_Temple, k_Centre, 100.0f), FlatLand);
	const auto back = keys.Act(TempleKey(20100, there->view, k_Temple, k_Centre, 100.0f), FlatLand);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->view.origin, k_Start.origin);
	EXPECT_EQ(back->view.focus, k_Start.focus);
	EXPECT_FALSE(keys.GetState().flownTo);
}

TEST(ZoomToPlaces, LookingAwayFromThePlaceFliesThereAgain)
{
	ZoomToPlaces keys(ZoomToPlaces::State {.lastTap = 19900, .flownTo = true, .returnTo = k_Start});
	// The second tap of a double tap, while the camera looks more than 100 metres from the temple
	const auto flight = keys.Act(TempleKey(20000, k_Start, k_Temple, k_Centre, 100.0f), FlatLand);
	ASSERT_TRUE(flight.has_value());
	EXPECT_EQ(flight->view.focus.x, k_Temple.x);
	EXPECT_TRUE(keys.GetState().flownTo);
}

TEST(ZoomToPlaces, TheRealmKeyHasNoSingleTapAndEndsTheTaps)
{
	ZoomToPlaces keys;
	std::ignore = keys.Act(TempleKey(10000, k_Start, k_Temple, k_Centre, 100.0f), FlatLand);
	const auto flight = keys.Act(RealmKey(10200, k_Start, k_Centre), FlatLand);
	ASSERT_TRUE(flight.has_value());
	EXPECT_TRUE(flight->toPlace);
	EXPECT_EQ(flight->view.focus.x, k_Centre.x);
	EXPECT_NEAR(glm::distance(flight->view.origin, flight->view.focus), k_RealmDistance, 0.01f);
	EXPECT_TRUE(keys.IsSingleTap(10300));
}

TEST(ZoomToPlaces, WithoutATempleTheDoubleTapGoesOverTheRealm)
{
	ZoomToPlaces keys;
	std::ignore = keys.Act(TempleKey(10000, k_Start, std::nullopt, k_Centre, 100.0f), FlatLand);
	const auto flight = keys.Act(TempleKey(10100, k_Start, std::nullopt, k_Centre, 100.0f), FlatLand);
	ASSERT_TRUE(flight.has_value());
	EXPECT_EQ(flight->view.focus.x, k_Centre.x);
	EXPECT_NEAR(glm::distance(flight->view.origin, flight->view.focus), k_RealmDistance, 0.01f);
}

TEST(ZoomToPlaces, TheIslandCentreIsHalfWayBetweenTheFirstAndLastBlocks)
{
	EXPECT_EQ(IslandCentre({3, 5}, {28, 26}), glm::vec2(2480.0f, 2480.0f));
	EXPECT_EQ(IslandCentre({0, 0}, {31, 31}), glm::vec2(2480.0f, 2480.0f));
	EXPECT_EQ(IslandCentre({4, 2}, {9, 30}), glm::vec2(1040.0f, 2560.0f));
}

TEST(ZoomToPlaces, TheMapPositionMultipliesByATenthAndTruncates)
{
	EXPECT_EQ(MapCoordOf(10.0f), 65536);
	EXPECT_EQ(MapCoordOf(0.0f), 0);
	EXPECT_EQ(MapCoordOf(2560.0f), 16777216);
}

TEST(ZoomToPlaces, ACameraThatLooksAtItselfHasNoLineOfSight)
{
	const CameraView still {.origin = {5.0f, 6.0f, 7.0f}, .focus = {5.0f, 6.0f, 7.0f}};
	EXPECT_EQ(AlongView(still, 50.0f), still.origin);
}
