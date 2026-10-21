/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

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
/// A place far from the fight, as a double tap of the temple key gives
constexpr glm::vec3 k_PlaceOrigin {1000.0f, 130.0f, 1600.0f};
constexpr glm::vec3 k_PlaceFocus {1000.0f, 0.0f, 1700.0f};
constexpr float k_TempleRise = 0.4f;
constexpr auto k_Frame = 20ms;

/// No land under the cursor
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

class FightCameraFlight: public testing::Test
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
		Locator::pickingSystem::reset<MockPickingSystem>(new NoLandPicking());
		Locator::gameActionSystem::emplace<MockAction>();
		_camera = std::make_unique<Camera>();
		_camera->SetOrigin(k_Origin).SetFocus(k_Focus);
		_fight = std::make_unique<FightCameraModel>(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus),
		                                            fight_view::View {.origin = k_Origin, .focus = k_Focus});
		_fight->SetFighters({.position = k_Focus + glm::vec3(-5.0f, 0.0f, 0.0f), .radius = 2.0f, .height = 6.0f},
		                    {.position = k_Focus + glm::vec3(5.0f, 0.0f, 0.0f), .radius = 2.0f, .height = 6.0f});
	}

	void TearDown() override
	{
		_fight.reset();
		_camera.reset();
		Locator::gameActionSystem::reset();
		Locator::pickingSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	/// Steps the fight view for a time, the camera put where the view shows it. Gives the highest point of the eye.
	float Run(std::chrono::milliseconds time)
	{
		float highest = _camera->GetOrigin().y;
		for (auto elapsed = 0ms; elapsed < time; elapsed += k_Frame)
		{
			const auto shown = _fight->Update(k_Frame, *_camera);
			if (shown.has_value())
			{
				_camera->SetOrigin(shown->origin).SetFocus(shown->focus);
				highest = std::max(highest, shown->origin.y);
			}
			_camera->SetModelSeconds(_camera->GetModelSeconds() + 0.02f);
		}
		return highest;
	}

	std::unique_ptr<Camera> _camera;
	std::unique_ptr<FightCameraModel> _fight;
};
} // namespace

TEST_F(FightCameraFlight, AFlightGoesThroughItsPointOnTheWayAndTheFightTakesTheCameraBack)
{
	// The camera flies to the fight and follows it
	Run(6000ms);
	const auto atFight = _camera->GetOrigin();
	ASSERT_LT(glm::distance(atFight, k_Focus), 200.0f);
	// A double tap of the temple key starts a flight from where the camera is. The point on the way is half way, raised
	// by 0.4 of the distance across the land, so the eye goes up far above both ends.
	_fight->SetFlight(k_PlaceOrigin, k_PlaceFocus, k_TempleRise);
	EXPECT_FLOAT_EQ(_fight->GetSecondsSinceFlight(), 0.0f);
	const auto highest = Run(800ms);
	EXPECT_GT(highest, k_PlaceOrigin.y + 50.0f);
	// After half the flight the camera goes to the end for one step, then the view of the fight takes it back
	Run(10000ms);
	EXPECT_LT(glm::distance(_camera->GetOrigin(), atFight), 50.0f);
}

TEST_F(FightCameraFlight, AFlightBeforeTheFirstStepTakesThePlaceOfTheFlightToTheFight)
{
	_fight->SetFlight(k_PlaceOrigin, k_PlaceFocus, k_TempleRise);
	// The flight to the fight has no rise; this one goes up far above both ends
	const auto highest = Run(800ms);
	EXPECT_GT(highest, k_PlaceOrigin.y + 50.0f);
}

TEST_F(FightCameraFlight, TheFlightToTheFightHasNoRise)
{
	const auto highest = Run(800ms);
	EXPECT_LT(highest, k_Origin.y + 1.0f);
}
