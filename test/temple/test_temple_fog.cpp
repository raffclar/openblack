/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/TempleFog.h"
#include "3D/TempleLight.h"

using namespace openblack;

TEST(TempleFog, RoomsAndTheirColour)
{
	const auto main = TempleFog::In(false, TempleLight {});
	EXPECT_EQ(main.start, 60.0f);
	EXPECT_EQ(main.end, 200.0f);
	// Two fifths of the neutral temple's white
	EXPECT_EQ(main.colour, glm::vec3(102.0f));
	const auto cave = TempleFog::In(true, TempleLight::At(-1.0f, 0));
	EXPECT_EQ(cave.start, 40.0f);
	EXPECT_EQ(cave.end, 150.0f);
	// The evil temple's light is 255, 175, 160
	EXPECT_FLOAT_EQ(cave.colour.r, 102.0f);
	EXPECT_FLOAT_EQ(cave.colour.g, 70.0f);
	EXPECT_FLOAT_EQ(cave.colour.b, 64.0f);
}

TEST(TempleFog, StepsRunEvenlyAndStopAtTheEnd)
{
	const TempleFog fog {.start = 60.0f, .end = 200.0f, .colour = glm::vec3(102.0f)};
	EXPECT_EQ(fog.Step(0.0f), 0);
	EXPECT_EQ(fog.Step(60.0f), 0);
	EXPECT_EQ(fog.Step(130.0f), 512);
	EXPECT_EQ(fog.Step(199.99f), 1023);
	EXPECT_EQ(fog.Step(200.0f), 1023);
	EXPECT_EQ(fog.Step(5000.0f), 1023);
}

TEST(TempleFog, ShadeFallsToTheFarShadeTakenAsAByte)
{
	// Even with no fog the scale of 256 is taken as a byte of 255
	EXPECT_EQ(TempleFog::Shade(0), 255.0f);
	// Half way 78 comes off: 178 of 256 is 177 of 255
	EXPECT_EQ(TempleFog::Shade(512), 177.0f);
	// The last step takes 155 off, leaving 101 of 256, 100 of 255
	EXPECT_EQ(TempleFog::Shade(1023), 100.0f);
}

TEST(TempleFog, AddedColourRoundsHalvesToEven)
{
	const TempleFog fog {.start = 60.0f, .end = 200.0f, .colour = glm::vec3(102.0f, 70.0f, 64.0f)};
	EXPECT_EQ(fog.Added(0), glm::vec3(0.0f));
	// Half of 102, 70 and 64
	EXPECT_EQ(fog.Added(512), glm::vec3(51.0f, 35.0f, 32.0f));
	// A quarter of 102 is 25.5, which rounds to 26; of 70, 17.5 rounds to 18
	EXPECT_EQ(fog.Added(256), glm::vec3(26.0f, 18.0f, 16.0f));
	EXPECT_EQ(fog.Added(1023), glm::vec3(102.0f, 70.0f, 64.0f));
}

TEST(TempleFog, AModelIsFoggedOnceAtItsDepth)
{
	const TempleFog fog {.start = 60.0f, .end = 200.0f, .colour = glm::vec3(102.0f)};
	const auto near = fog.AtModel(glm::vec3(224.0f), 10.0f);
	// Even before the fog a colour loses a little to the byte of 255
	EXPECT_EQ(near.diffuse, glm::vec3(223.0f));
	EXPECT_EQ(near.added, glm::vec3(0.0f));
	const auto half = fog.AtModel(glm::vec3(224.0f), 130.0f);
	EXPECT_EQ(half.diffuse, glm::vec3(154.0f));
	EXPECT_EQ(half.added, glm::vec3(51.0f));
}

TEST(TempleFog, AWallVertexMovesTowardsTheFogByTheStepAgain)
{
	const TempleFog fog {.start = 60.0f, .end = 200.0f, .colour = glm::vec3(102.0f)};
	const glm::vec3 white {255.0f};
	const glm::vec3 glow {0.0f, 0.0f, 20.0f};
	// Up to the start it keeps its own colours
	const auto near = fog.AtVertex(white, glow, 60.0f);
	EXPECT_EQ(near.diffuse, white);
	EXPECT_EQ(near.added, glow);
	// Half way it is half way to the step's colours: from 255 to 177 by 128 of 256, and from 0 to 51
	const auto half = fog.AtVertex(white, glow, 130.0f);
	EXPECT_EQ(half.diffuse, glm::vec3(216.0f));
	EXPECT_EQ(half.added, glm::vec3(25.0f, 25.0f, 35.0f));
	// From the end on it takes the last step's colours alone
	const auto far = fog.AtVertex(white, glow, 200.0f);
	EXPECT_EQ(far.diffuse, glm::vec3(100.0f));
	EXPECT_EQ(far.added, glm::vec3(102.0f));
}

TEST(TempleFog, TheShadersHaze)
{
	const TempleFog fog {.start = 40.0f, .end = 150.0f, .colour = glm::vec3(102.0f)};
	EXPECT_EQ(fog.Uniform(), glm::vec4(40.0f, 150.0f, 100.0f, 2.0f));
}
