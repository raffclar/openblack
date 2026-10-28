/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/ScreenPick.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/ThingOnScreen.h"

using namespace openblack;

namespace
{

constexpr glm::vec2 k_WideScreen {1920.0f, 1080.0f};

std::unique_ptr<Camera> CameraAt(glm::vec3 eye, glm::vec3 focus)
{
	auto camera = std::make_unique<Camera>();
	camera->SetProjectionMatrixPerspective(70.0f, k_WideScreen.x / k_WideScreen.y, 2.0f);
	camera->SetOrigin(eye);
	camera->SetFocus(focus);
	return camera;
}

ecs::thing_on_screen::ViewInputs InputsOf(const Camera& camera)
{
	const auto pose = camera.DrawnPose(Camera::Interpolation::Current);
	return {
	    .eye = pose.eye,
	    .focus = pose.focus,
	    .lens = camera.GetLens(),
	    .nearClip = camera.GetNearClip(),
	    .aspect = camera.GetAspect(),
	    .resolution = k_WideScreen,
	};
}

} // namespace

// The view that the turns make from a view report is the view of the camera that the report came from, to the bit
TEST(ReportedView, TheViewFromTheReportIsTheViewOfTheCamera)
{
	const auto owned = CameraAt({1000.0f, 120.0f, 900.0f}, {1100.0f, 10.0f, 1050.0f});
	const auto& camera = *owned;
	const auto fromCamera = screen_pick::FourByThreeMiddle({
	    .worldToClip = camera.GetViewProjectionMatrix(Camera::Projection::Normal),
	    .resolution = k_WideScreen,
	    .near = camera.GetNearClip(),
	    .xScale = camera.GetProjectionMatrix(Camera::Projection::Normal)[0][0],
	    .camera = glm::vec3(glm::inverse(camera.GetViewMatrix(Camera::Interpolation::Current))[3]),
	});
	const auto fromReport = ecs::thing_on_screen::ViewOf(InputsOf(camera));
	EXPECT_EQ(fromReport.worldToClip, fromCamera.worldToClip);
	EXPECT_EQ(fromReport.resolution, fromCamera.resolution);
	EXPECT_EQ(fromReport.near, fromCamera.near);
	EXPECT_EQ(fromReport.xScale, fromCamera.xScale);
	EXPECT_EQ(fromReport.camera, fromCamera.camera);
}

// A point that the camera looks at is on the screen; a point behind the camera and a point far to the side are not
TEST(ReportedView, OnlyWhatTheReportedCameraSeesIsOnTheScreen)
{
	const glm::vec3 eye {1000.0f, 120.0f, 900.0f};
	const glm::vec3 focus {1100.0f, 10.0f, 1050.0f};
	const auto view = ecs::thing_on_screen::ViewOf(InputsOf(*CameraAt(eye, focus)));
	EXPECT_TRUE(screen_pick::PointOnScreen(view, focus));
	EXPECT_FALSE(screen_pick::PointOnScreen(view, eye - (focus - eye)));
	// Across the line of sight, more than the 4:3 middle's half width away at that depth
	const glm::vec3 across {-(focus - eye).z, 0.0f, (focus - eye).x};
	EXPECT_FALSE(screen_pick::PointOnScreen(view, focus + across * 2.0f));
}

// The camera has arrived only when a report after the last camera order of the scripts says so
TEST(ReportedView, TheCameraArrivesByAReportAfterTheLastOrder)
{
	EXPECT_TRUE(script_camera::ArrivedByReport(true, 4, 4));
	// The report came before the last order: the camera may not have moved yet
	EXPECT_FALSE(script_camera::ArrivedByReport(true, 3, 4));
	EXPECT_FALSE(script_camera::ArrivedByReport(false, 4, 4));
}

// The player's camera has arrived once its eye and focus are where they go
TEST(ReportedView, ThePlayersCameraArrivesAtItsTarget)
{
	const glm::vec3 eye {10.0f, 20.0f, 30.0f};
	const glm::vec3 focus {40.0f, 0.0f, 60.0f};
	EXPECT_TRUE(script_camera::PlayerCameraArrived(eye, eye, focus, focus));
	EXPECT_TRUE(script_camera::PlayerCameraArrived(eye, eye + glm::vec3(0.01f, 0.0f, 0.0f), focus, focus));
	EXPECT_FALSE(script_camera::PlayerCameraArrived(eye, eye + glm::vec3(0.1f, 0.0f, 0.0f), focus, focus));
	EXPECT_FALSE(script_camera::PlayerCameraArrived(eye, eye, focus, focus + glm::vec3(0.0f, 0.0f, 0.1f)));
}
