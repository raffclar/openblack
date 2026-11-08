/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/PortableRotations.h"
#include "Common/X87Angle.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingFacing.h"
#include "ECS/Registry.h"
#include "Physics/Body.h"

using namespace openblack;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LivingFacing, AVillagerAndAnAnimalKeepTheirAngleAndOtherThingsKeepNone)
{
	ecs::Registry registry;
	const auto villager = registry.Create();
	registry.Assign<ecs::components::Villager>(villager);
	registry.Assign<ecs::components::WallHug>(villager, ecs::components::WallHug {.yAngle = 1.25f, .speed = 0.0f});
	const auto animal = registry.Create();
	auto& beast = registry.Assign<ecs::components::Animal>(animal);
	beast.heading = -2.5f;
	// An animal that walks round what is in its way keeps its angle as its heading, not with its walk
	registry.Assign<ecs::components::WallHug>(animal, ecs::components::WallHug {.yAngle = 0.5f, .speed = 0.0f});
	const auto walker = registry.Create();
	registry.Assign<ecs::components::WallHug>(walker, ecs::components::WallHug {.yAngle = 0.75f, .speed = 0.0f});
	const auto thing = registry.Create();

	EXPECT_EQ(ecs::living_facing::KeptAngleOf(registry, villager), 1.25f);
	EXPECT_EQ(ecs::living_facing::KeptAngleOf(registry, animal), -2.5f);
	EXPECT_FALSE(ecs::living_facing::KeptAngleOf(registry, walker).has_value());
	EXPECT_FALSE(ecs::living_facing::KeptAngleOf(registry, thing).has_value());
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LivingFacing, TheWorldAxesAreTheUprightTurnOfTheGameTurnedAQuarterToTheModel)
{
	constexpr float k_Scale = 1.3f;
	for (int step = 0; step <= 100; ++step)
	{
		const float angle = -3.1f + 0.062f * static_cast<float>(step);
		const auto axes = ecs::living_facing::WorldAxesOf(angle, k_Scale);
		EXPECT_EQ(axes, physics::QuarterTurned(x87::GameUprightTurnScaled(angle, k_Scale)));
		// The quarter turn keeps the up axis of the game as it is
		EXPECT_EQ(axes[1], glm::vec3(0.0f, k_Scale, 0.0f));
		// The axes point as the villager's model points when it faces the same angle
		const auto drawn = portable_math::EulerAngleY(-angle - glm::radians(90.0f));
		for (glm::length_t column = 0; column < 3; ++column)
		{
			EXPECT_LT(glm::length(axes[column] / k_Scale - glm::vec3(drawn[column])), 1.0e-6f);
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LivingFacing, TheWorldAxesHaveNoScaleWhenTheScaleIsOneOrNotANumber)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(ecs::living_facing::WorldAxesOf(0.8f, nan), physics::QuarterTurned(x87::GameUprightTurnScaled(0.8f, 1.0f)));
	EXPECT_EQ(ecs::living_facing::WorldAxesOf(0.8f, 1.0f), physics::QuarterTurned(x87::GameUprightTurnScaled(0.8f, 1.0f)));
	// The angle 0 gives the size on each axis before the quarter turn
	EXPECT_EQ(ecs::living_facing::WorldAxesOf(0.0f, 2.0f), physics::QuarterTurned(glm::mat3(2.0f)));
}
