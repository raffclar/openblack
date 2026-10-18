/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the jellyfish in the deep water

#include <cmath>
#include <cstdint>

#include <array>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/Underwater.h"
#include "Graphics/UnderwaterJellyfish.h"

namespace jelly = openblack::graphics::underwater::jellyfish;

namespace
{

constexpr float k_SeaLevel = 0.0f;

/// A sea bed this deep everywhere
jelly::DepthAt Flat(float depth)
{
	return [depth](glm::vec2) { return depth; };
}

} // namespace

TEST(UnderwaterJellyfish, NoneInTheShallows)
{
	EXPECT_TRUE(jelly::Around({0.0f, 0.0f}, 10.0f, k_SeaLevel, Flat(0.45f)).empty());
	EXPECT_TRUE(jelly::Around({0.0f, 0.0f}, 10.0f, k_SeaLevel, Flat(jelly::k_LeastDepth - 0.1f)).empty());
	EXPECT_FALSE(jelly::Around({0.0f, 0.0f}, 10.0f, k_SeaLevel, Flat(70.0f)).empty());
}

TEST(UnderwaterJellyfish, OnlyWhereTheSeaBedIsDeep)
{
	// Deep water on one side of x = 0 only
	const jelly::DepthAt half = [](glm::vec2 point) { return point.x > 0.0f ? 70.0f : 2.0f; };
	for (const auto& found : jelly::Around({0.0f, 0.0f}, 5.0f, k_SeaLevel, half))
	{
		EXPECT_GT(found.position.x, 0.0f);
	}
}

TEST(UnderwaterJellyfish, StayInTheWaterAndInRange)
{
	const float depth = 60.0f;
	for (const float seconds : {0.0f, 17.0f, 333.0f, 4000.0f})
	{
		const auto found = jelly::Around({500.0f, -200.0f}, seconds, k_SeaLevel, Flat(depth));
		ASSERT_LE(found.size(), jelly::k_Most);
		for (const auto& one : found)
		{
			EXPECT_LE(one.position.y, k_SeaLevel - jelly::k_TopDepth + 1e-3f);
			EXPECT_GE(one.position.y - one.size, k_SeaLevel - depth + jelly::k_BedGap - 1e-3f);
			EXPECT_LE(glm::distance(glm::vec2(one.position.x, one.position.z), glm::vec2(500.0f, -200.0f)), jelly::k_Range);
			EXPECT_GT(one.shows, 0.0f);
			EXPECT_LE(one.shows, 1.0f);
			EXPECT_GE(one.size, jelly::k_LeastSize);
			EXPECT_LE(one.size, jelly::k_MostSize);
		}
	}
}

TEST(UnderwaterJellyfish, TheyRiseSlowlyAndTheNearestComeFirst)
{
	const auto before = jelly::Around({0.0f, 0.0f}, 100.0f, k_SeaLevel, Flat(70.0f));
	const auto after = jelly::Around({0.0f, 0.0f}, 101.0f, k_SeaLevel, Flat(70.0f));
	ASSERT_FALSE(before.empty());
	uint32_t rose = 0;
	for (const auto& a : before)
	{
		for (const auto& b : after)
		{
			if (a.position.x == b.position.x && a.position.z == b.position.z)
			{
				const float risen = b.position.y - a.position.y;
				// One that starts again near the sea bed drops; the rest rise by the rise of one second
				if (risen > 0.0f)
				{
					EXPECT_NEAR(risen, jelly::k_Rise, 1e-3f);
					++rose;
				}
			}
		}
	}
	EXPECT_GT(rose, 0u);
	for (size_t i = 1; i < before.size(); ++i)
	{
		EXPECT_LE(glm::length(glm::vec2(before[i - 1].position.x, before[i - 1].position.z)),
		          glm::length(glm::vec2(before[i].position.x, before[i].position.z)) + 1e-3f);
	}
}

TEST(UnderwaterJellyfish, TheMeshHasEveryPart)
{
	std::vector<jelly::Vertex> vertices;
	std::vector<uint16_t> indices;
	jelly::Mesh(vertices, indices);
	const uint32_t shell = (jelly::k_ShellRings + 1) * (jelly::k_ShellSides + 1);
	const uint32_t tentacle = (jelly::k_TentacleSegments + 1) * 2;
	EXPECT_EQ(vertices.size(), 2 * shell + (jelly::k_InnerTentacles + jelly::k_OuterTentacles) * tentacle + 4);
	EXPECT_EQ(indices.size() % 3, 0u);
	for (const auto index : indices)
	{
		EXPECT_LT(index, vertices.size());
	}
	// The outer shell comes last, so that it glows over the rest
	EXPECT_EQ(vertices.back().data.z, static_cast<float>(jelly::Part::OuterShell));
	EXPECT_EQ(vertices.front().data.z, static_cast<float>(jelly::Part::InnerShell));
}

TEST(UnderwaterJellyfish, TheWaterPathIsThePartOfTheWayUnderTheSea)
{
	const glm::vec3 point {0.0f, -10.0f, 0.0f};
	// With the eye under the water, all of the way
	EXPECT_FLOAT_EQ(jelly::WaterPath({30.0f, -10.0f, 40.0f}, point, 0.0f), 50.0f);
	// With the eye above the water, the part under the sea's level: here half of it
	EXPECT_NEAR(jelly::WaterPath({0.0f, 10.0f, 0.0f}, point, 0.0f), 10.0f, 1e-4f);
	EXPECT_NEAR(jelly::WaterPath({30.0f, 10.0f, 40.0f}, point, 0.0f), 0.5f * std::sqrt(2900.0f), 1e-3f);
}

TEST(UnderwaterJellyfish, TheirLightFadesFasterThanTheWatersLight)
{
	// Near the eye it is all there
	const glm::vec3 near = jelly::GlowThroughWater(0.0f);
	EXPECT_NEAR(glm::length(near - glm::vec3(1.0f)), 0.0f, 1e-5f);
	float last = 1.0f;
	for (const float path : {5.0f, 10.0f, 20.0f, 40.0f})
	{
		const glm::vec3 left = jelly::GlowThroughWater(path);
		EXPECT_LT(left.b, last);
		last = left.b;
		// The water scatters it out of its way too, so less of it comes through than of the scene's light
		const glm::vec3 water = openblack::graphics::underwater::Transmittance(path);
		EXPECT_LT(left.r, water.r);
		EXPECT_LT(left.g, water.g);
		EXPECT_LT(left.b, water.b);
	}
	// Close by it shows: more than a tenth of the blue at 10 units
	EXPECT_GT(jelly::GlowThroughWater(10.0f).b, 0.1f);
}

TEST(UnderwaterJellyfish, NoneOfTheirLightComesToTheSurface)
{
	// The shortest way from a jellyfish to the surface is straight up from its top. The brightest glow of the brightest
	// flash and the brightest halo, through that water, are less than one step of an 8-bit colour, in each colour.
	const glm::vec3 left = jelly::GlowThroughWater(jelly::k_TopDepth);
	for (const float hue : {0.0f, 0.5f, 1.0f})
	{
		const glm::vec3 brightest = jelly::LightColour(hue) * jelly::Emission(1.0f, 1.0f) * (1.0f + jelly::k_HaloStrength);
		const glm::vec3 lit = jelly::LightColour(hue) * jelly::Emission(1.0f, 1.0f) * jelly::LightAt(jelly::k_TopDepth);
		const glm::vec3 pool = jelly::LightColour(hue) * jelly::Emission(1.0f, 1.0f) * jelly::PoolAt(jelly::k_TopDepth);
		for (int colour = 0; colour < 3; ++colour)
		{
			// With a margin: at most 0.7 of the step
			EXPECT_LT(brightest[colour] * left[colour], 0.7f / 255.0f);
			// The light that falls on the water at the surface
			EXPECT_LT(lit[colour], 0.7f / 255.0f);
			EXPECT_LT(pool[colour], 0.7f / 255.0f);
		}
	}
	// And they are all at least that deep
	for (const float seconds : {0.0f, 50.0f, 900.0f})
	{
		for (const auto& one : jelly::Around({0.0f, 0.0f}, seconds, k_SeaLevel, Flat(jelly::k_LeastDepth + 2.0f)))
		{
			EXPECT_LE(one.position.y, k_SeaLevel - jelly::k_TopDepth + 1e-3f);
		}
	}
}

TEST(UnderwaterJellyfish, TheirLightIsBlueGreenAndSoft)
{
	for (const float hue : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
	{
		const glm::vec3 colour = jelly::LightColour(hue);
		EXPECT_LT(colour.r, 0.1f);
		EXPECT_GT(colour.g, 0.5f);
		EXPECT_GT(colour.b, 0.5f);
		EXPECT_LE(glm::max(colour.r, glm::max(colour.g, colour.b)), 1.0f);
	}
	// The colour that goes furthest through the water is the most of the light at rest
	EXPECT_GT(jelly::LightColour(0.0f).b, jelly::LightColour(0.0f).g);
}

TEST(UnderwaterJellyfish, TheyAreDimAtRestPulseAndFlashBrighter)
{
	const float rest = jelly::Emission(0.0f, 0.0f);
	const float squeezed = jelly::Emission(1.0f, 0.0f);
	const float flash = jelly::Emission(0.0f, 1.0f);
	EXPECT_GT(rest, 0.0f);
	EXPECT_GT(squeezed, rest);
	EXPECT_GT(flash, 2.5f * rest);
	EXPECT_GT(flash, 1.5f * squeezed);
	for (const float squeeze : {0.0f, 0.5f, 1.0f})
	{
		for (const float strength : {0.0f, 0.5f, 1.0f, 3.0f})
		{
			EXPECT_LE(jelly::Emission(squeeze, strength), jelly::k_MostGlow);
		}
	}
	EXPECT_LE(jelly::k_MostGlow, 1.0f);
	// The squeeze of the beat: none at its start, all at the quick squeeze, then a slow release
	EXPECT_FLOAT_EQ(jelly::Squeeze(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(jelly::Squeeze(0.16f), 1.0f);
	EXPECT_GT(jelly::Squeeze(0.5f), 0.0f);
	EXPECT_LT(jelly::Squeeze(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(jelly::Squeeze(0.95f), 0.0f);
	const jelly::Jellyfish one {.phase = 0.3f, .seed = 0.5f};
	for (const float seconds : {0.0f, 1.3f, 77.0f})
	{
		const float cycle = jelly::BeatCycle(one, seconds);
		EXPECT_GE(cycle, 0.0f);
		EXPECT_LT(cycle, 1.0f);
	}
}

TEST(UnderwaterJellyfish, TheirLightFallsOffWithTheSquareOfTheDistanceAndTheWater)
{
	EXPECT_NEAR(jelly::LightAt(0.0f).b, 1.0f, 1e-5f);
	float last = 1.0f;
	for (const float distance : {0.2f, 0.5f, 1.0f, 2.0f, 4.0f})
	{
		const glm::vec3 lit = jelly::LightAt(distance);
		EXPECT_LT(lit.b, last);
		last = lit.b;
		// The water takes the red first, as the rest of the water
		EXPECT_LT(lit.r, lit.g);
		EXPECT_LT(lit.g, lit.b);
		// Not more than the square of the distance alone
		const float spread = jelly::k_LightRadius * jelly::k_LightRadius;
		EXPECT_LE(lit.b, spread / (distance * distance + spread));
	}
	// It makes the water bright for about half a metre to one metre: half of it at half a metre, a little at two metres
	const float metre = openblack::graphics::underwater::k_UnitsPerMetre;
	EXPECT_GT(jelly::LightAt(0.5f * metre).b, 0.4f);
	EXPECT_GT(jelly::LightAt(1.0f * metre).b, 0.15f);
	EXPECT_LT(jelly::LightAt(2.0f * metre).b, 0.05f);
	// Past its reach, the brightest flash gives less than one step of an 8-bit colour
	const glm::vec3 far = jelly::LightAt(jelly::k_LightReach) * jelly::k_MostGlow;
	EXPECT_LT(far.b, 1.0f / 255.0f);
}

TEST(UnderwaterJellyfish, ThingsNearByStirThem)
{
	EXPECT_FLOAT_EQ(jelly::Stir({}), 0.0f);
	const auto stir = [](float distance, float reach, float since = 0.0f) {
		const std::array<jelly::Touch, 1> touches {{{.distance = distance, .reach = reach, .since = since}}};
		return jelly::Stir(touches);
	};
	EXPECT_FLOAT_EQ(stir(0.0f, 6.0f), 1.0f);
	EXPECT_FLOAT_EQ(stir(2.0f, 6.0f), 1.0f);
	EXPECT_GT(stir(4.0f, 6.0f), 0.0f);
	EXPECT_LT(stir(4.0f, 6.0f), 1.0f);
	EXPECT_FLOAT_EQ(stir(6.0f, 6.0f), 0.0f);
	EXPECT_FLOAT_EQ(stir(20.0f, 6.0f), 0.0f);
	// A splash stirs them only while its wave goes past
	EXPECT_FLOAT_EQ(stir(10.0f, jelly::k_SplashReach, 0.2f), 1.0f);
	EXPECT_FLOAT_EQ(stir(10.0f, jelly::k_SplashReach, jelly::k_StirWindow + 0.1f), 0.0f);
	// The nearest thing for its reach counts
	const std::array<jelly::Touch, 2> two {{{.distance = 5.0f, .reach = 6.0f}, {.distance = 1.0f, .reach = 6.0f}}};
	EXPECT_FLOAT_EQ(jelly::Stir(two), 1.0f);
}

TEST(UnderwaterJellyfish, AFlashComesUpQuicklyAndGoesInASecondOrTwo)
{
	constexpr float k_Step = 1.0f / 60.0f;
	float flash = 0.0f;
	float time = 0.0f;
	for (; time < 0.2f; time += k_Step)
	{
		flash = jelly::StepFlash(flash, 1.0f, k_Step);
	}
	EXPECT_GT(flash, 0.9f);
	EXPECT_LE(flash, 1.0f);
	// The stir stops: the flash goes down
	float after = 0.0f;
	float atOne = 0.0f;
	for (; after < 2.0f; after += k_Step)
	{
		flash = jelly::StepFlash(flash, 0.0f, k_Step);
		if (std::abs(after - 1.0f) < 0.5f * k_Step)
		{
			atOne = flash;
		}
	}
	EXPECT_GT(atOne, 0.05f);
	EXPECT_LT(atOne, 0.25f);
	EXPECT_LT(flash, 0.03f);
	// A weak stir does not take a flash down under it
	EXPECT_FLOAT_EQ(jelly::StepFlash(0.3f, 0.3f, 1.0f), 0.3f);
}

TEST(UnderwaterJellyfish, EachKeepsItsOwnFlash)
{
	const std::array<jelly::Jellyfish, 2> two {{{.position = {0.0f, -40.0f, 0.0f}, .size = 1.0f, .shows = 1.0f, .id = 7},
	                                            {.position = {30.0f, -40.0f, 0.0f}, .size = 1.0f, .shows = 1.0f, .id = 9}}};
	const std::array<jelly::Disturber, 1> near {{{.position = {1.0f, -40.0f, 0.0f}, .reach = jelly::k_HandReach}}};
	jelly::Flashes flashes;
	// The first step has no time
	flashes.Update(two, near, 10.0f);
	EXPECT_FLOAT_EQ(flashes.Of(7), 0.0f);
	float seconds = 10.0f;
	for (int i = 0; i < 30; ++i)
	{
		seconds += 1.0f / 60.0f;
		flashes.Update(two, near, seconds);
	}
	EXPECT_GT(flashes.Of(7), 0.9f);
	EXPECT_FLOAT_EQ(flashes.Of(9), 0.0f);
	// The hand goes away: the flash goes in a few seconds, and the jellyfish is not kept
	for (int i = 0; i < 300; ++i)
	{
		seconds += 1.0f / 60.0f;
		flashes.Update(two, {}, seconds);
	}
	EXPECT_FLOAT_EQ(flashes.Of(7), 0.0f);
	// Its light is brighter in a flash, in its own colour
	const auto rest = jelly::LightOf(two[0], seconds, 0.0f);
	const auto flashed = jelly::LightOf(two[0], seconds, 1.0f);
	EXPECT_GT(flashed.colour.b, rest.colour.b);
	EXPECT_NEAR(flashed.colour.g / flashed.colour.b, rest.colour.g / rest.colour.b, 1e-4f);
	EXPECT_LT(flashed.position.y, two[0].position.y);
}

TEST(UnderwaterJellyfish, TheirHaloIsAboutOneCubicMetre)
{
	const float side = 2.0f * jelly::k_LightRadius / openblack::graphics::underwater::k_UnitsPerMetre;
	EXPECT_NEAR(side, 1.0f, 1e-4f);
}

TEST(UnderwaterJellyfish, TheirLightMakesASoftPoolOnTheSeaBed)
{
	// Under the bell it is all there, and it goes down with the distance to none at its reach
	float last = jelly::PoolAt(0.0f).b;
	for (const float distance : {0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 7.0f})
	{
		const float pool = jelly::PoolAt(distance).b;
		EXPECT_LT(pool, last);
		EXPECT_GT(pool, 0.0f);
		last = pool;
	}
	EXPECT_EQ(jelly::PoolAt(jelly::k_PoolReach).b, 0.0f);
	EXPECT_EQ(jelly::PoolAt(jelly::k_PoolReach + 3.0f).b, 0.0f);
	// From a bell 2 to 3 units over the bed, the pool gives the bed more light than the light that comes straight, so
	// the bed shows the pool at night
	for (const float height : {2.0f, 2.5f, 3.0f})
	{
		EXPECT_GT(jelly::PoolAt(height).b, 1.5f * jelly::LightAt(height).b);
	}
	// The pool is soft: at 4 units to the side of the bed under a bell 2.5 units up it keeps more than a third of its
	// light at the middle
	EXPECT_GT(jelly::PoolAt(std::sqrt(2.5f * 2.5f + 4.0f * 4.0f)).b, jelly::PoolAt(2.5f).b / 3.0f);
	// The bed at rest takes more than an 8-bit step of blue from a bell 2.5 units up, and it pulses with the beat
	const float rest = jelly::LightColour(0.0f).b * jelly::Emission(0.0f, 0.0f) * jelly::PoolAt(2.5f).b;
	const float squeezed = jelly::LightColour(0.0f).b * jelly::Emission(1.0f, 0.0f) * jelly::PoolAt(2.5f).b;
	EXPECT_GT(rest * jelly::PoolPulse(0.0f), 6.0f / 255.0f);
	EXPECT_GT(squeezed * jelly::PoolPulse(1.0f), 3.0f * rest * jelly::PoolPulse(0.0f));
}
