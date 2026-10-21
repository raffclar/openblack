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
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <ECS/Components/Creature.h>
#include <ECS/Components/Transform.h>
#include <ECS/Registry.h>
#include <ECS/Systems/Implementations/CreatureModeSystem.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
/// The creature stands far from where the camera looks
constexpr glm::vec3 k_CreatureAt {1500.0f, 0.0f, 1000.0f};
constexpr auto k_Frame = 20ms;

/// No land under the cursor
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

class CreatureModeHandBack: public testing::Test
{
protected:
	void SetUp() override
	{
		// The camera reports its flights to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
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
		_creature = registry.Create();
		registry.Assign<ecs::components::Creature>(_creature);
		registry.Assign<ecs::components::Transform>(_creature, k_CreatureAt, glm::mat3(1.0f), glm::vec3(1.0f));
	}

	void TearDown() override
	{
		Locator::camera::reset();
		Locator::pickingSystem::reset();
		Locator::gameActionSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	/// Frames of the game: the creature mode, then the camera
	void RunFrames(int frames, bool gripping = false)
	{
		auto& camera = Locator::camera::value();
		for (int frame = 0; frame < frames; ++frame)
		{
			_mode.Update(k_Frame, {.handGripping = gripping});
			camera.Update(k_Frame);
		}
	}

	ecs::systems::CreatureModeSystem _mode;
	entt::entity _creature {entt::null};
};
} // namespace

TEST_F(CreatureModeHandBack, WhenTheCreatureGoesThePlayersCameraFliesBack)
{
	ASSERT_TRUE(_mode.Enter(_creature));
	RunFrames(300);
	const auto& camera = Locator::camera::value();
	ASSERT_LT(glm::distance(glm::vec2(camera.GetFocus().x, camera.GetFocus().z), glm::vec2(k_CreatureAt.x, k_CreatureAt.z)),
	          50.0f);
	// The creature goes: the follow camera is no longer valid, and the player's camera flies back to where it was going
	Locator::entitiesRegistry::value().Destroy(_creature);
	RunFrames(300);
	EXPECT_FALSE(_mode.IsActive());
	EXPECT_LT(glm::distance(camera.GetOrigin(), k_Origin), 1.0f);
}

TEST_F(CreatureModeHandBack, AGripOfTheLandStartsThePlayersCameraWhereItIs)
{
	ASSERT_TRUE(_mode.Enter(_creature));
	RunFrames(300);
	const auto& camera = Locator::camera::value();
	// The button of the double click is let go, then a fresh grip gives the camera back where it is
	RunFrames(1, false);
	const auto left = camera.GetOrigin();
	RunFrames(1, true);
	EXPECT_FALSE(_mode.IsActive());
	EXPECT_GT(glm::distance(camera.GetOrigin(), k_Origin), 100.0f);
	EXPECT_LT(glm::distance(camera.GetOrigin(), left), 50.0f);
}
