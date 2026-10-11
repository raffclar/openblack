/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <memory>
#include <optional>
#include <tuple>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_StartOrigin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_StartFocus {1000.0f, 0.0f, 1060.0f};
constexpr glm::vec3 k_FarOrigin {1600.0f, 120.0f, 1400.0f};
constexpr glm::vec3 k_FarFocus {1700.0f, 0.0f, 1500.0f};
// Where the camera is put: looking south-west, a little down
constexpr glm::vec3 k_PlacedOrigin {2430.6f, 68.6f, 2574.4f};
constexpr glm::vec3 k_PlacedFocus {2420.4f, 66.0f, 2546.3f};
constexpr auto k_Frame = 16ms;

/// No land under any pixel
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The world camera with nobody touching the controls
class CameraPlace: public testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		_picking = new NoLandPicking();
		_picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(_picking);
		Locator::gameActionSystem::reset<MockAction>(new MockAction());

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f, 65536.0f);
		std::ignore = _camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_StartOrigin, k_StartFocus));
		_camera.SetOrigin(k_StartOrigin).SetFocus(k_StartFocus);
	}

	void TearDown() override
	{
		Locator::gameActionSystem::reset();
		Locator::pickingSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	void RunFrames(int frames)
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			_camera.HandleActions(k_Frame);
			_camera.Update(k_Frame);
		}
	}

	/// The camera stays where it was put, looking the same way, for this many frames
	void ExpectItStaysPlaced(int frames)
	{
		const auto look = glm::normalize(k_PlacedFocus - k_PlacedOrigin);
		for (int frame = 0; frame < frames; ++frame)
		{
			_camera.HandleActions(k_Frame);
			_camera.Update(k_Frame);
			const auto origin = _camera.GetOrigin();
			ASSERT_LT(glm::distance(origin, k_PlacedOrigin), 1e-3f) << "frame " << frame;
			// What it looks at may slide along its line of sight onto the land ahead, but it looks the same way
			ASSERT_GT(glm::dot(glm::normalize(_camera.GetFocus() - origin), look), 0.99999f) << "frame " << frame;
		}
	}

	Camera _camera;
	NoLandPicking* _picking = nullptr;
};
} // namespace

// A flight under way when the camera is put somewhere is forgotten: it used to carry the camera off after a few frames
TEST_F(CameraPlace, AFlightUnderWayDoesntCarryThePlacedCameraOff)
{
	_camera.GetModel().SetFlight(k_FarOrigin, k_FarFocus);
	RunFrames(3);
	_camera.Place(k_PlacedOrigin, k_PlacedFocus);
	EXPECT_LT(glm::distance(_camera.GetOrigin(), k_PlacedOrigin), 1e-3f);
	ExpectItStaysPlaced(100);
}

// The camera easing towards somewhere else stops there, with nothing of its speed left
TEST_F(CameraPlace, EasingTowardsElsewhereStopsWhereItIsPut)
{
	_camera.SetOriginInterpolator(k_StartOrigin, k_FarOrigin, glm::vec3(300.0f, 0.0f, 300.0f), glm::vec3(0.0f));
	_camera.SetFocusInterpolator(k_StartFocus, k_FarFocus, glm::vec3(300.0f, 0.0f, 300.0f), glm::vec3(0.0f));
	_camera.SetInterpolatorDuration(2s);
	_camera.SetInterpolatorTime(500ms);
	_camera.Place(k_PlacedOrigin, k_PlacedFocus);
	EXPECT_EQ(_camera.GetOriginVelocity(), glm::vec3(0.0f));
	ExpectItStaysPlaced(100);
}

// Put there, the world camera's targets are the new place at once
TEST_F(CameraPlace, TheWorldCamerasTargetsAreThePlace)
{
	_camera.Place(k_PlacedOrigin, k_PlacedFocus);
	EXPECT_EQ(_camera.GetModel().GetTargetOrigin(), k_PlacedOrigin);
	EXPECT_EQ(_camera.GetModel().GetTargetFocus(), k_PlacedFocus);
}
