/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro's walking villagers: a villager walking a camera track, MoveAlongPath (the retime by the speed, the step,
// the turn and the move) and GetWalkPathPercentage, on a hand-made straight track.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CameraTracks.h"
#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MobileWalkPath.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingWalkPath.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace living = openblack::ecs::living;

namespace
{
/// One straight segment from (0, 0, 0) to (10, 0, 0) in 1000 ms, for both ways; the focus way's length 10
std::shared_ptr<const CameraTrack> StraightTrack()
{
	auto track = std::make_shared<CameraTrack>();
	for (auto* way : {&track->position, &track->focus})
	{
		way->points = {glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f)};
		way->handles = {{glm::vec3(10.0f / 3.0f, 0.0f, 0.0f), glm::vec3(20.0f / 3.0f, 0.0f, 0.0f)}};
		way->times = {0.0f, 1000.0f};
		way->speeds = {10.0f, 10.0f};
		way->duration = 1000;
		way->length = 10.0f;
	}
	return track;
}

class LivingWalkPathTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// a villager at the origin walking 6550 units a turn (s = 6550 / 655 x 0.1 = 1: step 1000 / (10 / 1) = 100)
	static entt::entity MakeWalker(float from, float to)
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.town = entt::null;
		v.abode = entt::null;
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<WallHug>(e, glm::vec2(0.0f), glm::vec2(0.0f), 0.0f, map_coords::ToMetres(6550));
		Reg().Assign<LivingAction>(e, VillagerStates::MoveAlongPath, static_cast<uint16_t>(0));
		auto& walk = Reg().Assign<LivingWalkPath>(e);
		walk.path.track = StraightTrack();
		walk.path.runner = std::make_unique<CameraWayRunner>(walk.path.track->position);
		walk.path.to = to;
		walk.path.forward = true;
		walk.path.current = 1000.0f * from;
		walk.path.step = 100.0f;
		walk.path.cachedSpeed = 6550.0f;
		return e;
	}
};
} // namespace

TEST_F(LivingWalkPathTest, Percentage)
{
	const auto e = MakeWalker(0.25f, 1.0f);
	EXPECT_EQ(living::GetWalkPathPercentage(e).value_or(-1.0f), 0.25f);
	EXPECT_TRUE(living::WalkPathReached(e, 0.25f));
	EXPECT_FALSE(living::WalkPathReached(e, 0.5f));
	// no walk path: reached, no percentage
	const auto other = Reg().Create();
	EXPECT_FALSE(living::GetWalkPathPercentage(other).has_value());
	EXPECT_TRUE(living::WalkPathReached(other, 0.9f));
}

TEST_F(LivingWalkPathTest, StepsAndMoves)
{
	const auto e = MakeWalker(0.0f, 1.0f);
	auto& action = Reg().Get<LivingAction>(e);
	// the first turn samples at 0 (the start), then advances: current 100
	EXPECT_EQ(living::MoveAlongPath(action), 1u);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.current, 100.0f);
	EXPECT_NEAR(Reg().Get<const Transform>(e).position.x, 0.0f, 1e-3f);
	// the next one moves to the 100 ms sample: a tenth of the way, the x axis
	living::MoveAlongPath(action);
	EXPECT_NEAR(Reg().Get<const Transform>(e).position.x, 1.0f, 0.05f);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.current, 200.0f);
}

TEST_F(LivingWalkPathTest, SpeedRetimes)
{
	const auto e = MakeWalker(0.0f, 1.0f);
	auto& action = Reg().Get<LivingAction>(e);
	// half the speed (3275): s = 0.5, L = 20, step = 50
	Reg().Get<WallHug>(e).speed = map_coords::ToMetres(3275);
	living::MoveAlongPath(action);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.cachedSpeed, 3275.0f);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.step, 50.0f);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.current, 50.0f);
	// speed 0: s = 0, L = 0, no step (how the intro stops Father in state 28)
	Reg().Get<WallHug>(e).speed = 0.0f;
	living::MoveAlongPath(action);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.step, 0.0f);
	EXPECT_EQ(Reg().Get<const LivingWalkPath>(e).path.current, 50.0f);
}
