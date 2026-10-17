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

#include <vector>

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
	// The shortest way from a jellyfish to the surface is straight up from its top. The brightest glow and the brightest
	// halo, through that water, are less than one step of an 8-bit colour, in each colour.
	const glm::vec3 left = jelly::GlowThroughWater(jelly::k_TopDepth);
	const float brightest = jelly::k_MostGlow * (1.0f + jelly::k_HaloStrength);
	for (int colour = 0; colour < 3; ++colour)
	{
		EXPECT_LT(brightest * left[colour], 1.0f / 255.0f);
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

TEST(UnderwaterJellyfish, TheirHaloIsAboutOneCubicMetre)
{
	const float side = 2.0f * jelly::k_LightRadius / openblack::graphics::underwater::k_UnitsPerMetre;
	EXPECT_NEAR(side, 1.0f, 1e-4f);
}
