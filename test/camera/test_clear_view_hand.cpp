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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/ZoomToPlaces.h>
#include <ECS/Components/Transform.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
constexpr glm::vec3 k_Thing {1012.5f, 7.0f, 1047.25f};
constexpr auto k_Frame = 16ms;

/// The cursor picks the thing that the test gives, and no land
class ThingPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }

	void Pick(std::optional<entt::entity> thing) { _pick.object = thing; }
};

/// Ctrl and Shift held, or nothing held
class ClearViewKeys final: public MockAction
{
public:
	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const override
	{
		return held && (action == input::BindableActionMap::ZOOM_ON || action == input::BindableActionMap::ROTATE_ON);
	}

	bool held {true};
};

class ClearViewHand: public testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		_picking = new ThingPicking();
		_picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(_picking);
		_keys = new ClearViewKeys();
		Locator::gameActionSystem::reset<MockAction>(_keys);

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
		std::ignore = _camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus));
		_camera.SetOrigin(k_Origin).SetFocus(k_Focus);

		auto& registry = Locator::entitiesRegistry::value();
		_thing = registry.Create();
		registry.Assign<ecs::components::Transform>(_thing, k_Thing, glm::mat3(1.0f), glm::vec3(1.0f));
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

	/// One frame of the game: the mouse, the step of the camera, then the keys
	void Frame()
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
		_camera.HandleKeys(k_Frame);
	}

	[[nodiscard]] std::optional<glm::vec3> Hand() const
	{
		return dynamic_cast<const DefaultWorldCameraModel&>(_camera.GetModel()).GetClearViewHand();
	}

	Camera _camera;
	ThingPicking* _picking = nullptr;
	ClearViewKeys* _keys = nullptr;
	entt::entity _thing {entt::null};
};
} // namespace

TEST_F(ClearViewHand, TheHandGoesOnTheLandAtTheThingUnderIt)
{
	_picking->Pick(_thing);
	Frame();
	const auto hand = Hand();
	ASSERT_TRUE(hand.has_value());
	// The hand is at the thing across the land, on the land under it
	EXPECT_EQ(hand->x, k_Thing.x);
	EXPECT_EQ(hand->z, k_Thing.z);
	const auto land =
	    Locator::terrainSystem::value().GetAltitude(zoom_to::MapCoordOf(k_Thing.x), zoom_to::MapCoordOf(k_Thing.z));
	EXPECT_EQ(hand->y, land);
}

TEST_F(ClearViewHand, TheHandFollowsTheThingWhileTheKeysAreHeld)
{
	_picking->Pick(_thing);
	// Until the view starts to ease in, it takes the thing under the cursor again in each frame
	for (int frame = 0; frame < 10; ++frame)
	{
		Frame();
	}
	// The cursor no longer picks the thing, but the view keeps it
	_picking->Pick(std::nullopt);
	constexpr glm::vec3 k_Moved {1020.0f, 3.0f, 1050.0f};
	Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_thing).position = k_Moved;
	Frame();
	ASSERT_TRUE(Hand().has_value());
	EXPECT_EQ(Hand()->x, k_Moved.x);
	EXPECT_EQ(Hand()->z, k_Moved.z);
}

TEST_F(ClearViewHand, WithNoThingOrTheKeysLetGoTheCameraDoesNotMoveTheHand)
{
	// No thing under the cursor: the view turns round the hand, and leaves it where it is
	Frame();
	EXPECT_FALSE(Hand().has_value());

	_picking->Pick(_thing);
	_keys->held = false;
	for (int frame = 0; frame < 60; ++frame)
	{
		Frame();
	}
	EXPECT_FALSE(Hand().has_value());
}

TEST_F(ClearViewHand, AThingThatIsGoneLetsTheHandGo)
{
	_picking->Pick(_thing);
	Frame();
	ASSERT_TRUE(Hand().has_value());
	Locator::entitiesRegistry::value().Destroy(_thing);
	Frame();
	EXPECT_FALSE(Hand().has_value());
}
