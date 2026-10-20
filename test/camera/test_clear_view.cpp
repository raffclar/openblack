/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ClearView.h"
#include "Camera/ScriptCameraModel.h"

using namespace openblack::clear_view;

TEST(ClearView, TheOrbitIsCloseAndCloserForANearCamera)
{
	EXPECT_EQ(OrbitRadius(301.0f), 50.0f);
	EXPECT_EQ(OrbitRadius(300.0f), 15.0f);
	EXPECT_EQ(OrbitRadius(50.0f), 15.0f);
	EXPECT_EQ(OrbitRadius(49.0f), 10.0f);
}

TEST(ClearView, ThePitchGoesAQuarterOfTheWayFromThreeQuartersOfPi)
{
	// The shift is the float of three quarters of pi
	EXPECT_EQ(k_PitchShift, std::numbers::pi_v<float> * 3.0f / 4.0f);
	EXPECT_EQ(OrbitPitch(0.0f), k_PitchShift * 0.25f);
	EXPECT_EQ(OrbitPitch(0.4f), (0.4f + k_PitchShift) * 0.25f);
}

TEST(ClearView, ItStartsFromTheCameraAndEasesToTheOrbitRoundThePoint)
{
	const glm::vec3 origin {100.0f, 80.0f, 100.0f};
	const glm::vec3 focus {100.0f, 0.0f, 160.0f};
	const auto orbit = Start(origin, focus);
	// The camera is 100 from what it looks at
	EXPECT_EQ(orbit.radius, 15.0f);
	const auto look = openblack::script_camera::HeadingAndPitchFromPoints(origin, focus);
	EXPECT_EQ(orbit.heading, look.heading);
	EXPECT_EQ(orbit.pitch, OrbitPitch(look.pitch));

	const glm::vec3 centre {120.0f, 5.0f, 150.0f};
	const auto none = Eased(orbit, centre, 0.0f);
	EXPECT_EQ(none.origin, origin);
	EXPECT_EQ(none.focus, focus);
	const auto all = Eased(orbit, centre, 1.0f);
	EXPECT_EQ(all.focus, centre);
	EXPECT_NEAR(glm::distance(all.origin, centre), 15.0f, 1e-4f);
	const auto half = Eased(orbit, centre, 0.5f);
	EXPECT_EQ(half.focus, (centre - focus) * 0.5f + focus);
}
