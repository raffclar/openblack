/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <fstream>
#include <iostream>

#include <Camera/Camera.h>
#include <ECS/Registry.h>
#include <Locator.h>
#include <gtest/gtest.h>
#include <json_helpers.h>

#include "scenarios/DoubleClickFlyTo.h"
#include "scenarios/DragUpDown.h"
#include "scenarios/MiddleDragRightUp.h"
#include "scenarios/MoveBackwardForward.h"
#include "scenarios/MoveRightLeft.h"
#include "scenarios/PanRightLeft.h"
#include "scenarios/TiltDownZoomOut.h"
#include "scenarios/TiltUpDown.h"
#include "scenarios/TiltUpPanLeft.h"
#include "scenarios/TwoButtonZoomOutIn.h"
#include "scenarios/ZoomOutIn.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include <Camera/DefaultWorldCameraModel.h>
#include <Common/RandomNumberManagerTesting.h>

#include "scenarios/Mock.h"

using nlohmann::json;
using openblack::ecs::Registry;
using namespace openblack;

void LoadRaycasts(RecordedMockDynamicsSystem& dynamics, const std::filesystem::path& path);

struct TestValues
{
	std::string_view name;
	RecordedMockDynamicsSystem* dynamicsSystem;
	MockAction* actionInterface;
};
// Padding causes valgrind errors https://github.com/google/googletest/issues/3805
static_assert(std::has_unique_object_representations_v<TestValues>);

class TestDefaultCameraModel: public testing::TestWithParam<TestValues>
{
protected:
	void SetUp() override
	{
		const auto testName = GetParam().name;
		const auto testResultsPath = std::filesystem::path(k_ScenarioPath) / (testName.data() + std::string(".json"));
		ASSERT_TRUE(std::filesystem::exists(testResultsPath));
		std::ifstream(testResultsPath) >> _scenario;
		const auto raycastsPath = std::filesystem::path(k_ScenarioPath) / "raycasts" / (testName.data() + std::string(".json"));
		ASSERT_TRUE(std::filesystem::exists(raycastsPath));
		LoadRaycasts(*GetParam().dynamicsSystem, raycastsPath);

		Locator::entitiesRegistry::emplace<Registry>();

		_camera = std::make_unique<Camera>();
		GetParam().dynamicsSystem->camera = _camera.get();

		Locator::rng::emplace<openblack::RandomNumberManagerTesting>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		Locator::dynamicsSystem::reset<MockDynamicsSystem>(GetParam().dynamicsSystem);
		Locator::gameActionSystem::reset<MockAction>(GetParam().actionInterface);

		const auto aspect = Locator::windowing::value().GetAspectRatio();
		constexpr float k_CameraXFov {70.0f};
		constexpr float k_CameraNearClip {1.0f};
		constexpr float k_CameraFarClip {static_cast<float>(0x10000)};
		_camera->SetProjectionMatrixPerspective(k_CameraXFov, aspect, k_CameraNearClip, k_CameraFarClip);
		// Camera has a model, but we want to test in isolation, so we create a model here
		_model = CameraModel::CreateModel(CameraModel::Model::DefaultWorld);
	}

	void TearDown() override
	{
		_model.reset();
		_camera.reset();
		Locator::terrainSystem::reset();
		Locator::windowing::reset();
		Locator::dynamicsSystem::reset();
		Locator::gameActionSystem::reset();
		Locator::rng::reset();
		Locator::entitiesRegistry::reset();
	}

	static void SetModel(DefaultWorldCameraModel& m, const json& json);
	static void ValidateModel(const DefaultWorldCameraModel& m, const json& json, int frameNumber);

	static constexpr std::string_view k_ScenarioPath = TEST_BINARY_DIR "/camera/scenarios";
	json _scenario;
	std::unique_ptr<Camera> _camera;
	std::unique_ptr<CameraModel> _model;
};

/// The scenario's recorded raycasts (raycasts/<name>.json): branches in the order they are tried, each with the
/// screen coordinates it answers and, per frame, the hit point or null for no hit
void LoadRaycasts(RecordedMockDynamicsSystem& dynamics, const std::filesystem::path& path)
{
	json data;
	std::ifstream(path) >> data;
	dynamics.branches.clear();
	for (const auto& b : data["branches"])
	{
		auto& branch = dynamics.branches.emplace_back();
		for (const auto& coord : b["coords"])
		{
			branch.coords.emplace_back(coord[0].get<uint16_t>(), coord[1].get<uint16_t>());
		}
		for (const auto& frame : b["frames"])
		{
			std::optional<glm::vec2> hit;
			if (!frame[1].is_null())
			{
				hit = glm::vec2(frame[1][0].get<float>(), frame[1][1].get<float>());
			}
			branch.frames.emplace(frame[0].get<uint16_t>(), hit);
		}
	}
}

/// A Zoomer as the recording dumps it (current_value first ... non_linear_acceleration = c2, c3, c4 last)
void LoadZoomer(Zoomer& zoomer, const json& j)
{
	zoomer.value = j["current_value"].get<float>();
	zoomer.destination = j["destination"].get<float>();
	zoomer.destinationSpeed = j["destination_speed"].get<float>();
	zoomer.speed = j["current_speed"].get<float>();
	zoomer.time = j["current_time"].get<float>();
	zoomer.duration = j["duration"].get<float>();
	zoomer.startValue = j["start_value"].get<float>();
	zoomer.startSpeed = j["start_speed"].get<float>();
	zoomer.c2 = j["non_linear_acceleration"]["x"].get<float>();
	zoomer.c3 = j["non_linear_acceleration"]["y"].get<float>();
	zoomer.c4 = j["non_linear_acceleration"]["z"].get<float>();
}

void LoadZoomer3d(Zoomer3& zoomer, const json& j)
{
	LoadZoomer(zoomer.axis[0], j["x"]);
	LoadZoomer(zoomer.axis[1], j["y"]);
	LoadZoomer(zoomer.axis[2], j["z"]);
}

void ValidateCamera(const Camera& c, const json& expected, int frameNumber)
{
	const float ep = 5e-2f;
	const auto& expectedOriginZoomer = expected["camera_origin_zoomer"];
	const auto& expectedFocusZoomer = expected["camera_heading_zoomer"];
	const auto interpolatorTime = std::chrono::duration_cast<std::chrono::duration<float>>(c.GetInterpolatorTime()).count();
	ASSERT_NEAR(interpolatorTime, expectedOriginZoomer["x"]["current_time"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(interpolatorTime, expectedOriginZoomer["y"]["current_time"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(interpolatorTime, expectedOriginZoomer["z"]["current_time"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(interpolatorTime, expectedFocusZoomer["x"]["current_time"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(interpolatorTime, expectedFocusZoomer["y"]["current_time"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(interpolatorTime, expectedFocusZoomer["z"]["current_time"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetOrigin(Camera::Interpolation::Start).x, expectedOriginZoomer["x"]["start_value"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetOrigin(Camera::Interpolation::Start).y, expectedOriginZoomer["y"]["start_value"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetOrigin(Camera::Interpolation::Start).z, expectedOriginZoomer["z"]["start_value"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetFocus(Camera::Interpolation::Start).x, expectedFocusZoomer["x"]["start_value"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetFocus(Camera::Interpolation::Start).y, expectedFocusZoomer["y"]["start_value"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetFocus(Camera::Interpolation::Start).z, expectedFocusZoomer["z"]["start_value"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetOrigin(Camera::Interpolation::Target).x, expectedOriginZoomer["x"]["destination"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetOrigin(Camera::Interpolation::Target).y, expectedOriginZoomer["y"]["destination"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetOrigin(Camera::Interpolation::Target).z, expectedOriginZoomer["z"]["destination"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetFocus(Camera::Interpolation::Target).x, expectedFocusZoomer["x"]["destination"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetFocus(Camera::Interpolation::Target).y, expectedFocusZoomer["y"]["destination"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(c.GetFocus(Camera::Interpolation::Target).z, expectedFocusZoomer["z"]["destination"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
}

void TestDefaultCameraModel::SetModel(DefaultWorldCameraModel& m, const json& json)
{
	const auto& expected = json["cameraController"];

	if (expected["screenCentreHit"].get<bool>())
	{
		m._screenSpaceCenterRaycastHit = expected["screenCentreHitPoint"].get<glm::vec3>();
	}
	else
	{
		m._screenSpaceCenterRaycastHit = std::nullopt;
	}

	m._currentOrigin = expected["origin"].get<glm::vec3>();
	m._currentFocus = expected["heading"].get<glm::vec3>();

	m._originFocusDistanceAtInteractionStart = expected["originHeadingDistanceAtInteractionStart"].get<float>();
	m._originToHandPlaneNormal = expected["handHeadingNormalAtInteractionStart"].get<glm::vec3>();
	m._alignmentAtInteractionStart = expected["mouseHitPointDot"].get<float>();
	m._focusDistance = expected["headingDistance"].get<float>();
	m._distanceFromBoundY = expected["verticalDistance"].get<float>();
	m._averageIslandDistance = expected["averageIslandDistance"].get<float>();
	m._originAtClick = expected["originAtClick"].get<glm::vec3>();
	m._focusAtClick = expected["headingAtClick"].get<glm::vec3>();

	m._targetOrigin = expected["fallbackOrigin"].get<glm::vec3>();
	m._targetFocus = expected["fallbackHeading"].get<glm::vec3>();

	if (expected["handHit"].get<bool>())
	{
		m._screenSpaceMouseRaycastHit = expected["mouseHitPoint"].get<glm::vec3>();
	}
	else
	{
		m._screenSpaceMouseRaycastHit = std::nullopt;
	}
	m._screenSpaceMouseRaycastHitAtClick = expected["lastGrabMouseHitPoint"].get<glm::vec3>();

	glm::xz(m._rotateAroundDelta) = glm::xz(json["RotateAroundDelta"].get<glm::vec3>());
	m._keyBoardMoveDelta = json["KeyboardMoveDelta"].get<glm::vec2>();

	const auto handStatus = expected["handStatus"].get<std::string>();
	if (handStatus == "CAMERA_MODE_HAND_STATUS_NORMAL")
	{
		m._modePrev = DefaultWorldCameraModel::Mode::Cartesian;
	}
	else if (handStatus == "CAMERA_MODE_HAND_STATUS_ZOOMING")
	{
		m._modePrev = DefaultWorldCameraModel::Mode::Polar;
	}
	else if (handStatus == "CAMERA_MODE_HAND_STATUS_GRABBING_LAND")
	{
		m._modePrev = DefaultWorldCameraModel::Mode::DraggingLandscape;
	}
	else if (handStatus == "CAMERA_MODE_HAND_STATUS_TILT_ON")
	{
		m._modePrev = DefaultWorldCameraModel::Mode::ArcBall;
	}
	else
	{
		assert(false);
	}
}

void TestDefaultCameraModel::ValidateModel(const DefaultWorldCameraModel& m, const json& json, int frameNumber)
{
	const float ep = 5e-2f;
	const auto& expected = json["cameraController"];

	ASSERT_EQ(m._screenSpaceCenterRaycastHit.has_value(), expected["screenCentreHit"].get<bool>())
	    << "at start of frame " << frameNumber;
	if (m._screenSpaceCenterRaycastHit.has_value())
	{
		ASSERT_EQ(m._screenSpaceCenterRaycastHit, expected["screenCentreHitPoint"].get<glm::vec3>())
		    << "at start of frame " << frameNumber;
	}

	ASSERT_NEAR(m._originFocusDistanceAtInteractionStart, expected["originHeadingDistanceAtInteractionStart"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._focusDistance, expected["headingDistance"].get<float>(), ep) << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._distanceFromBoundY, expected["verticalDistance"].get<float>(), ep) << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._averageIslandDistance, expected["averageIslandDistance"].get<float>(), ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._focusAtClick.x, expected["headingAtClick"].get<glm::vec3>().x, ep) << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._focusAtClick.y, expected["headingAtClick"].get<glm::vec3>().y, ep) << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._focusAtClick.z, expected["headingAtClick"].get<glm::vec3>().z, ep) << "at start of frame " << frameNumber;

	ASSERT_NEAR(m._mouseAtClick.x, expected["mousePosPrevious"].get<glm::u16vec2>().x, ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m._mouseAtClick.y, expected["mousePosPrevious"].get<glm::u16vec2>().y, ep)
	    << "at start of frame " << frameNumber;

	ASSERT_NEAR(m.GetTargetOrigin().x, expected["fallbackOrigin"].get<glm::vec3>().x, ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m.GetTargetOrigin().y, expected["fallbackOrigin"].get<glm::vec3>().y, ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m.GetTargetOrigin().z, expected["fallbackOrigin"].get<glm::vec3>().z, ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m.GetTargetFocus().x, expected["fallbackHeading"].get<glm::vec3>().x, ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m.GetTargetFocus().y, expected["fallbackHeading"].get<glm::vec3>().y, ep)
	    << "at start of frame " << frameNumber;
	ASSERT_NEAR(m.GetTargetFocus().z, expected["fallbackHeading"].get<glm::vec3>().z, ep)
	    << "at start of frame " << frameNumber;

	// Frame 1 should have 15.8141842, -13.4212151 but current raycasting doesn't find anything.
	// This will have to be investigated and reversed if the mouse mock pos contributes greatly
	ASSERT_EQ(m._screenSpaceMouseRaycastHit.has_value(), expected["handHit"].get<bool>())
	    << "at start of frame " << frameNumber;
	if (m._screenSpaceMouseRaycastHit.has_value())
	{
		ASSERT_EQ(m._screenSpaceMouseRaycastHit, expected["mouseHitPoint"].get<glm::vec3>())
		    << "at start of frame " << frameNumber;
	}

	if (!m._flightPath.has_value())
	{
		const auto handStatus = expected["handStatus"].get<std::string>();
		if (handStatus == "CAMERA_MODE_HAND_STATUS_NORMAL")
		{
			ASSERT_EQ(m._modePrev, DefaultWorldCameraModel::Mode::Cartesian) << "at start of frame " << frameNumber;
		}
		else if (handStatus == "CAMERA_MODE_HAND_STATUS_ZOOMING")
		{
			ASSERT_EQ(m._modePrev, DefaultWorldCameraModel::Mode::Polar) << "at start of frame " << frameNumber;
		}
		else if (handStatus == "CAMERA_MODE_HAND_STATUS_GRABBING_LAND")
		{
			ASSERT_EQ(m._modePrev, DefaultWorldCameraModel::Mode::DraggingLandscape) << "at start of frame " << frameNumber;
		}
		else if (handStatus == "CAMERA_MODE_HAND_STATUS_TILT_ON")
		{
			ASSERT_EQ(m._modePrev, DefaultWorldCameraModel::Mode::ArcBall) << "at start of frame " << frameNumber;
		}
		else
		{
			ASSERT_FALSE(true) << "at start of frame " << frameNumber;
		}
	}

	ASSERT_EQ(m._flightPath.has_value(), expected["setZoomerToFlyToTargetFocus"].get<bool>())
	    << "at start of frame " << frameNumber;
	if (m._flightPath.has_value())
	{
		ASSERT_EQ(m._flightPath->midpoint.has_value(), expected["setZoomerToFlyToHalfWay"].get<bool>())
		    << "at start of frame " << frameNumber;
		ASSERT_NEAR(m._flightPath->origin.x, expected["flyToTargetOrigin"].get<glm::vec3>().x, ep)
		    << "at start of frame " << frameNumber;
		ASSERT_NEAR(m._flightPath->origin.y, expected["flyToTargetOrigin"].get<glm::vec3>().y, ep)
		    << "at start of frame " << frameNumber;
		ASSERT_NEAR(m._flightPath->origin.z, expected["flyToTargetOrigin"].get<glm::vec3>().z, ep)
		    << "at start of frame " << frameNumber;
		ASSERT_NEAR(m._flightPath->focus.x, expected["flyToTargetFocus"].get<glm::vec3>().x, ep)
		    << "at start of frame " << frameNumber;
		ASSERT_NEAR(m._flightPath->focus.y, expected["flyToTargetFocus"].get<glm::vec3>().y, ep)
		    << "at start of frame " << frameNumber;
		ASSERT_NEAR(m._flightPath->focus.z, expected["flyToTargetFocus"].get<glm::vec3>().z, ep)
		    << "at start of frame " << frameNumber;
		if (m._flightPath->midpoint.has_value())
		{
			ASSERT_NEAR(m._flightPath->midpoint->x, expected["setZoomerToFlyToHalfWay"].get<glm::vec3>().x, ep)
			    << "at start of frame " << frameNumber;
			ASSERT_NEAR(m._flightPath->midpoint->y, expected["setZoomerToFlyToHalfWay"].get<glm::vec3>().y, ep)
			    << "at start of frame " << frameNumber;
			ASSERT_NEAR(m._flightPath->midpoint->z, expected["setZoomerToFlyToHalfWay"].get<glm::vec3>().z, ep)
			    << "at start of frame " << frameNumber;
		}
	}
}

TEST_P(TestDefaultCameraModel, ValidateRecordedData)
{
	using namespace std::chrono_literals;

	for (int i = 0; i < _scenario["frames"].size() - 1; ++i)
	{
		const auto& framePrev = _scenario["frames"][i];
		const auto& framePost = _scenario["frames"][i + 1];
		ASSERT_EQ(framePrev["frame"], i);
		ASSERT_EQ(framePost["frame"], i + 1);

		SetModel(reinterpret_cast<DefaultWorldCameraModel&>(*_model), framePrev);

		// the original's zoomers as they were at the start of the frame (the camera's origin and focus zoomers)
		LoadZoomer3d(_camera->GetOriginZoomer(), framePrev["camera"]["camera_origin_zoomer"]);
		LoadZoomer3d(_camera->GetFocusZoomer(), framePrev["camera"]["camera_heading_zoomer"]);

		GetParam().dynamicsSystem->frameNumber = i;
		GetParam().actionInterface->frameNumber = i;

		const auto deltaTimePrev = std::chrono::milliseconds(framePrev["g_delta_time"].get<int>());
		const auto updateInfo = _model->Update(deltaTimePrev, *_camera);
		_model->HandleActions(deltaTimePrev);
		// the camera's update: the frame's ms times 0.001
		_camera->UpdateZoomers(updateInfo, static_cast<float>(deltaTimePrev.count()) * 0.001f);

		ValidateModel(reinterpret_cast<DefaultWorldCameraModel&>(*_model), framePost, i + 1);
		ValidateCamera(*_camera, framePost["camera"], i + 1);
	}
}

TEST(TestCameraZoomers, ZoomerMatchesRecording)
{
	// Every zoomer state recorded from the original, bit for bit: its coefficients are those of
	// SetDestinationWithSpeedAndTime (solved with Inverse) from its start value and speed to its
	// destination, and its value and speed those of Update at its time. (Not a TEST_P: the fixture owns its
	// mocks once per scenario)
	size_t curves = 0;
	size_t values = 0;
	size_t scenarios = 0;
	for (const auto& entry : std::filesystem::directory_iterator(TEST_BINARY_DIR "/camera/scenarios"))
	{
		if (entry.path().extension() != ".json")
		{
			continue;
		}
		++scenarios;
		json scenario;
		std::ifstream(entry.path()) >> scenario;
		for (const auto& frame : scenario["frames"])
		{
			for (const char* name : {"camera_origin_zoomer", "camera_heading_zoomer"})
			{
				for (const char* axis : {"x", "y", "z"})
				{
					Zoomer recorded;
					LoadZoomer(recorded, frame["camera"][name][axis]);
					if (!(recorded.duration >= 0.001f))
					{
						continue;
					}
					Zoomer zoomer;
					zoomer.value = recorded.startValue;
					zoomer.speed = recorded.startSpeed;
					zoomer.SetDestinationWithSpeedAndTime(recorded.destination, recorded.destinationSpeed, recorded.duration);
					ASSERT_EQ(zoomer.c2, recorded.c2) << "frame " << frame["frame"] << " " << name << "." << axis;
					ASSERT_EQ(zoomer.c3, recorded.c3) << "frame " << frame["frame"] << " " << name << "." << axis;
					ASSERT_EQ(zoomer.c4, recorded.c4) << "frame " << frame["frame"] << " " << name << "." << axis;
					++curves;
					if (recorded.time > 0.0f && recorded.time < recorded.duration)
					{
						zoomer.Update(recorded.time);
						ASSERT_EQ(zoomer.value, recorded.value) << "frame " << frame["frame"] << " " << name << "." << axis;
						ASSERT_EQ(zoomer.speed, recorded.speed) << "frame " << frame["frame"] << " " << name << "." << axis;
						++values;
					}
				}
			}
		}
	}
	std::cout << curves << " curves, " << values << " values" << std::endl;
	EXPECT_EQ(scenarios, 11u);
	EXPECT_GT(curves, 0u);
	EXPECT_GT(values, 0u);
}

#define SCENARIO_VALUES(name)                                       \
	TestValues                                                      \
	{                                                               \
		#name, new RecordedMockDynamicsSystem, new name##MockAction \
	}

const auto k_TestingScenarioValues = testing::Values( //
    SCENARIO_VALUES(MoveBackwardForward),             //
    SCENARIO_VALUES(MoveRightLeft),                   //
    SCENARIO_VALUES(PanRightLeft),                    //
    SCENARIO_VALUES(TiltUpDown),                      //
    SCENARIO_VALUES(ZoomOutIn),                       //
    SCENARIO_VALUES(TiltDownZoomOut),                 //
    SCENARIO_VALUES(TiltUpPanLeft),                   //
    SCENARIO_VALUES(DragUpDown),                      //
    SCENARIO_VALUES(DoubleClickFlyTo),                //
    SCENARIO_VALUES(TwoButtonZoomOutIn),              //
    SCENARIO_VALUES(MiddleDragRightUp)                //
);

INSTANTIATE_TEST_SUITE_P(TestScenarioInstantiation, TestDefaultCameraModel, k_TestingScenarioValues,
                         [](const testing::TestParamInfo<TestDefaultCameraModel::ParamType>& info) {
	                         return info.param.name.data();
                         });
