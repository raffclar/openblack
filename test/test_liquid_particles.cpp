/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LiquidParticleSystem.h"
#include "Particles/LiquidParticles.h"
#include "Particles/ParticleDrawFrame.h"

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

TEST(LiquidParticleSystem, EachOtherPlayerHasAPoolOfItsOwnWithItsOwnLimit)
{
	QueuedRandom random {.numbers = {5, 7}};
	ecs::systems::LiquidParticleSystem pool(Sources(random, false));
	pool.AddForPlayer({.player = PlayerNames::PLAYER_TWO}, glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f), 10.0f, 0x80FF0000u,
	                  Kind::Mote);
	pool.SetPlayerPoint({.player = PlayerNames::PLAYER_TWO}, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(pool.Particles().empty());
	ASSERT_EQ(pool.PlayerParticles({.player = PlayerNames::PLAYER_TWO}).size(), 1u);
	EXPECT_EQ(pool.PlayerParticles({.player = PlayerNames::PLAYER_TWO})[0].variant, 5);
	EXPECT_TRUE(pool.PlayerParticles({.player = PlayerNames::PLAYER_THREE}).empty());
	// The shared pool full leaves room in the other pools
	for (size_t i = 0; i < liquid_particles::k_MostParticles; ++i)
	{
		pool.Add(glm::vec3(0.0f), glm::vec3(0.0f), 1.0f, 0xFFFFFFFFu, Kind::Dust);
	}
	pool.AddForPlayer({.player = PlayerNames::PLAYER_TWO}, glm::vec3(0.0f), glm::vec3(0.0f), 10.0f, 0x80FF0000u, Kind::Mote);
	EXPECT_EQ(pool.PlayerParticles({.player = PlayerNames::PLAYER_TWO}).size(), 2u);
	// The other pools move on with the shared pool: a mote lives two seconds
	pool.UpdateFrame(2.5f, false);
	EXPECT_TRUE(pool.PlayerParticles({.player = PlayerNames::PLAYER_TWO}).empty());
	pool.AddForPlayer({.player = PlayerNames::PLAYER_TWO}, glm::vec3(0.0f), glm::vec3(0.0f), 10.0f, 0x80FF0000u, Kind::Mote);
	pool.Clear();
	EXPECT_TRUE(pool.PlayerParticles({.player = PlayerNames::PLAYER_TWO}).empty());
}

TEST(LiquidParticles, AFramesMillisecondsAreTakenToSecondsAsTheGameTakesThem)
{
	// Times a thousandth in single precision, not divided by a thousand
	EXPECT_EQ(std::bit_cast<uint32_t>(liquid_particles::FrameSeconds(33)), std::bit_cast<uint32_t>(33.0f * 0.001f));
	EXPECT_NE(std::bit_cast<uint32_t>(liquid_particles::FrameSeconds(5)), std::bit_cast<uint32_t>(5.0f / 1000.0f));
	EXPECT_EQ(liquid_particles::FrameSeconds(0), 0.0f);
}

TEST(LiquidParticleSystem, ThePoolOfAnotherPlayerShowsTheSymbolOfItsGodLast)
{
	QueuedRandom random {.numbers = {5, 7, 9}};
	ecs::systems::LiquidParticleSystem pool(Sources(random, false));
	const liquid_particles::Interface khazar {.player = PlayerNames::PLAYER_TWO};
	const liquid_particles::Interface lethys {.player = PlayerNames::PLAYER_THREE};
	const glm::vec3 point {1.0f, 2.0f, 3.0f};
	pool.AddForPlayer(khazar, point, glm::vec3(0.0f), 10.0f, 0x80FF0000u, Kind::Mote);
	pool.AddForPlayer(khazar, point, glm::vec3(0.0f), 10.0f, 0x80FF0000u, Kind::Mote);
	pool.SetPlayerPoint(khazar, point);
	pool.AddForPlayer(lethys, glm::vec3(9.0f), glm::vec3(0.0f), 10.0f, 0x800000FFu, Kind::Mote);
	pool.SetPlayerPoint(lethys, glm::vec3(9.0f));
	const float turn = 20.0f * std::bit_cast<float>(0x3A03126Fu);

	// Lethys shows no symbol; Khazar's symbol is drawn after the two motes, as one thing at the point
	pool.TurnPlayerSymbols(20, glm::vec3(0.0f), [&](liquid_particles::Interface interface) { return interface == khazar; });
	particles::draw::Frame frame;
	pool.CollectDrawFrame(frame);
	ASSERT_EQ(frame.groups.size(), 2u);
	EXPECT_EQ(frame.groups[0].origin, point);
	ASSERT_EQ(frame.groups[0].itemCount, 3u);
	EXPECT_EQ(frame.groups[1].itemCount, 1u);
	const auto& symbol = frame.sprites[2];
	EXPECT_EQ(glm::vec3(symbol.positionHalfWidth), point);
	EXPECT_EQ(symbol.positionHalfWidth.w, 3.0f);
	EXPECT_EQ(symbol.shape.y, turn);
	EXPECT_EQ(symbol.colour, glm::vec4(0.0f, 0.0f, 0.0f, 0x30 / 255.0f));
	const auto& material = frame.materials[frame.spriteMaterials[2]];
	EXPECT_EQ(material.texture, entt::hashed_string("raw/PlayersSymbols").value());
	EXPECT_EQ(material.alphaTexture, entt::hashed_string("raw/PlayersSymbolsa").value());
	EXPECT_EQ(material.mode, graphics::render_modes::Mode::AlphaTexturedAlphaNz);

	// When both show it, they share one angle that turns once for each, in the order they draw: Lethys's pool is
	// farther from the camera, so it draws and turns first
	pool.TurnPlayerSymbols(20, glm::vec3(0.0f), [](liquid_particles::Interface /*interface*/) { return true; });
	frame.Clear();
	pool.CollectDrawFrame(frame);
	ASSERT_EQ(frame.sprites.size(), 5u);
	EXPECT_EQ(frame.sprites[2].shape.y, turn + turn + turn);
	EXPECT_EQ(frame.sprites[4].shape.y, turn + turn);

	// From the other side, Khazar's pool is the farther one and turns first
	pool.TurnPlayerSymbols(20, glm::vec3(20.0f), [](liquid_particles::Interface /*interface*/) { return true; });
	frame.Clear();
	pool.CollectDrawFrame(frame);
	ASSERT_EQ(frame.sprites.size(), 5u);
	EXPECT_EQ(frame.sprites[2].shape.y, turn + turn + turn + turn);
	EXPECT_EQ(frame.sprites[4].shape.y, turn + turn + turn + turn + turn);

	// A pool with no particles left shows no symbol
	pool.UpdateFrame(2.5f, false);
	pool.TurnPlayerSymbols(20, glm::vec3(0.0f), [](liquid_particles::Interface /*interface*/) { return true; });
	frame.Clear();
	pool.CollectDrawFrame(frame);
	EXPECT_TRUE(frame.sprites.empty());
}
