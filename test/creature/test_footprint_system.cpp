/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footprint system with an injected date: April Fools' day read once per land, the override, and the prints laid
// under a creature's foot (where its body is drawn between turns, as small as it is drawn in its pen), faded and taken
// away

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <numeric>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFootprints.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/FootprintSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/LandFakes.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CalendarDate;
using openblack::ecs::systems::FootprintSystem;

namespace
{
FootprintSystem On(CalendarDate date, int* reads = nullptr)
{
	return FootprintSystem([date, reads]() {
		if (reads != nullptr)
		{
			++*reads;
		}
		return date;
	});
}

class FootprintSystemTest: public ::testing::Test
{
protected:
	/// A posed Giant Ape, its rig in the cache, on a flat land
	entt::entity PosedApe(bool withLand)
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {}};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		if (withLand)
		{
			Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {1});
		}
		const auto creature = test::creature_world::World::MakeCreature();
		auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
		animation.boneMatrices.assign(13, glm::mat4(1.0f));
		animation.mirror.resize(13);
		std::iota(animation.mirror.begin(), animation.mirror.end(), 0u);
		return creature;
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	const test::RestoreService<Locator::terrainSystem> _restoreTerrain;
};
} // namespace

TEST(FootprintSystemDate, AprilFoolsComesFromTheInjectedDate)
{
	auto first = On({.month = 4, .day = 1});
	// no date before the first land
	EXPECT_FALSE(first.IsAprilFools());
	first.Reset();
	EXPECT_TRUE(first.IsAprilFools());
	auto second = On({.month = 4, .day = 2});
	second.Reset();
	EXPECT_FALSE(second.IsAprilFools());
	// without a date, never
	FootprintSystem none {{}};
	none.Reset();
	EXPECT_FALSE(none.IsAprilFools());
}

TEST(FootprintSystemDate, TheOverrideWins)
{
	auto aprilFools = On({.month = 4, .day = 1});
	aprilFools.Reset();
	aprilFools.SetAprilFoolsOverride(false);
	EXPECT_FALSE(aprilFools.IsAprilFools());
	auto ordinary = On({.month = 7, .day = 14});
	ordinary.Reset();
	ordinary.SetAprilFoolsOverride(true);
	EXPECT_TRUE(ordinary.IsAprilFools());
	EXPECT_EQ(ordinary.GetAprilFoolsOverride(), std::optional<bool>(true));
	ordinary.SetAprilFoolsOverride(std::nullopt);
	EXPECT_FALSE(ordinary.IsAprilFools());
}

TEST(FootprintSystemDate, TheDateIsReadOncePerLand)
{
	int reads = 0;
	auto system = On({.month = 4, .day = 1}, &reads);
	EXPECT_EQ(reads, 0);
	system.Reset();
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_TRUE(system.IsAprilFools());
	}
	EXPECT_EQ(reads, 1);
	system.Reset();
	EXPECT_EQ(reads, 2);
}

TEST_F(FootprintSystemTest, NoPrintWithoutALandOrARig)
{
	auto system = On({.month = 7, .day = 14});
	// no rig in the cache
	const auto bare = test::creature_world::World::MakeCreature();
	Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {1});
	system.Step(bare);
	EXPECT_TRUE(system.GetPrints().empty());
	// a rig, but no land
	Locator::terrainSystem::reset();
	system.Step(PosedApe(false));
	EXPECT_TRUE(system.GetPrints().empty());
	// nor for what is no creature
	system.Step(test::creature_world::World::Registry().Create());
	EXPECT_TRUE(system.GetPrints().empty());
	EXPECT_EQ(system.GetDroppedCount(), 0u);
}

TEST_F(FootprintSystemTest, APrintIsLaidFadesAndGoesWithTheLand)
{
	auto system = On({.month = 7, .day = 14});
	system.Reset();
	system.Step(PosedApe(true));
	ASSERT_EQ(system.GetPrints().size(), 1u);
	EXPECT_EQ(system.GetPrints().front().alpha, creature_footprints::k_StartAlpha);
	// on the land, lifted a little
	EXPECT_FLOAT_EQ(system.GetPrints().front().corners[0].y,
	                test::WaterCellIsland::k_HeightMarker + creature_footprints::k_Lift);

	system.Update(std::chrono::duration<float, std::milli>(1000.0f));
	ASSERT_EQ(system.GetPrints().size(), 1u);
	EXPECT_LT(system.GetPrints().front().alpha, creature_footprints::k_StartAlpha);

	system.Reset();
	EXPECT_TRUE(system.GetPrints().empty());
	EXPECT_EQ(system.GetDroppedCount(), 0u);
}

TEST_F(FootprintSystemTest, APrintIsLaidUnderTheFootWhereTheBodyIsDrawn)
{
	auto system = On({.month = 7, .day = 14});
	system.Reset();
	const auto creature = PosedApe(true);
	auto& registry = test::creature_world::World::Registry();
	const auto transform = registry.Get<Transform>(creature);
	auto& locomotion = registry.Get<CreatureLocomotion>(creature);
	locomotion.started = true;
	locomotion.toPosition = transform.position;
	auto& pose = registry.Get<CreatureDrawPose>(creature);
	pose = {.position = transform.position, .rotation = transform.rotation};
	system.Step(creature);
	// between turns, a step behind where its turn put it
	const glm::vec3 step {-3.0f, 0.0f, 1.5f};
	pose.position = transform.position + step;
	system.Step(creature);
	ASSERT_EQ(system.GetPrints().size(), 2u);
	const auto& standing = system.GetPrints().front();
	const auto& between = system.GetPrints().back();
	for (size_t i = 0; i < standing.corners.size(); ++i)
	{
		EXPECT_NEAR(between.corners.at(i).x, standing.corners.at(i).x + step.x, 1e-3f);
		EXPECT_NEAR(between.corners.at(i).z, standing.corners.at(i).z + step.z, 1e-3f);
	}
	EXPECT_EQ(registry.Get<Transform>(creature).position, transform.position);
}

TEST_F(FootprintSystemTest, InItsPenThePrintIsAsSmallAsTheFootIsDrawn)
{
	auto system = On({.month = 7, .day = 14});
	system.Reset();
	const auto creature = PosedApe(true);
	auto& registry = test::creature_world::World::Registry();
	system.Step(creature);
	// drawn at 0.22 of its own size, as in its temple's pen
	constexpr float k_Share = 0.22f;
	const auto own = registry.Get<Transform>(creature).scale;
	registry.Get<CreatureDrawPose>(creature).scale = own * k_Share;
	system.Step(creature);
	ASSERT_EQ(system.GetPrints().size(), 2u);
	const auto& full = system.GetPrints().front().corners;
	const auto& shrunk = system.GetPrints().back().corners;
	EXPECT_GT(glm::distance(full.at(0), full.at(2)), 0.0f);
	EXPECT_NEAR(glm::distance(glm::xz(shrunk.at(0)), glm::xz(shrunk.at(2))),
	            glm::distance(glm::xz(full.at(0)), glm::xz(full.at(2))) * k_Share, 1e-3f);
	EXPECT_EQ(registry.Get<Transform>(creature).scale, own);
}
