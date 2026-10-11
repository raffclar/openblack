/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <memory>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"

using openblack::Camera;

namespace
{

/// A point straight ahead of the camera at a depth along its view, in the view's space, which looks down positive z
glm::vec4 Clip(const Camera& camera, float depth)
{
	return camera.GetProjectionMatrix(Camera::Projection::ReversedZ) * glm::vec4(0.0f, 0.0f, depth, 1.0f);
}

/// Inside the depth range the renderer keeps: 0 to w
bool KeptByDepth(const glm::vec4& clip)
{
	return clip.z >= 0.0f && clip.z <= clip.w;
}

std::unique_ptr<Camera> MakeCamera(float nearClip)
{
	auto camera = std::make_unique<Camera>();
	camera->SetProjectionMatrixPerspective(70.0f, 16.0f / 9.0f, nearClip);
	return camera;
}

} // namespace

// The game clips what is drawn at the camera's near plane and nowhere nearer to the camera
TEST(CameraProjection, ClipsAtTheNearPlane)
{
	for (const float nearClip : {0.1f, 0.3f, 1.0f, 3.5f})
	{
		const auto camera = MakeCamera(nearClip);
		EXPECT_FALSE(KeptByDepth(Clip(*camera, nearClip * 0.99f))) << nearClip;
		EXPECT_TRUE(KeptByDepth(Clip(*camera, nearClip * 1.01f))) << nearClip;
		EXPECT_TRUE(KeptByDepth(Clip(*camera, nearClip * 1.9f))) << nearClip;
	}
}

// Depth is reversed: the near plane at 1, falling towards 0 far away, nearer things deeper
TEST(CameraProjection, ReversedDepthRunsFromNearToFar)
{
	const auto camera = MakeCamera(3.5f);
	const auto atNear = Clip(*camera, 3.5f);
	const auto atFar = Clip(*camera, 30000.0f);
	EXPECT_NEAR(atNear.z / atNear.w, 1.0f, 1e-5f);
	EXPECT_NEAR(atFar.z / atFar.w, 3.5f / 30000.0f, 1e-6f);
	const auto closer = Clip(*camera, 6.0f);
	const auto further = Clip(*camera, 9.0f);
	EXPECT_GT(closer.z / closer.w, further.z / further.w);
}

// The game has no far plane: the sky's dome across the map and the sea to its horizon are drawn from as high as the
// camera goes, and the depth is the near plane's distance over the depth, as the game's
TEST(CameraProjection, NothingIsLostFarAway)
{
	for (const float nearClip : {0.1f, 1.0f, 3.5f})
	{
		const auto camera = MakeCamera(nearClip);
		for (const float depth : {65536.0f, 80000.0f, 250000.0f, 1.0e7f})
		{
			const auto clip = Clip(*camera, depth);
			EXPECT_TRUE(KeptByDepth(clip)) << nearClip << " " << depth;
			EXPECT_GT(clip.z, 0.0f) << nearClip << " " << depth;
			EXPECT_NEAR(clip.z / clip.w, nearClip / depth, 1e-6f * nearClip / depth + 1e-9f) << nearClip << " " << depth;
		}
	}
}

// A projection handed over whole is reversed the same way
TEST(CameraProjection, AGivenProjectionClipsAtItsNearPlane)
{
	const auto source = MakeCamera(3.5f);
	Camera camera;
	camera.SetProjectionMatrix(source->GetProjectionMatrix(Camera::Projection::Normal));
	EXPECT_FALSE(KeptByDepth(Clip(camera, 3.4f)));
	EXPECT_TRUE(KeptByDepth(Clip(camera, 3.6f)));
}

// A camera shown elsewhere for a frame (the inspector's override) is given its whole movement back, so that it carries
// on from where it was going, not from where it was shown
TEST(CameraProjection, ItsMovementIsGivenBackWhole)
{
	using namespace std::chrono_literals;
	Camera camera;
	camera.SetOriginInterpolator({0.0f, 10.0f, 0.0f}, {100.0f, 10.0f, 0.0f}, glm::vec3(0.0f), glm::vec3(0.0f));
	camera.SetFocusInterpolator({0.0f, 0.0f, 50.0f}, {100.0f, 0.0f, 50.0f}, glm::vec3(0.0f), glm::vec3(0.0f));
	camera.SetInterpolatorDuration(2s);
	camera.SetInterpolatorTime(1s);
	const auto origin = camera.GetOrigin();
	const auto focus = camera.GetFocus();
	const auto own = camera.GetMotion();

	camera.SetOrigin({5.0f, 500.0f, 5.0f}).SetFocus({6.0f, 0.0f, 6.0f});
	EXPECT_EQ(camera.GetOrigin(), glm::vec3(5.0f, 500.0f, 5.0f));

	camera.SetMotion(own);
	EXPECT_EQ(camera.GetOrigin(), origin);
	EXPECT_EQ(camera.GetFocus(), focus);
	EXPECT_EQ(camera.GetInterpolatorTime(), 1s);
	EXPECT_EQ(camera.GetInterpolatorDuration(), 2s);
	// And carries on to where it was going
	camera.SetInterpolatorT(1.0f);
	EXPECT_NEAR(camera.GetOrigin().x, 100.0f, 1e-3f);
}

// A resized window changes the camera's shape only: the field of view across the width and the clip planes stay
TEST(CameraProjection, ANewAspectKeepsTheLensAndTheClipPlanes)
{
	const auto camera = MakeCamera(3.5f);
	const auto across = camera->GetProjectionMatrix(Camera::Projection::Normal)[0][0];
	for (const float aspect : {4.0f / 3.0f, 16.0f / 9.0f, 32.0f / 9.0f})
	{
		camera->SetAspect(aspect);
		const auto projection = camera->GetProjectionMatrix(Camera::Projection::Normal);
		EXPECT_FLOAT_EQ(projection[0][0], across) << aspect;
		EXPECT_FLOAT_EQ(projection[1][1], across * aspect) << aspect;
		EXPECT_FLOAT_EQ(camera->GetNearClip(), 3.5f) << aspect;
		EXPECT_FLOAT_EQ(camera->GetAspect(), aspect) << aspect;
		EXPECT_FALSE(KeptByDepth(Clip(*camera, 3.5f * 0.99f))) << aspect;
		EXPECT_TRUE(KeptByDepth(Clip(*camera, 3.5f * 1.01f))) << aspect;
	}
}
