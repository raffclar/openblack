/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LiquidParticleSystem.h"
#include "Particles/LiquidParticles.h"

using namespace openblack;
using liquid_particles::Kind;
using liquid_particles::Particle;

namespace
{
/// A land that is flat at a height
liquid_particles::AltitudeAt FlatLand(float height)
{
	return [height](int32_t /*x*/, int32_t /*z*/) { return height; };
}

/// Gives the queued numbers in order, then zeros, and counts the draws
struct QueuedRandom
{
	std::vector<int32_t> numbers;
	size_t draws {0};
	liquid_particles::CrtRandom Get()
	{
		return [this]() {
			const auto value = draws < numbers.size() ? numbers[draws] : 0;
			++draws;
			return value;
		};
	}
};

ecs::systems::LiquidParticleSystem::Sources Sources(QueuedRandom& random, bool drawDue)
{
	return {
	    .random = random.Get(),
	    .altitude = FlatLand(0.0f),
	    .drawDue = [drawDue](frame_pace::Pace& /*pace*/) { return drawDue; },
	};
}
} // namespace

TEST(LiquidParticles, DustGrowsThenShrinksOverASecond)
{
	Particle puff {.velocity = glm::vec3(1.0f, 0.0f, 0.0f), .size = 4.0f, .variant = 15, .kind = Kind::Dust};
	EXPECT_FLOAT_EQ(liquid_particles::DrawnSize(puff), 0.0f);
	puff.age = 0.0625f;
	EXPECT_NEAR(liquid_particles::DrawnSize(puff), 4.0f * 0.9375f * 0.5f, 1e-5f);
	puff.age = 0.5f;
	EXPECT_NEAR(liquid_particles::DrawnSize(puff), 2.0f, 1e-5f);
	EXPECT_EQ(liquid_particles::Frame(puff), 16 + ((15 + 1) & 15));
	EXPECT_TRUE(liquid_particles::Step(puff, 0.25f, {}, {}));
	EXPECT_FLOAT_EQ(puff.position.x, 0.25f);
	// At an age of exactly its life it stays, and it goes only after that
	EXPECT_TRUE(liquid_particles::Step(puff, 0.25f, {}, {}));
	EXPECT_FALSE(liquid_particles::Step(puff, 0.0625f, {}, {}));
}

TEST(LiquidParticles, ADropFallsThenSlidesOnTheLand)
{
	Particle drop {.position = {0.0f, 1.0f, 0.0f}, .velocity = {2.0f, 0.0f, 0.0f}, .size = 0.5f, .kind = Kind::Liquid};
	QueuedRandom random {.numbers = {150, 20}};
	EXPECT_FLOAT_EQ(liquid_particles::DrawnSize(drop), 0.5f);
	drop.variant = liquid_particles::LiquidFrame(13);
	EXPECT_EQ(liquid_particles::Frame(drop), 32 + 5);
	// It falls under gravity and stays in the air
	EXPECT_TRUE(liquid_particles::Step(drop, 0.1f, FlatLand(0.0f), random.Get()));
	EXPECT_FLOAT_EQ(drop.velocity.y, -0.981f);
	EXPECT_FLOAT_EQ(drop.position.x, 0.2f);
	EXPECT_FALSE(drop.onLand);
	EXPECT_EQ(random.draws, 0u);
	// It meets the land: it lies on it, lives anew and slides by two draws of 0 to 99
	EXPECT_TRUE(liquid_particles::Step(drop, 1.0f, FlatLand(0.0f), random.Get()));
	EXPECT_TRUE(drop.onLand);
	EXPECT_EQ(random.draws, 2u);
	EXPECT_FLOAT_EQ(drop.position.y, 0.0f);
	EXPECT_FLOAT_EQ(drop.age, 0.0f);
	EXPECT_FLOAT_EQ(drop.velocity.x, 50.0f * (0.5f * liquid_particles::k_SlidePerDraw) - 15.0f);
	EXPECT_FLOAT_EQ(drop.velocity.z, 20.0f * (0.5f * liquid_particles::k_SlidePerDraw) - 15.0f);
	EXPECT_FLOAT_EQ(drop.velocity.y, 0.0f);
	// On the land it keeps a quarter of its speed each second and follows the land
	const auto vz = drop.velocity.z;
	EXPECT_TRUE(liquid_particles::Step(drop, 1.0f, FlatLand(2.0f), random.Get()));
	EXPECT_FLOAT_EQ(drop.velocity.z, vz * 0.25f);
	EXPECT_FLOAT_EQ(drop.position.y, 2.0f);
	// Three seconds after it met the land it is gone
	EXPECT_TRUE(liquid_particles::Step(drop, 2.0f, FlatLand(2.0f), random.Get()));
	EXPECT_FALSE(liquid_particles::Step(drop, 0.5f, FlatLand(2.0f), random.Get()));
}

TEST(LiquidParticles, ADropAsksTheLandAtItsPointInFixedPoint)
{
	EXPECT_EQ(liquid_particles::ToMapUnits(10.0f), 0x10000);
	EXPECT_EQ(liquid_particles::ToMapUnits(0.0f), 0);
	EXPECT_EQ(liquid_particles::ToMapUnits(-10.0f), -0x10000);
	Particle drop {.position = {25.0f, 5.0f, 35.0f}, .kind = Kind::Liquid};
	int32_t askedX = 0;
	int32_t askedZ = 0;
	const liquid_particles::AltitudeAt altitude = [&askedX, &askedZ](int32_t x, int32_t z) {
		askedX = x;
		askedZ = z;
		return 0.0f;
	};
	EXPECT_TRUE(liquid_particles::Step(drop, 0.0f, altitude, {}));
	EXPECT_EQ(askedX, liquid_particles::ToMapUnits(25.0f));
	EXPECT_EQ(askedZ, liquid_particles::ToMapUnits(35.0f));
}

TEST(LiquidParticles, OnlyKindsOtherThanLiquidDrawAStartFrame)
{
	QueuedRandom random {.numbers = {35, 7}};
	EXPECT_EQ(liquid_particles::StartVariant(Kind::Liquid, random.Get()), 0);
	EXPECT_EQ(random.draws, 0u);
	EXPECT_EQ(liquid_particles::StartVariant(Kind::Dust, random.Get()), 35 % 16);
	EXPECT_EQ(liquid_particles::StartVariant(Kind::Mote, random.Get()), 7);
	EXPECT_EQ(random.draws, 2u);
	EXPECT_FLOAT_EQ(liquid_particles::Life(Kind::Liquid), 3.0f);
	EXPECT_FLOAT_EQ(liquid_particles::Life(Kind::Dust), 1.0f);
	EXPECT_FLOAT_EQ(liquid_particles::Life(Kind::Smoke), 2.0f);
	EXPECT_FLOAT_EQ(liquid_particles::Life(Kind::MoteReversed), 2.0f);
}

TEST(LiquidParticles, AParticleThatGoesTakesTheLastInItsPlace)
{
	std::vector<Particle> pool {
	    {.age = 0.95f, .kind = Kind::Dust},  {.size = 1.0f, .kind = Kind::Smoke},
	    {.size = 2.0f, .kind = Kind::Smoke}, {.size = 3.0f, .age = 0.95f, .kind = Kind::Dust},
	    {.size = 4.0f, .kind = Kind::Smoke},
	};
	liquid_particles::StepPool(pool, 0.1f, {}, {});
	// The first went and the last took its place, and that one stepped too. The fourth went and the pool ended there.
	ASSERT_EQ(pool.size(), 3u);
	EXPECT_FLOAT_EQ(pool[0].size, 4.0f);
	EXPECT_FLOAT_EQ(pool[0].age, 0.1f);
	EXPECT_FLOAT_EQ(pool[1].size, 1.0f);
	EXPECT_FLOAT_EQ(pool[2].size, 2.0f);
}

TEST(LiquidParticleSystem, ThePoolHoldsAtMostItsLimit)
{
	QueuedRandom random;
	ecs::systems::LiquidParticleSystem pool(Sources(random, false));
	for (size_t i = 0; i < liquid_particles::k_MostParticles + 10; ++i)
	{
		pool.Add(glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, 0xFFFFFFFFu, Kind::Dust);
	}
	EXPECT_EQ(pool.Particles().size(), liquid_particles::k_MostParticles);
	// A particle there is no room for draws no number
	EXPECT_EQ(random.draws, liquid_particles::k_MostParticles);
	pool.Clear();
	EXPECT_TRUE(pool.Particles().empty());
}

TEST(LiquidParticleSystem, DropsTakeNewFramesAfterTheyMoveOnWhenTheFrameDrawsThem)
{
	QueuedRandom random {.numbers = {9, 12, 6}};
	ecs::systems::LiquidParticleSystem pool(Sources(random, true));
	pool.Add(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f), 1.0f, 0xFF00FF00u, Kind::Liquid);
	pool.Add(glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, 0xFF00FF00u, Kind::Smoke);
	pool.Add(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f), 1.0f, 0xFF00FF00u, Kind::Liquid);
	// Only the smoke drew a number as it came
	EXPECT_EQ(random.draws, 1u);
	pool.UpdateFrame(0.01f, true);
	ASSERT_EQ(pool.Particles().size(), 3u);
	EXPECT_EQ(pool.Particles()[0].variant, 12 & 7);
	EXPECT_EQ(pool.Particles()[1].variant, 9 % 16);
	EXPECT_EQ(pool.Particles()[2].variant, 6 & 7);
	// A frame that does not draw the drops leaves their frames
	pool.UpdateFrame(0.01f, false);
	EXPECT_EQ(random.draws, 3u);
}
