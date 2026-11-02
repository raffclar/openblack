/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#define LOCATOR_IMPLEMENTATIONS
#include <3D/CameraPath.h>
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/CameraPathControl.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/ScriptCameraModel.h>
#include <ECS/Registry.h>
#include <ECS/Systems/Implementations/CameraPathSystem.h>
#include <Locator.h>
#include <Particles/CameraPathLink.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
/// The path is placed this far from the camera
constexpr glm::vec3 k_PathPlace {1400.0f, 0.0f, 1000.0f};
constexpr auto k_Frame = 20ms;
/// The animation that the path goes with plays over ten seconds
constexpr int32_t k_CycleMilliseconds = 10000;

/// A path of two points
std::unique_ptr<CameraPath> TwoPointPath()
{
	// The file: its size, its time in milliseconds and its count of points, then each point's place and focus
	const std::array<uint32_t, 3> header {0, 1000, 2};
	const std::array<float, 12> points {0.0f, 20.0f, 0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 20.0f, 0.0f, 10.0f, 0.0f, 10.0f};
	std::vector<uint8_t> data(sizeof(header) + sizeof(points));
	std::memcpy(data.data(), header.data(), sizeof(header));
	std::memcpy(data.data() + sizeof(header), points.data(), sizeof(points));
	auto path = std::make_unique<CameraPath>("two points");
	EXPECT_TRUE(path->LoadFromBuffer(data));
	return path;
}

/// No land under the cursor
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

class CameraPathStart: public testing::Test
{
protected:
	void SetUp() override
	{
		// The camera reports its flights to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		Locator::gameActionSystem::emplace<MockAction>();
		auto& camera = Locator::camera::emplace<Camera>();
		auto* picking = new NoLandPicking();
		picking->camera = &camera;
		Locator::pickingSystem::reset<MockPickingSystem>(picking);
		camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
		std::ignore = camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus));
		camera.SetOrigin(k_Origin).SetFocus(k_Focus);
	}

	void TearDown() override
	{
		Locator::camera::reset();
		Locator::gameActionSystem::reset();
		Locator::pickingSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	void FollowAPath()
	{
		_link = std::make_shared<particles::CameraPathLink>(
		    particles::CameraPathLink {.cycleMilliseconds = k_CycleMilliseconds, .frames = camera_path::k_AnimationFrames});
		_paths.FollowPlaced(TwoPointPath(), glm::translate(glm::mat4(1.0f), k_PathPlace), 1.0f, _link);
	}

	/// The atom's step ends at a frame of its animation, drawn from now on, with the pause gone by or not
	void StepAtom(float frame, bool playing)
	{
		_link->previousFrame = _link->currentFrame;
		_link->currentFrame = frame;
		_link->shown = true;
		_link->playing = playing;
	}

	/// Frames of the game: the path, else the player's camera, moves the camera. Each frame draws the atom's last step.
	void RunFrames(int frames)
	{
		auto& camera = Locator::camera::value();
		for (int frame = 0; frame < frames; ++frame)
		{
			const bool held = _paths.HoldsCamera();
			_paths.Update(k_Frame, k_Frame);
			if (!held)
			{
				camera.HandleActions(k_Frame);
				camera.Update(k_Frame);
				camera.HandleKeys(k_Frame);
			}
			_paths.ShowPlaced(1.0f);
		}
	}

	ecs::systems::CameraPathSystem _paths;
	std::shared_ptr<particles::CameraPathLink> _link;
};
} // namespace

TEST_F(CameraPathStart, AMiraclesPathTakesTheWorldCamera)
{
	FollowAPath();
	EXPECT_TRUE(_paths.HoldsCamera());
}

TEST_F(CameraPathStart, AScriptsCameraKeepsTheCameraFromAMiraclesPath)
{
	std::ignore = Locator::camera::value().SetModel(
	    std::make_unique<ScriptCameraModel>(k_Origin, k_Focus, [](float /*x*/, float /*z*/) { return 0.0f; }));
	FollowAPath();
	EXPECT_FALSE(_paths.HoldsCamera());
}

TEST_F(CameraPathStart, TheGlideStartsInTheNextFrame)
{
	FollowAPath();
	const auto& camera = Locator::camera::value();
	EXPECT_EQ(camera.GetOrigin(), k_Origin);
	RunFrames(1);
	EXPECT_NE(camera.GetOrigin(), k_Origin);
}

TEST_F(CameraPathStart, TheLastFrameEndsThePathAtTheEndOfTheNextTurn)
{
	FollowAPath();
	StepAtom(0.0f, true);
	RunFrames(100);
	const auto& camera = Locator::camera::value();
	ASSERT_LT(glm::distance(camera.GetOrigin(), k_PathPlace), 100.0f);
	// The draw shows the last frame. The path keeps the camera until the turn ends.
	StepAtom(999.0f, true);
	RunFrames(1);
	EXPECT_TRUE(_link->ended);
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.CheckPlaced();
	EXPECT_FALSE(_paths.HoldsCamera());
	// The world camera flies back to where it was going
	RunFrames(300);
	EXPECT_LT(glm::distance(camera.GetOrigin(), k_Origin), 1.0f);
}

TEST_F(CameraPathStart, TheAtomGoingEndsThePathAtTheEndOfTheTurn)
{
	FollowAPath();
	RunFrames(10);
	_link.reset();
	RunFrames(1);
	EXPECT_TRUE(_paths.HoldsCamera());
	_paths.CheckPlaced();
	EXPECT_FALSE(_paths.HoldsCamera());
}

TEST_F(CameraPathStart, ThePathTimeIsThatOfTheDrawnFrame)
{
	FollowAPath();
	StepAtom(100.0f, true);
	StepAtom(200.0f, true);
	_paths.ShowPlaced(0.5f);
	// Frame 150 of a thousand over ten seconds
	EXPECT_EQ(_link->drawnMilliseconds, 1500);
	EXPECT_FALSE(_link->ended);
}

TEST_F(CameraPathStart, AnAtomNotYetDrawnLeavesThePathTime)
{
	FollowAPath();
	_link->currentFrame = 500.0f;
	_paths.ShowPlaced(1.0f);
	EXPECT_EQ(_link->drawnMilliseconds, 0);
}

TEST_F(CameraPathStart, TakingThePathsCameraBackStartsTheWorldCameraWhereItIs)
{
	FollowAPath();
	RunFrames(100);
	const auto& camera = Locator::camera::value();
	const auto left = camera.GetOrigin();
	// A movement key takes the camera back: a new world camera starts from where the camera is
	_paths.HandlePlayerControl({.movementKey = true, .frameMilliseconds = 20, .grippingLand = false});
	EXPECT_FALSE(_paths.HoldsCamera());
	RunFrames(300);
	EXPECT_LT(glm::distance(camera.GetOrigin(), left), 1.0f);
}

TEST_F(CameraPathStart, WhileTheGameIsPausedTheCameraStaysWhereThePathStopped)
{
	FollowAPath();
	StepAtom(10.0f, true);
	StepAtom(20.0f, true);
	RunFrames(70);
	const auto& camera = Locator::camera::value();
	// Paused: no step moves the frame on, so the camera comes to the path's place in a moment and stays there
	RunFrames(50);
	const auto stopped = camera.GetOrigin();
	RunFrames(100);
	EXPECT_LT(glm::distance(camera.GetOrigin(), stopped), 0.01f);
	EXPECT_TRUE(_paths.HoldsCamera());
	// The game goes on, and the path with it
	StepAtom(60.0f, true);
	RunFrames(20);
	EXPECT_GT(glm::distance(camera.GetOrigin(), stopped), 0.1f);
}

// The frames move on at one play of the animation over its cycle time, shortened by the speed-up
TEST(CameraPathTiming, TheFrameRateIsAThousandFramesOverTheSpedUpCycle)
{
	EXPECT_FLOAT_EQ(camera_path::FrameRate(10000, 0.6f), 60.0f);
	EXPECT_EQ(camera_path::FrameRate(1000, 1.0f), 1000.0f);
}

TEST(CameraPathTiming, TheDrawnFrameIsCutOffAndKeptInTheFrames)
{
	EXPECT_EQ(camera_path::DrawnFrame(10.0f, 20.0f, 0.55f, 1000, false), 15);
	EXPECT_EQ(camera_path::DrawnFrame(10.0f, 20.0f, -1.0f, 1000, false), 10);
	EXPECT_EQ(camera_path::DrawnFrame(10.0f, 20.0f, 3.0f, 1000, false), 20);
	EXPECT_EQ(camera_path::DrawnFrame(0.0f, 0.9f, 1.0f, 1000, false), 0);
	EXPECT_EQ(camera_path::DrawnFrame(990.0f, 1020.0f, 1.0f, 1000, false), 999);
	// A looped frame goes round and may run on past the step
	EXPECT_EQ(camera_path::DrawnFrame(990.0f, 1000.0f, 2.0f, 1000, true), 10);
}

TEST(CameraPathTiming, ThePathTimeIsInWholeMilliseconds)
{
	EXPECT_EQ(camera_path::PathMilliseconds(10000, 1), 10);
	EXPECT_EQ(camera_path::PathMilliseconds(1234, 999), 1232);
	EXPECT_TRUE(camera_path::EndsPath(999, 1000));
	EXPECT_FALSE(camera_path::EndsPath(998, 1000));
}
