/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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

std::unique_ptr<Camera> MakeCamera(float nearClip, float farClip)
{
	auto camera = std::make_unique<Camera>();
	camera->SetProjectionMatrixPerspective(70.0f, 16.0f / 9.0f, nearClip, farClip);
	return camera;
}

} // namespace

// The game clips what is drawn at the camera's near plane and nowhere nearer to the camera
TEST(CameraProjection, ClipsAtTheNearPlane)
{
	for (const float nearClip : {0.1f, 0.3f, 1.0f, 3.5f})
	{
		const auto camera = MakeCamera(nearClip, 30000.0f);
		EXPECT_FALSE(KeptByDepth(Clip(*camera, nearClip * 0.99f))) << nearClip;
		EXPECT_TRUE(KeptByDepth(Clip(*camera, nearClip * 1.01f))) << nearClip;
		EXPECT_TRUE(KeptByDepth(Clip(*camera, nearClip * 1.9f))) << nearClip;
	}
}

// Depth is reversed: the near plane at 1 and the far plane at 0, nearer things deeper
TEST(CameraProjection, ReversedDepthRunsFromNearToFar)
{
	const auto camera = MakeCamera(3.5f, 30000.0f);
	const auto atNear = Clip(*camera, 3.5f);
	const auto atFar = Clip(*camera, 30000.0f);
	EXPECT_NEAR(atNear.z / atNear.w, 1.0f, 1e-5f);
	EXPECT_NEAR(atFar.z / atFar.w, 0.0f, 1e-5f);
	const auto closer = Clip(*camera, 6.0f);
	const auto further = Clip(*camera, 9.0f);
	EXPECT_GT(closer.z / closer.w, further.z / further.w);
}

// A projection handed over whole is reversed the same way
TEST(CameraProjection, AGivenProjectionClipsAtItsNearPlane)
{
	const auto source = MakeCamera(3.5f, 30000.0f);
	Camera camera;
	camera.SetProjectionMatrix(source->GetProjectionMatrix(Camera::Projection::Normal));
	EXPECT_FALSE(KeptByDepth(Clip(camera, 3.4f)));
	EXPECT_TRUE(KeptByDepth(Clip(camera, 3.6f)));
}
