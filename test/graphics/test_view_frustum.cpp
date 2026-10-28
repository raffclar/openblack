/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Graphics/ViewFrustum.h"

using namespace openblack::graphics;

namespace
{

/// A camera 10 up at the origin looking along +z, a 90 degree view either way across a square screen
view_frustum::Frustum Camera(bool reversedDepth)
{
	const auto view = glm::lookAt(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 1000.0f);
	if (reversedDepth)
	{
		// Depth from 1 at the near plane to 0 at the far one: the sides stay as they are
		projection =
		    glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f) *
		    projection;
	}
	return view_frustum::FromViewProjection(projection * view);
}

} // namespace

TEST(ViewFrustum, ASphereIsSeenInFrontAndWithinTheSides)
{
	for (const bool reversed : {false, true})
	{
		const auto camera = Camera(reversed);
		EXPECT_TRUE(view_frustum::SeesSphere(camera, {0.0f, 10.0f, 50.0f}, 1.0f));
		// Just inside the right side, which runs at 45 degrees
		EXPECT_TRUE(view_frustum::SeesSphere(camera, {49.0f, 10.0f, 50.0f}, 0.1f));
		// Behind the camera
		EXPECT_FALSE(view_frustum::SeesSphere(camera, {0.0f, 10.0f, -50.0f}, 1.0f));
		// Off to the side, and above
		EXPECT_FALSE(view_frustum::SeesSphere(camera, {60.0f, 10.0f, 50.0f}, 1.0f));
		EXPECT_FALSE(view_frustum::SeesSphere(camera, {0.0f, 70.0f, 50.0f}, 1.0f));
	}
}

TEST(ViewFrustum, ASphereOutsideIsSeenByWhatOfItCrossesASide)
{
	const auto camera = Camera(false);
	// Its centre 3 across from a side, which is 3 over the square root of 2 square to it
	const glm::vec3 centre {53.0f, 10.0f, 50.0f};
	const float out = 3.0f / glm::sqrt(2.0f);
	EXPECT_FALSE(view_frustum::SeesSphere(camera, centre, out - 0.01f));
	EXPECT_TRUE(view_frustum::SeesSphere(camera, centre, out + 0.01f));
}

TEST(ViewFrustum, TheSeaReflectsWhatIsAboveIt)
{
	// Looking down at 45 degrees from 10 up, the view runs from straight ahead to straight down
	const auto view = glm::lookAt(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto camera = view_frustum::FromViewProjection(glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 1000.0f) * view);
	// Above the camera, out of view, but its reflection, as far under the sea, is in it
	EXPECT_FALSE(view_frustum::SeesSphere(camera, {0.0f, 20.0f, 5.0f}, 1.0f));
	EXPECT_TRUE(view_frustum::SeesSphereOrReflection(camera, {0.0f, 20.0f, 5.0f}, 1.0f));
	// Behind the camera, and so is its reflection
	EXPECT_FALSE(view_frustum::SeesSphereOrReflection(camera, {0.0f, 20.0f, -30.0f}, 1.0f));
}

TEST(ViewFrustum, ABoxIsSeenUnlessAllItsCornersAreOutsideOneSide)
{
	const auto camera = Camera(false);
	const glm::vec3 minima {-1.0f, -1.0f, -1.0f};
	const glm::vec3 maxima {1.0f, 1.0f, 1.0f};
	EXPECT_TRUE(view_frustum::SeesBox(camera, glm::translate(glm::mat4(1.0f), {0.0f, 10.0f, 50.0f}), minima, maxima));
	EXPECT_FALSE(view_frustum::SeesBox(camera, glm::translate(glm::mat4(1.0f), {0.0f, 10.0f, -50.0f}), minima, maxima));
	EXPECT_FALSE(view_frustum::SeesBox(camera, glm::translate(glm::mat4(1.0f), {70.0f, 10.0f, 50.0f}), minima, maxima));
	// A box round the camera has corners outside each side, but no side has all of them outside
	EXPECT_TRUE(view_frustum::SeesBox(camera, glm::scale(glm::mat4(1.0f), glm::vec3(100.0f)), minima, maxima));
}

TEST(ViewFrustum, ABoxGoesThroughItsModelAndKeepsAMargin)
{
	const auto camera = Camera(false);
	const glm::vec3 minima {0.0f, 0.0f, 0.0f};
	const glm::vec3 maxima {1.0f, 1.0f, 1.0f};
	// Out to the right of the side at 45 degrees, then scaled until it crosses the side
	const auto at = glm::translate(glm::mat4(1.0f), {60.0f, 10.0f, 50.0f});
	EXPECT_FALSE(view_frustum::SeesBox(camera, at, minima, maxima));
	EXPECT_TRUE(view_frustum::SeesBox(camera, at * glm::scale(glm::mat4(1.0f), glm::vec3(-20.0f, 1.0f, 1.0f)), minima, maxima));
	// A corner less than the margin outside the side counts as in view
	const float justOut = 0.5f * view_frustum::k_BoxMargin * glm::sqrt(2.0f);
	EXPECT_TRUE(
	    view_frustum::SeesBox(camera, glm::translate(glm::mat4(1.0f), {51.0f + justOut, 10.0f, 50.0f}), minima, maxima));
	EXPECT_FALSE(view_frustum::SeesBox(camera, glm::translate(glm::mat4(1.0f), {51.0f + (3.0f * justOut), 10.0f, 50.0f}),
	                                   minima, maxima));
}

TEST(ViewFrustum, ASweptBoxIsSeenWhenAnyPlaceAlongItsSweepIsInView)
{
	const auto camera = Camera(false);
	const glm::vec3 minima {-1.0f, -1.0f, -1.0f};
	const glm::vec3 maxima {1.0f, 1.0f, 1.0f};
	// Over the top side, which is 60 high at 50 along the view
	const auto above = glm::translate(glm::mat4(1.0f), {0.0f, 70.0f, 50.0f});
	EXPECT_FALSE(view_frustum::SeesSweptBox(camera, above, minima, maxima, glm::vec3(0.0f)));
	EXPECT_FALSE(view_frustum::SeesSweptBox(camera, above, minima, maxima, {0.0f, 5.0f, 0.0f}));
	// Each end of the sweep counts: down by 15, and up by 15 with the sweep turned over
	EXPECT_TRUE(view_frustum::SeesSweptBox(camera, above, minima, maxima, {0.0f, 15.0f, 0.0f}));
	EXPECT_TRUE(view_frustum::SeesSweptBox(camera, above, minima, maxima, {0.0f, -15.0f, 0.0f}));
	// With no sweep, the test is the same as that of SeesBox
	const auto inFront = glm::translate(glm::mat4(1.0f), {0.0f, 10.0f, 50.0f});
	EXPECT_EQ(view_frustum::SeesSweptBox(camera, inFront, minima, maxima, glm::vec3(0.0f)),
	          view_frustum::SeesBox(camera, inFront, minima, maxima));
}

TEST(ViewFrustum, ALandRiseSweepFollowsTheModelUp)
{
	constexpr float k_Rise = 100.0f;
	// An upright model moves straight up by the rise, whatever its size
	const auto upright = glm::scale(glm::translate(glm::mat4(1.0f), {5.0f, 2.0f, 7.0f}), glm::vec3(3.0f));
	EXPECT_EQ(view_frustum::LandRiseSweep(upright, k_Rise), glm::vec3(0.0f, k_Rise, 0.0f));
	// A tilted model moves along its up by the rise over the length of its first axis
	const auto tilted =
	    glm::scale(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), {0.0f, 0.0f, 1.0f}), glm::vec3(2.0f, 4.0f, 2.0f));
	const auto sweep = view_frustum::LandRiseSweep(tilted, k_Rise);
	ASSERT_TRUE(sweep.has_value());
	EXPECT_NEAR(sweep->x, -2.0f * k_Rise, 0.001f);
	EXPECT_NEAR(sweep->y, 0.0f, 0.001f);
	EXPECT_NEAR(sweep->z, 0.0f, 0.001f);
	// A tilted model of no size has no limit
	auto flat = tilted;
	flat[0] = glm::vec4(0.0f);
	EXPECT_FALSE(view_frustum::LandRiseSweep(flat, k_Rise).has_value());
}

TEST(ViewFrustum, TheHighestLandUnderAModelReadsTheCellsUnderItsBoxAndOneMore)
{
	const glm::vec3 minima {-5.0f, 0.0f, -5.0f};
	const glm::vec3 maxima {5.0f, 30.0f, 5.0f};
	const auto at = glm::translate(glm::mat4(1.0f), {100.0f, 3.0f, 200.0f});
	// A flat land of altitude 2 with one high corner
	const auto landWith = [](glm::ivec2 high) {
		return [high](glm::ivec2 corner) { return corner == high ? uint8_t {100} : uint8_t {2}; };
	};
	// The box covers the corners 9 to 11 across and 19 to 21 along: one more cell on each side, and the far corner of
	// the last cell
	EXPECT_FLOAT_EQ(view_frustum::HighestLandUnder(at, minima, maxima, landWith({12, 22})), 100.0f * 0.67f);
	EXPECT_FLOAT_EQ(view_frustum::HighestLandUnder(at, minima, maxima, landWith({8, 18})), 100.0f * 0.67f);
	EXPECT_FLOAT_EQ(view_frustum::HighestLandUnder(at, minima, maxima, landWith({13, 20})), 2.0f * 0.67f);
	EXPECT_FLOAT_EQ(view_frustum::HighestLandUnder(at, minima, maxima, landWith({10, 17})), 2.0f * 0.67f);
	// A model far off the map reads no corner
	const auto off = glm::translate(glm::mat4(1.0f), {-1.0e9f, 0.0f, 1.0e9f});
	EXPECT_FLOAT_EQ(view_frustum::HighestLandUnder(off, minima, maxima, landWith({0, 511})), 0.0f);
}

TEST(ViewFrustum, TheRadiusOfASquareIsItsFarthestCornerAfterTheModel)
{
	EXPECT_FLOAT_EQ(view_frustum::QuadRadius(glm::mat3(1.0f)), glm::sqrt(2.0f));
	// Scaled 3 across and 4 up: each corner is 5 from the middle, however the square turns
	const auto sized = glm::mat3(glm::scale(glm::mat4(1.0f), {3.0f, 4.0f, 7.0f}));
	EXPECT_FLOAT_EQ(view_frustum::QuadRadius(sized), 5.0f);
	const auto turned = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), {0.0f, 1.0f, 0.0f})) * sized;
	EXPECT_NEAR(view_frustum::QuadRadius(turned), 5.0f, 0.0001f);
	// A skew makes one corner go farther than the other
	glm::mat3 skewed(1.0f);
	skewed[1] = {1.0f, 1.0f, 0.0f};
	EXPECT_FLOAT_EQ(view_frustum::QuadRadius(skewed), glm::sqrt(5.0f));
}
