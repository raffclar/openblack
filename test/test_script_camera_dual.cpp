/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Step 3 of the script camera: the dual camera on two objects (start, update, validity, release and the stacked
// modes), the camera shakes and the camera exclusion zone (ResetExclusionFile, LoadExclusionFile, InsideInclusion)

#include <cmath>
#include <cstring>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Camera/CameraShake.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"

using namespace openblack;

namespace
{
struct Things
{
	std::map<entt::entity, script_camera::ThingInfo> things;

	Things()
	{
		script_camera::Reset();
		script_camera::detail::SetThingReaderForTests([this](entt::entity e) -> std::optional<script_camera::ThingInfo> {
			const auto it = things.find(e);
			return it != things.end() ? std::optional(it->second) : std::nullopt;
		});
	}
	~Things()
	{
		script_camera::detail::SetThingReaderForTests(nullptr);
		script_camera::Reset();
	}
	Things(const Things&) = delete;
	Things& operator=(const Things&) = delete;
	Things(Things&&) = delete;
	Things& operator=(Things&&) = delete;
};

constexpr auto k_A = static_cast<entt::entity>(11);
constexpr auto k_B = static_cast<entt::entity>(12);
constexpr auto k_C = static_cast<entt::entity>(13);

script_camera::ThingInfo MakeThing(glm::vec3 mapPoint, float height, std::optional<float> radius)
{
	script_camera::ThingInfo info;
	info.mapPoint = mapPoint;
	info.height = height;
	info.radius2d = radius;
	info.drawnPoint = mapPoint + glm::vec3(5.0f, 0.0f, 0.0f); // not read by the dual camera
	return info;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-3f)
{
	EXPECT_NEAR(a.x, b.x, tolerance);
	EXPECT_NEAR(a.y, b.y, tolerance);
	EXPECT_NEAR(a.z, b.z, tolerance);
}

/// The expected destinations of the dual camera's update, written out again independently
void ExpectedDual(const glm::vec3& a, float ha, std::optional<float> ra, const glm::vec3& b, std::optional<float> hb,
                  std::optional<float> rb, float factor, glm::vec3& focus, glm::vec3& position)
{
	const glm::vec3 middle = (b + a) * 0.5f;
	const float mean = (ha + hb.value_or(1.0f)) * 0.5f;
	const float tallest = std::max(ha, hb.value_or(0.0f));
	focus = {middle.x, middle.y + mean * 0.5f, middle.z};
	const float apart = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
	const float distance = (apart + rb.value_or(30.0f) + ra.value_or(30.0f)) * factor + tallest * 1.4f;
	const glm::vec3 v = b - a;
	const auto heading = static_cast<float>(static_cast<double>(script_camera::k_DualHeading) - affine::GetYAngle(v));
	position = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, script_camera::k_DualPitch);
}

/// A Lionhead segment file with one "cameraexc" segment
std::vector<uint8_t> ZoneFile(int32_t drawForceField, const std::vector<glm::vec3>& points, int32_t recordSize = 0x24)
{
	std::vector<uint8_t> data;
	const auto put = [&data](const void* p, size_t n) {
		const auto* bytes = static_cast<const uint8_t*>(p);
		data.insert(data.end(), bytes, bytes + n);
	};
	const int32_t header = 1;
	const int32_t flag = 0;
	const int32_t one = 1;
	const float limitA = 255.2f;
	const float limitB = 139.3f;
	const auto count = static_cast<int32_t>(points.size());
	const int32_t records = 0;
	put(&header, 4);
	put(&flag, 4);
	put(&drawForceField, 4);
	put(&one, 4);
	put(&flag, 4);
	put(&limitA, 4);
	put(&limitB, 4);
	put(&count, 4);
	for (const auto& p : points)
	{
		put(&p.x, 4);
		put(&p.y, 4);
		put(&p.z, 4);
	}
	put(&records, 4);
	put(&recordSize, 4);
	std::vector<uint8_t> file(8 + 32 + 4);
	std::memcpy(file.data(), "LiOnHeAd", 8);
	std::memcpy(file.data() + 8, "cameraexc", 9);
	const auto size = static_cast<uint32_t>(data.size());
	std::memcpy(file.data() + 40, &size, 4);
	file.insert(file.end(), data.begin(), data.end());
	return file;
}

const std::vector<glm::vec3> k_Square {{0, 5, 0}, {100, 5, 0}, {100, 5, 100}, {0, 5, 100}};
} // namespace

// ---- Dual camera -----------------------------------------------------------------------------------------------------

TEST(ScriptCameraDual, TwoThingsMiddleAndDistance)
{
	Things t;
	const glm::vec3 a(1000, 10, 2000);
	const glm::vec3 b(1040, 20, 2030);
	t.things[k_A] = MakeThing(a, 2.0f, 1.0f);
	t.things[k_B] = MakeThing(b, 4.0f, 3.0f);
	ASSERT_TRUE(script_camera::Begin({900, 50, 1900}, {1000, 0, 2000}));
	script_camera::StartDual(k_A, k_B, {}, {}); // over the script mode: the zoomers are not taken
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::ScriptModeCurrent()); // the script opcodes now meet "the wrong camera mode"
	EXPECT_TRUE(script_camera::HasMode());
	script_camera::Frame(0.05f, 16, 0.016f);
	glm::vec3 focus;
	glm::vec3 position;
	ExpectedDual(a, 2.0f, 1.0f, b, 4.0f, 3.0f, 1.0f, focus, position);
	ExpectNear(focus, {1020, 16.5f, 2015}); // middle (1020, 15, 2015) + (2 + 4) / 2 / 2
	ExpectNear(script_camera::Get().focus.GetDestination(), focus);
	ExpectNear(script_camera::Get().position.GetDestination(), position);
	// distance: (50 + 3 + 1) x 1 + 4 x 1.4 = 59.6 from the focus
	EXPECT_NEAR(glm::length(position - focus), 59.6f, 1e-3f);
}

TEST(ScriptCameraDual, PaceFromTwoSecondsToOne)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({900, 50, 900}, {1000, 0, 1000});
	script_camera::StartDual(k_A, k_B, {}, {});
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f); // a new mode starts its clock at 0
	script_camera::Frame(0.075f, 0, 0.0f);
	// 0.075 / 1.5 x (1 - 2) + 2 = 1.95 s
	EXPECT_NEAR(script_camera::Get().focus.axis[0].duration, 1.95f, 1e-5f);
	for (int i = 0; i < 20; ++i)
	{
		script_camera::Frame(0.1f, 0, 0.0f);
	}
	EXPECT_EQ(script_camera::Get().focus.axis[0].duration, 1.0f); // past 1.5 s: 1
}

TEST(ScriptCameraDual, SameThingsKeepTheMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_C] = MakeThing({1200, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	script_camera::StartDual(k_A, k_B, {}, {}); // the same pair again: the new mode deletes itself
	EXPECT_EQ(script_camera::Get().duals.size(), 1u);
	EXPECT_NEAR(script_camera::Get().modeSeconds, 0.1f, 1e-6f);
	script_camera::StartDual(k_B, k_A, {}, {}); // another order is another mode
	EXPECT_EQ(script_camera::Get().duals.size(), 2u);
	EXPECT_TRUE(script_camera::UpdateDual(k_A, k_C)); // SetObjects on the current one
	EXPECT_EQ(script_camera::Get().duals.back().a, k_A);
	EXPECT_EQ(script_camera::Get().duals.back().b, k_C);
}

TEST(ScriptCameraDual, ReleasePopsBackToTheScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	EXPECT_FALSE(script_camera::ReleaseDual()); // the current mode is not a dual camera
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f); // popping back restarts the mode's clock
	EXPECT_FALSE(script_camera::UpdateDual(k_A, k_B));
}

TEST(ScriptCameraDual, EndReleasesOneDualThenTheScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	EXPECT_TRUE(script_camera::End()); // the dual, then the script mode
	EXPECT_FALSE(script_camera::HasMode());
	// Two dual cameras: End releases one, the other is current -> "wrong camera mode - excep", the script mode stays
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::StartDual(k_B, k_A, {}, {});
	EXPECT_FALSE(script_camera::End());
	EXPECT_TRUE(script_camera::Active());
	EXPECT_TRUE(script_camera::DualCurrent());
}

TEST(ScriptCameraDual, PointCamera)
{
	Things t;
	const glm::vec3 a(1000, 10, 2000);
	const glm::vec3 point(1060, 0, 2080);
	t.things[k_A] = MakeThing(a, 3.0f, 2.0f);
	script_camera::Begin({}, {});
	script_camera::StartDualWithPoint(k_A, point, {}, {});
	EXPECT_EQ(script_camera::Get().duals.back().distanceFactor, script_camera::k_DualPointDistanceFactor);
	script_camera::StartDualWithPoint(k_A, point, {}, {}); // same thing and point: not stacked again
	EXPECT_EQ(script_camera::Get().duals.size(), 1u);
	script_camera::Frame(0.1f, 0, 0.0f);
	glm::vec3 focus;
	glm::vec3 position;
	// B's heights are 1 for the mean and 0 for the largest; B's radius 30
	ExpectedDual(a, 3.0f, 2.0f, point, std::nullopt, std::nullopt, 1.2f, focus, position);
	ExpectNear(script_camera::Get().focus.GetDestination(), focus);
	ExpectNear(script_camera::Get().position.GetDestination(), position);
}

TEST(ScriptCameraDual, ContainersHaveRadiusThirty)
{
	Things t;
	const glm::vec3 a(1000, 0, 1000);
	const glm::vec3 b(1000, 0, 1050);
	t.things[k_A] = MakeThing(a, 0.0f, std::nullopt);
	t.things[k_B] = MakeThing(b, 0.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto focus = script_camera::Get().focus.GetDestination();
	EXPECT_NEAR(glm::length(script_camera::Get().position.GetDestination() - focus), 50.0f + 30.0f + 30.0f, 1e-3f);
}

TEST(ScriptCameraDual, ThingOverTheOtherKeepsTheHeading)
{
	Things t;
	const glm::vec3 a(1000, 0, 1000);
	const glm::vec3 b(1000.005f, 40, 1000.005f); // |x|, |z| <= 0.01
	t.things[k_A] = MakeThing(a, 2.0f, 1.0f);
	t.things[k_B] = MakeThing(b, 2.0f, 1.0f);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto focus = script_camera::Get().focus.GetDestination();
	const float distance = (std::sqrt(0.005f * 0.005f * 2.0f) + 1.0f + 1.0f) + 2.0f * 1.4f;
	ExpectNear(script_camera::Get().position.GetDestination(),
	           script_camera::PointFromDistanceHeadingAndPitch(focus, distance, script_camera::k_DualHeading,
	                                                           script_camera::k_DualPitch));
}

TEST(ScriptCameraDual, GoneThingDropsTheModeOnTheTurn)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	t.things[k_B].available = false; // no longer available
	script_camera::Validate();       // the stacked modes are checked first
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	// a point camera with no thing moves nothing and goes at the next turn
	script_camera::StartDualWithPoint(entt::null, {0, 0, 0}, {}, {});
	const auto before = script_camera::Get().focus.GetDestination();
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_EQ(script_camera::Get().focus.GetDestination(), before);
	script_camera::Validate();
	EXPECT_FALSE(script_camera::DualCurrent());
}

TEST(ScriptCameraDual, DualDrivesWithoutAScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	// The player's zoomers, still heading somewhere: the dual camera goes on from them as they are (as BeginFrom)
	Zoomer3 origin;
	Zoomer3 focus;
	origin.SetPosition({900, 60, 900});
	focus.SetPosition({1000, 0, 1000});
	origin.SetDestinationWithTime({950, 60, 900}, 2.0f);
	origin.Update(0.5f);
	script_camera::StartDual(k_A, k_B, origin, focus); // over the player's mode
	EXPECT_TRUE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), origin.GetCurrentValue());
	EXPECT_EQ(script_camera::Get().position.GetSpeed(), origin.GetSpeed());
	EXPECT_EQ(script_camera::Get().position.GetDestination(), glm::vec3(950, 60, 900));
	EXPECT_EQ(script_camera::Get().focus.GetCurrentValue(), glm::vec3(1000, 0, 1000));
	// Another one on top does not take them again
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto moved = script_camera::Get().position.GetCurrentValue();
	script_camera::StartDual(k_B, k_A, origin, focus);
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), moved);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_FALSE(script_camera::HasMode());
}

TEST(ScriptCameraDual, ArrivedIgnoresThePathUnderneath)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({1050, 30, 900}, {1050, 0, 1000});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_FALSE(script_camera::ScriptArrived()); // arrival is judged on the zoomers
	// the zoomers on their destinations (the dual camera sets new ones every frame, so it only arrives when still)
	const auto& state = script_camera::Get();
	script_camera::SetPositionAndFocus(state.position.GetDestination(), state.focus.GetDestination());
	EXPECT_TRUE(script_camera::ScriptArrived());
}

// ---- Camera shake ----------------------------------------------------------------------------------------------------

TEST(CameraShake, CreateAndTick)
{
	camera_shake::Reset();
	camera_shake::StartCameraShake({10, 0, 10}, 50.0f, 2.0f, 0.4004f); // rounded: 400.4 -> 400 ms
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 400);
	EXPECT_EQ(camera_shake::Checkers().front().remainingMs, 400);
	EXPECT_FALSE(camera_shake::Checkers().front().yOnly);
	camera_shake::Create(5.0f, {0, 0, 0}, 1.0f, 100, true);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 100); // newest first
	// 100 - 100 = 0: freed
	camera_shake::Tick(100);
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().remainingMs, 300);
	camera_shake::Tick(299);
	EXPECT_EQ(camera_shake::Checkers().size(), 1u);
	camera_shake::Tick(1);
	EXPECT_TRUE(camera_shake::Checkers().empty());
}

TEST(CameraShake, OneTickAFrame)
{
	// The script camera's frame does not tick the shakes; ApplyShake does, once per rendered frame
	camera_shake::Reset();
	camera_shake::Create(50.0f, {0, 0, 0}, 1.0f, 1000, false);
	Camera camera;
	static_cast<void>(script_camera::UpdateCamera(camera, 0.016f, 16, 0.016f));
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().remainingMs, 1000);
	script_camera::ApplyShake(camera, camera.GetOrigin());
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_LT(camera_shake::Checkers().front().remainingMs, 1000);
	camera_shake::Reset();
}

TEST(CameraSphere, ThePlayersCameraStaysInTheWorld)
{
	// The camera's update pulls any mode's position destination back to the sphere round (2560, 0, 2560): it tests 3500
	// squared but scales by the slightly different k_DiscScale, so the point lands at 1 / k_DiscScale (about 3499)
	Camera camera;
	camera.GetOriginZoomer().SetPosition({2560.0f + 5000.0f, 0.0f, 2560.0f});
	camera.UpdateZoomers(std::nullopt, 0.016f);
	const auto destination = camera.GetOriginZoomer().GetDestination();
	EXPECT_NEAR(destination.x, 2560.0f + 1.0f / script_camera::k_DiscScale, 0.01f);
	EXPECT_EQ(destination.z, 2560.0f);
	camera.GetOriginZoomer().SetPosition({3000.0f, 50.0f, 3000.0f}); // inside: untouched
	camera.UpdateZoomers(std::nullopt, 0.016f);
	EXPECT_EQ(camera.GetOriginZoomer().GetDestination(), glm::vec3(3000.0f, 50.0f, 3000.0f));
}

TEST(CameraShake, OnlyWithinTheRadiusAndLessAndLess)
{
	camera_shake::Reset();
	camera_shake::Create(50.0f, {0, 0, 0}, 4.0f, 1000, false);
	glm::vec3 position(100, 0, 0);
	glm::vec3 target(110, 0, 0);
	camera_shake::Adjust({100, 0, 0}, position, target); // 100 from the point: no shake
	EXPECT_EQ(position, glm::vec3(100, 0, 0));
	EXPECT_EQ(target, glm::vec3(110, 0, 0));
	camera_shake::Tick(750); // a = 250 / 1000 x 4 = 1
	for (int i = 0; i < 50; ++i)
	{
		glm::vec3 p(10, 0, 0);
		glm::vec3 f(20, 0, 0);
		camera_shake::Adjust({10, 0, 0}, p, f);
		EXPECT_LE(std::abs(p.x - 10.0f), 1.0f);
		EXPECT_LE(std::abs(p.y), 1.0f);
		EXPECT_LE(std::abs(p.z), 1.0f);
		EXPECT_LE(std::abs(f.x - 20.0f), 1.0f);
	}
}

TEST(CameraShake, YOnlyMovesY)
{
	camera_shake::Reset();
	camera_shake::Create(50.0f, {0, 0, 0}, 4.0f, 1000, true);
	glm::vec3 p(10, 0, 0);
	glm::vec3 f(20, 0, 0);
	camera_shake::Adjust({10, 0, 0}, p, f);
	EXPECT_EQ(p.x, 10.0f);
	EXPECT_EQ(p.z, 0.0f);
	EXPECT_EQ(f.x, 20.0f);
	EXPECT_EQ(f.z, 0.0f);
	camera_shake::Reset();
}

TEST(CameraShake, TheNearestDecides)
{
	camera_shake::Reset();
	camera_shake::Create(1000.0f, {0, 0, 0}, 4.0f, 1000, false); // far one with a big radius
	camera_shake::Create(5.0f, {30, 0, 0}, 4.0f, 1000, false);   // nearer, but its radius does not reach
	glm::vec3 p(20, 0, 0);
	glm::vec3 f(25, 0, 0);
	camera_shake::Adjust({20, 0, 0}, p, f); // nearest is the second (10 < 20): 10 >= 5, nothing
	EXPECT_EQ(p, glm::vec3(20, 0, 0));
	camera_shake::Reset();
}

// ---- Camera exclusion zone -------------------------------------------------------------------------------------------

TEST(PlayerCameraZone, LoadsTheSegment)
{
	player_camera::Reset();
	ASSERT_TRUE(player_camera::LoadExclusionFile(ZoneFile(0, k_Square), 1));
	const auto& zone = player_camera::Get().zone;
	EXPECT_EQ(zone.header, 1);
	EXPECT_EQ(zone.drawForceField, 0);
	EXPECT_EQ(zone.flagC5E14C, 1);
	EXPECT_EQ(zone.limit9CE6AC, 255.2f);
	EXPECT_EQ(zone.limit9CE6A8, 139.3f);
	EXPECT_EQ(zone.forceFieldPoints.size(), 4u);
	EXPECT_TRUE(zone.exclusions.empty());
	player_camera::ResetExclusionFile(1);
	EXPECT_TRUE(player_camera::Get().zone.forceFieldPoints.empty());
	EXPECT_EQ(player_camera::Get().zone.limit9CE6AC, 500.0f);
	EXPECT_EQ(player_camera::Get().zone.flag9CE6B0, 1);
	EXPECT_FALSE(player_camera::LoadExclusionFile({}, 1)); // no LiOnHeAd
	player_camera::Reset();
}

TEST(PlayerCameraZone, InsideInclusion)
{
	player_camera::Reset();
	ASSERT_TRUE(player_camera::LoadExclusionFile(ZoneFile(1, k_Square), 1));
	const auto& zone = player_camera::Get().zone;
	glm::vec3 hit;
	glm::vec3 normal;
	EXPECT_TRUE(player_camera::InsideInclusion(zone, {50, 0, 50}, {1, 0, 0}, &hit, &normal));
	// the crossing ahead on the edge (100, 0)-(100, 100); every corner is farther, so the hit stays there
	ExpectNear(hit, {100, 0, 50});
	ExpectNear(normal, {-100, 0, 0}); // e = (100, 0) - (100, 100): (e.z, 0, -e.x)
	EXPECT_FALSE(player_camera::InsideInclusion(zone, {150, 0, 50}, {1, 0, 0}, &hit, nullptr));
	EXPECT_FALSE(player_camera::InsideInclusion(zone, {-50, 0, 50}, {0, 0, 1}, nullptr, nullptr));
	EXPECT_TRUE(player_camera::InsideInclusion(zone, {0, 0, 0}, {1, 0, 0}, nullptr, nullptr)); // on a point
	// near a corner, the corner is nearer than the crossing
	EXPECT_TRUE(player_camera::InsideInclusion(zone, {95, 0, 2}, {0, 0, 1}, &hit, nullptr));
	ExpectNear(hit, {100, 5, 0});
	// no force field: always inside, the hit untouched
	player_camera::ResetExclusionFile(1);
	hit = {1, 2, 3};
	EXPECT_TRUE(player_camera::InsideInclusion(player_camera::Get().zone, {500, 0, 500}, {1, 0, 0}, &hit, nullptr));
	EXPECT_EQ(hit, glm::vec3(1, 2, 3));
	player_camera::Reset();
}

TEST(PlayerCameraZone, FixedRotation)
{
	player_camera::Reset();
	player_camera::ForceRotateAboutPoint(glm::vec3(1, 2, 3));
	EXPECT_TRUE(player_camera::Get().fixedRotation.on);
	EXPECT_EQ(player_camera::Get().fixedRotation.point, glm::vec3(1, 2, 3));
	player_camera::ForceRotateAboutPoint(std::nullopt);
	EXPECT_FALSE(player_camera::Get().fixedRotation.on);
	EXPECT_EQ(player_camera::Get().fixedRotation.point, glm::vec3(1, 2, 3)); // the point is kept
	EXPECT_EQ(player_camera::Get().inclusionDistance, player_camera::k_NoInclusionDistance);
	player_camera::Reset();
}
