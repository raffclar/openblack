/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/CreatureShadow.h"

using namespace openblack::graphics;

TEST(CreatureShadow, FadesBetween50And80Radii)
{
	EXPECT_EQ(CreatureShadow::Alpha(10.0f, 1.0f), 255);
	EXPECT_EQ(CreatureShadow::Alpha(49.9f, 1.0f), 255);
	EXPECT_EQ(CreatureShadow::Alpha(65.0f, 1.0f), 127);
	EXPECT_EQ(CreatureShadow::Alpha(80.0f, 1.0f), 0);
	// In radii of the creature
	EXPECT_EQ(CreatureShadow::Alpha(650.0f, 10.0f), 127);
	EXPECT_EQ(CreatureShadow::Alpha(10.0f, 0.0f), 0);
}

TEST(CreatureShadow, TheLightIsKeptAtLeast45DegreesUp)
{
	const glm::vec3 centre {100.0f, 10.0f, 100.0f};
	// A low sun far away, 10 degrees up
	const auto low = CreatureShadow::LightPoint(centre + glm::vec3(1000.0f, 176.3f, 0.0f), centre, 2.0f);
	const auto towards = low - centre;
	EXPECT_NEAR(towards.y, 1000.0f, 1e-2f);
	EXPECT_NEAR(towards.x, 1000.0f, 1e-2f);
	// A high one is left as it is
	const auto high = CreatureShadow::LightPoint(centre + glm::vec3(10.0f, 500.0f, 0.0f), centre, 2.0f);
	EXPECT_NEAR(high.y - centre.y, 500.0f, 1e-3f);
}

TEST(CreatureShadow, ANearLightIsPushedOutToThreeRadii)
{
	const glm::vec3 centre {0.0f, 0.0f, 0.0f};
	const auto pushed = CreatureShadow::LightPoint(glm::vec3(1.0f, 0.5f, 0.0f), centre, 2.0f);
	const auto across = std::sqrt((pushed.x * pushed.x) + (pushed.z * pushed.z));
	// Out to 6 units, then raised to 45 degrees
	EXPECT_GT(across, 5.0f);
	EXPECT_NEAR(pushed.y, across, 1e-4f);
	// Straight overhead, it is nudged to a side first
	const auto overhead = CreatureShadow::LightPoint(glm::vec3(0.0f, 1.0f, 0.0f), centre, 2.0f);
	EXPECT_GT(overhead.x, 0.0f);
	EXPECT_GT(overhead.z, 0.0f);
}

TEST(CreatureShadow, TheCreaturesCentreFallsInTheMiddleOfItsCell)
{
	const glm::vec3 centre {50.0f, 20.0f, 60.0f};
	const auto shadow = CreatureShadow::Compute(CreatureShadow::Caster::Creature, centre, 5.0f, 10.0f,
	                                            centre + glm::vec3(-100.0f, 300.0f, -100.0f),
	                                            centre + glm::vec3(0.0f, 30.0f, -50.0f), 3, 8, true, false);
	ASSERT_TRUE(shadow.has_value());
	EXPECT_FLOAT_EQ(shadow->strength, 1.0f);
	const auto coordinate = shadow->receiverMatrix * glm::vec4(centre, 1.0f);
	EXPECT_NEAR(coordinate.x, 3.5f / 8.0f, 1e-4f);
	EXPECT_NEAR(coordinate.y, 0.5f, 1e-4f);
	EXPECT_NEAR(coordinate.z, 0.0f, 1e-3f);
	// The ground under it is further along the light, so is shaded
	const auto ground = shadow->receiverMatrix * glm::vec4(centre.x, 10.0f, centre.z, 1.0f);
	EXPECT_GT(ground.z, shadow->startDepth);
}

TEST(CreatureShadow, FarCamerasSeeNoShadow)
{
	const glm::vec3 centre {0.0f, 5.0f, 0.0f};
	EXPECT_FALSE(CreatureShadow::Compute(CreatureShadow::Caster::Creature, centre, 1.0f, 0.0f, glm::vec3(0.0f, 100.0f, 10.0f),
	                                     glm::vec3(0.0f, 100.0f, 0.0f), 0, 8, true, false)
	                 .has_value());
}

TEST(CreatureShadow, ACreatureCastsFromTheScenesLightBroughtRound)
{
	const glm::vec3 centre {100.0f, 10.0f, 100.0f};
	const glm::vec3 light {1100.0f, 186.3f, 100.0f};
	const auto from = CreatureShadow::CastFrom(CreatureShadow::Caster::Creature, light, centre, 2.0f);
	const auto expected = CreatureShadow::LightPoint(light, centre, 2.0f);
	EXPECT_FLOAT_EQ(from.x, expected.x);
	EXPECT_FLOAT_EQ(from.y, expected.y);
	EXPECT_FLOAT_EQ(from.z, expected.z);
}

TEST(CreatureShadow, AHighDetailVillagerCastsFromTheFixedSunWhateverTheLight)
{
	const glm::vec3 centre {1494.9f, 1.0f, 2093.0f};
	for (const auto& light : {glm::vec3(0.0f, 1000.0f, 0.0f), glm::vec3(2000.0f, 10.0f, 2000.0f), centre})
	{
		const auto from = CreatureShadow::CastFrom(CreatureShadow::Caster::HighDetailVillager, light, centre, 0.9f);
		EXPECT_FLOAT_EQ(from.x, -500000.0f);
		EXPECT_FLOAT_EQ(from.y, 500000.0f);
		EXPECT_FLOAT_EQ(from.z, -500000.0f);
	}
}

TEST(CreatureShadow, OnlyACreaturesShadowFallsOnObjects)
{
	EXPECT_TRUE(CreatureShadow::FallsOnObjects(CreatureShadow::Caster::Creature));
	EXPECT_FALSE(CreatureShadow::FallsOnObjects(CreatureShadow::Caster::HighDetailVillager));

	const glm::vec3 centre {50.0f, 20.0f, 60.0f};
	const auto camera = centre + glm::vec3(0.0f, 3.0f, -5.0f);
	const auto light = centre + glm::vec3(-100.0f, 300.0f, -100.0f);
	const auto creature =
	    CreatureShadow::Compute(CreatureShadow::Caster::Creature, centre, 1.0f, 19.0f, light, camera, 0, 8, true, false);
	const auto villager = CreatureShadow::Compute(CreatureShadow::Caster::HighDetailVillager, centre, 1.0f, 19.0f, light,
	                                              camera, 1, 8, true, false);
	ASSERT_TRUE(creature.has_value());
	ASSERT_TRUE(villager.has_value());
	EXPECT_TRUE(creature->onObjects);
	EXPECT_FALSE(villager->onObjects);
}

TEST(CreatureShadow, AHighDetailVillagersShadowFallsAwayFromTheFixedSun)
{
	const glm::vec3 centre {50.0f, 20.0f, 60.0f};
	const auto shadow =
	    CreatureShadow::Compute(CreatureShadow::Caster::HighDetailVillager, centre, 1.0f, 18.0f, glm::vec3(0.0f, 1000.0f, 0.0f),
	                            centre + glm::vec3(0.0f, 3.0f, -5.0f), 2, 8, true, false);
	ASSERT_TRUE(shadow.has_value());
	// The ground two below the centre, one along x and z from it: where a ray from the sun through the centre lands
	const auto ray = glm::normalize(centre - CreatureShadow::k_FixedSun);
	const auto landing = centre + ray * (2.0f / -ray.y);
	EXPECT_NEAR(landing.x - centre.x, 2.0f, 1e-2f);
	EXPECT_NEAR(landing.z - centre.z, 2.0f, 1e-2f);
	const auto atCentre = shadow->receiverMatrix * glm::vec4(centre, 1.0f);
	const auto atLanding = shadow->receiverMatrix * glm::vec4(landing, 1.0f);
	EXPECT_NEAR(atLanding.x, atCentre.x, 1e-4f);
	EXPECT_NEAR(atLanding.y, atCentre.y, 1e-4f);
	EXPECT_GT(atLanding.z, shadow->startDepth);
}

TEST(CreatureShadow, AHighDetailVillagersShadowFadesWithDistanceAsACreaturesDoes)
{
	const glm::vec3 centre {0.0f, 1.0f, 0.0f};
	EXPECT_FALSE(CreatureShadow::Compute(CreatureShadow::Caster::HighDetailVillager, centre, 1.0f, 0.0f,
	                                     glm::vec3(0.0f, 100.0f, 0.0f), glm::vec3(0.0f, 80.0f, 0.0f), 0, 8, true, false)
	                 .has_value());
	EXPECT_TRUE(CreatureShadow::Compute(CreatureShadow::Caster::HighDetailVillager, centre, 1.0f, 0.0f,
	                                    glm::vec3(0.0f, 100.0f, 0.0f), glm::vec3(0.0f, 40.0f, 0.0f), 0, 8, true, false)
	                .has_value());
}
