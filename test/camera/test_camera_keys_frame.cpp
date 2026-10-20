/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>
#include <memory>
#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/FightCameraModel.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
constexpr auto k_Frame = 16ms;

/// No land under the cursor
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The key that turns the camera right, held or not
class TurnKey final: public MockAction
{
public:
	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const override
	{
		// A question about several keys asks whether any of them is held
		return held && (static_cast<uint32_t>(action) & static_cast<uint32_t>(input::BindableActionMap::ROTATE_RIGHT)) != 0;
	}

	bool held {false};
};

class CameraKeysFrame: public testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		Locator::pickingSystem::reset<MockPickingSystem>(new NoLandPicking());
		_key = new TurnKey();
		Locator::gameActionSystem::reset<MockAction>(_key);
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

	TurnKey* _key = nullptr;
};
} // namespace

TEST_F(CameraKeysFrame, TheWorldCameraTurnsByAKeyInTheNextFrame)
{
	DefaultWorldCameraModel world(k_Origin, k_Focus);
	_key->held = true;
	// The key is held in this frame: the camera reads the mouse before the key handler has run, and asks for no turn
	world.HandleActions(k_Frame);
	EXPECT_EQ(world.GetAsked().turn, 0.0f);
	// After the step of the camera, the key handler reads the key. The next frame asks for the turn, also when the key
	// is up by then.
	world.HandleKeys(k_Frame);
	_key->held = false;
	world.HandleActions(k_Frame);
	EXPECT_GT(world.GetAsked().turn, 0.0f);
	// The key that was let go turns nothing in the frame after
	world.HandleKeys(k_Frame);
	world.HandleActions(k_Frame);
	EXPECT_EQ(world.GetAsked().turn, 0.0f);
}

TEST_F(CameraKeysFrame, TheFightViewGivesTheKeysToThePlayersCamera)
{
	FightCameraModel fight(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus),
	                       {.origin = k_Origin, .focus = k_Focus});
	_key->held = true;
	fight.HandleActions(k_Frame);
	fight.HandleKeys(k_Frame);
	fight.HandleActions(k_Frame);
	auto player = fight.TakePlayerModel();
	ASSERT_NE(player, nullptr);
	EXPECT_GT(player->GetAsked().turn, 0.0f);
}
