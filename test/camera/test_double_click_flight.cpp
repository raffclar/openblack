/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <chrono>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/CameraLimits.h"
#include "Camera/CameraModel.h"
#include "Camera/CameraZones.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/DoubleClickFlight.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using namespace openblack::camera_fly;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;

camera_limits::Ground FlatLand(float height)
{
	return {
	    .height = [height](glm::vec2) { return height; },
	    .normal = [](glm::vec2) { return glm::vec3(0.0f, 1.0f, 0.0f); },
	};
}

/// A camera model that always wants to go to the same place, easing there over a set time
class FixedModel final: public CameraModel
{
public:
	FixedModel(glm::vec3 origin, glm::vec3 focus, std::chrono::microseconds duration)
	    : _origin(origin)
	    , _focus(focus)
	    , _duration(duration)
	{
	}
	/// It sets off once, and then lets the camera ease
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		if (_setOff)
		{
			return std::nullopt;
		}
		_setOff = true;
		return CameraInterpolationUpdateInfo {.origin = _origin, .focus = _focus, .duration = _duration};
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _focus; }
	[[nodiscard]] std::chrono::seconds GetIdleTime() const override { return std::chrono::seconds::zero(); }

private:
	glm::vec3 _origin;
	glm::vec3 _focus;
	std::chrono::microseconds _duration;
	bool _setOff {false};
};

/// A land's zones with a square fence from 1000 to 2000 across
class SquareFence final: public ecs::systems::CameraZoneSystemInterface
{
public:
	SquareFence()
	{
		_zones.fenceOn = true;
		_zones.fence = {{1000.0f, 0.0f, 1000.0f}, {2000.0f, 0.0f, 1000.0f}, {2000.0f, 0.0f, 2000.0f}, {1000.0f, 0.0f, 2000.0f}};
	}
	bool SetZones(std::string_view /*fileName*/) override { return true; }
	[[nodiscard]] const camera_zones::Zones& GetZones() const override { return _zones; }
	[[nodiscard]] const std::string& GetFileName() const override { return _name; }
	void FenceHit(const glm::vec3& /*origin*/, const glm::vec3& /*listener*/) override {}
	[[nodiscard]] const particles::LightSheet& GetForceField() const override { return _sheet; }
	[[nodiscard]] uint32_t GetFenceHits() const override { return 0; }

private:
	camera_zones::Zones _zones;
	std::string _name;
	particles::LightSheet _sheet;
};
} // namespace

TEST(DoubleClickFlight, ARecordedFlightOverFlatLand)
{
	// As the game flew in its recorded double click (test_camera's DoubleClickFlyTo): from 174 away, looking along the
	// ground, to a place more than 10 from what the middle of the screen shows
	const glm::vec3 origin {1000.0f, 3.0f, 1120.0f};
	const glm::vec3 focus {1000.0f, 0.0f, -368.455078f};
	const glm::vec3 place {1080.70996f, 0.0f, 966.307434f};
	const auto flight = Plan(Reason::DoubleClick, FlatLand(0.0f), place, origin, focus, {1000.0f, 0.0f, -368.546997f});
	EXPECT_TRUE(flight.halfWay);
	EXPECT_TRUE(flight.woosh);
	EXPECT_FLOAT_EQ(flight.startSeconds, 0.0f);
	EXPECT_EQ(flight.focus, place);
	// The game's own end: 100 from the place, its heading kept, looking down as eased from the old pitch and the flat land
	EXPECT_NEAR(flight.origin.x, 1080.70996f, 0.05f);
	EXPECT_NEAR(flight.origin.y, 50.6682434f, 0.05f);
	EXPECT_NEAR(flight.origin.z, 1052.52075f, 0.05f);
	// Its high point, half way from where the camera was
	const auto halfWay = HalfWayPoint(flight.origin, origin, 0.1f, [](glm::vec2) { return 0.0f; });
	EXPECT_NEAR(halfWay.x, 1040.35498f, 0.05f);
	EXPECT_NEAR(halfWay.y, 37.3544312f, 0.05f);
	EXPECT_NEAR(halfWay.z, 1086.26038f, 0.05f);
}

TEST(DoubleClickFlight, NearerPlacesAreFlownToFromCloserAndPartWayIntoTheEase)
{
	const auto land = FlatLand(0.0f);
	const glm::vec3 focus {0.0f, 0.0f, -500.0f};
	const auto from = [&](float away, glm::vec3 screenCentre) {
		const glm::vec3 origin {0.0f, 0.0f, away};
		return Plan(Reason::DoubleClick, land, {0.0f, 0.0f, 0.0f}, origin, focus, screenCentre);
	};
	// Far, but at what the camera already looks at: a medium flight
	const auto atFocus = from(200.0f, {0.0f, 0.0f, 5.0f});
	EXPECT_NEAR(glm::length(atFocus.origin), k_MediumDistance, 1e-3f);
	EXPECT_FALSE(atFocus.halfWay);
	EXPECT_FALSE(atFocus.woosh);
	EXPECT_FLOAT_EQ(atFocus.startSeconds, 0.75f);
	EXPECT_NEAR(glm::length(from(100.0f, {}).origin), k_MediumDistance, 1e-3f);
	EXPECT_NEAR(glm::length(from(80.0f, {}).origin), k_NearDistance, 1e-3f);
	EXPECT_NEAR(glm::length(from(40.0f, {}).origin), k_CloseDistance, 1e-3f);
	EXPECT_FLOAT_EQ(from(40.0f, {}).startSeconds, 0.75f);
}

TEST(DoubleClickFlight, LandGrippedTooFarIsFlownToFromAThousandLookingAsBefore)
{
	const glm::vec3 origin {0.0f, 400.0f, 4000.0f};
	const glm::vec3 focus {0.0f, 0.0f, 0.0f};
	const glm::vec3 place {0.0f, 0.0f, -100.0f};
	const auto flight = Plan(Reason::GripTooFar, FlatLand(0.0f), place, origin, focus, {});
	EXPECT_NEAR(glm::distance(flight.origin, place), k_GripDistance, 1e-2f);
	EXPECT_FLOAT_EQ(flight.startSeconds, k_GripStart);
	EXPECT_FALSE(flight.halfWay);
	EXPECT_TRUE(flight.woosh);
	// The heading kept, the shallow pitch raised to an eighth of a half turn
	const auto bearing = BearingOf(flight.origin, place);
	EXPECT_NEAR(bearing.heading, BearingOf(origin, focus).heading, 1e-4f);
	EXPECT_NEAR(bearing.pitch, k_Pi / 8.0f, 1e-5f);
}

TEST(DoubleClickFlight, TheFlightLooksOverTheLowestLand)
{
	// A ridge behind the place, where the camera stands now: the flight turns round to look from the low side
	const camera_limits::Ground ridge {
	    .height = [](glm::vec2 point) { return point.y > -20.0f ? 80.0f : 0.0f; },
	    .normal = [](glm::vec2) { return glm::vec3(0.0f, 1.0f, 0.0f); },
	};
	const auto flight =
	    Plan(Reason::DoubleClick, ridge, {0.0f, 0.0f, 0.0f}, {0.0f, 20.0f, 300.0f}, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -10.0f});
	EXPECT_LT(flight.origin.z, 0.0f);
	// On flat land the pitch is a fifth of the old one plus a tenth of the land's (a quarter turn up) plus 0.377
	const auto best = BestBearing(FlatLand(0.0f), {.heading = 0.0f, .pitch = 0.1f}, 100.0f, {});
	EXPECT_NEAR(best.pitch, 0.02f + 0.2f * 1.53938043f * 0.5f + 0.37699114f, 1e-5f);
	EXPECT_FLOAT_EQ(best.heading, 0.0f);
}

TEST(DoubleClickFlight, APlaceOutsideTheFenceIsMovedOntoItAndOneBeyondTheDiscIsNotFlownTo)
{
	EXPECT_FALSE(Destination(std::nullopt, {}).has_value());
	// A square fence from 1000 to 2000
	camera_zones::Zones zones;
	zones.fenceOn = true;
	zones.fence = {{1000.0f, 0.0f, 1000.0f}, {2000.0f, 0.0f, 1000.0f}, {2000.0f, 0.0f, 2000.0f}, {1000.0f, 0.0f, 2000.0f}};
	const auto inside = Destination(glm::vec3 {1500.0f, 12.0f, 1500.0f}, zones);
	ASSERT_TRUE(inside.has_value());
	EXPECT_EQ(*inside, glm::vec3(1500.0f, 12.0f, 1500.0f));
	// Outside to the west: along the ground's first axis to the fence, the height kept
	const auto moved = Destination(glm::vec3 {800.0f, 7.0f, 1500.0f}, zones);
	ASSERT_TRUE(moved.has_value());
	EXPECT_NEAR(moved->x, 1000.0f, 1e-3f);
	EXPECT_NEAR(moved->z, 1500.0f, 1e-3f);
	EXPECT_FLOAT_EQ(moved->y, 7.0f);
	// Beyond the disc round the island, nothing
	EXPECT_FALSE(Destination(glm::vec3 {2560.0f + 5200.0f, 0.0f, 2560.0f}, {}).has_value());
}

TEST(DoubleClickFlight, TheHighPointClearsTheLand)
{
	const auto point = HalfWayPoint({100.0f, 50.0f, 0.0f}, {0.0f, 50.0f, 0.0f}, 0.1f, [](glm::vec2) { return 70.0f; });
	EXPECT_FLOAT_EQ(point.y, 80.0f);
	const auto low = HalfWayPoint({100.0f, 50.0f, 0.0f}, {0.0f, 50.0f, 0.0f}, 0.1f, [](glm::vec2) { return 0.0f; });
	EXPECT_FLOAT_EQ(low.y, 60.0f);
}

TEST(CameraWorldBall, TheEyeIsSentBackIntoTheBallOverThreeSecondsOnItsOwnClock)
{
	using namespace std::chrono_literals;
	Camera camera;
	const glm::vec3 far {2560.0f, 9000.0f, 2560.0f};
	const glm::vec3 focus {2560.0f, 0.0f, 2600.0f};
	std::ignore = camera.SetModel(std::make_unique<FixedModel>(far, focus, 300'000us));
	camera.Update(16'000us);
	// The eye goes to the ball's edge straight below where it wanted to be, easing over three seconds
	EXPECT_NEAR(camera.GetOrigin(Camera::Interpolation::Target).y, 3499.0f, 1e-2f);
	EXPECT_EQ(camera.GetInterpolatorDuration(), 3'000'000us);
	// What it looks at keeps the model's own easing
	EXPECT_EQ(camera.GetFocus(Camera::Interpolation::Target), focus);
	EXPECT_EQ(camera.GetFocusInterpolatorDuration(), 300'000us);
	// Each clock runs on by itself: the focus arrives long before the eye
	for (int i = 0; i < 30; ++i)
	{
		camera.Update(16'000us);
	}
	EXPECT_NEAR(glm::distance(camera.GetFocus(), focus), 0.0f, 1e-3f);
	EXPECT_LT(camera.GetOrigin().y, 3498.0f);
	// Inside the ball, the eye goes where the model says
	std::ignore = camera.SetModel(std::make_unique<FixedModel>(glm::vec3 {2560.0f, 900.0f, 2560.0f}, focus, 300'000us));
	camera.Update(16'000us);
	EXPECT_EQ(camera.GetOrigin(Camera::Interpolation::Target), glm::vec3(2560.0f, 900.0f, 2560.0f));
	EXPECT_EQ(camera.GetInterpolatorDuration(), 300'000us);
}

TEST(CreatureFollowFence, TheFollowCameraStopsAtTheFenceAndGivesTheCameraBack)
{
	using namespace std::chrono_literals;
	Locator::cameraZoneSystem::emplace<SquareFence>();
	Camera camera;
	// A creature just inside the fence's west side, the camera looking at it from outside, to the west
	const glm::vec3 creature {1010.0f, 0.0f, 1500.0f};
	CreatureCameraModel outside({940.0f, 30.0f, 1500.0f}, creature, creature, 10.0f);
	const auto stopped = outside.Update(16'000us, camera);
	ASSERT_TRUE(stopped.has_value());
	// It goes no further than where its line of sight crosses the fence
	EXPECT_NEAR(stopped->origin.x, 1000.0f, 1e-2f);
	EXPECT_NEAR(stopped->origin.z, 1500.0f, 1e-2f);
	EXPECT_TRUE(outside.WantsToLeave());
	EXPECT_TRUE(outside.StoppedAtFence());
	// Looking from inside, it follows as before
	CreatureCameraModel inside({1100.0f, 30.0f, 1500.0f}, creature, creature, 10.0f);
	std::ignore = inside.Update(16'000us, camera);
	EXPECT_FALSE(inside.WantsToLeave());
	EXPECT_FALSE(inside.StoppedAtFence());
	Locator::cameraZoneSystem::reset();
}
