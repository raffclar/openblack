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
