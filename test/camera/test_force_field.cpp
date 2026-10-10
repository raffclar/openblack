/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraZones.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using openblack::particles::LightSheet;

namespace
{
/// A square fence 100 across, from (0, 0) to (100, 100), closed back to its first corner as the force field is
const std::vector<glm::vec3> k_Square {
    {0.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 100.0f},
    {0.0f, 0.0f, 100.0f},
};

std::vector<glm::vec3> Closed(std::vector<glm::vec3> points)
{
	points.push_back(points.front());
	return points;
}

LightSheet ForceField()
{
	LightSheet sheet;
	sheet.StartPulsed(0x808080u, 250.0f, 256);
	sheet.SetPoints(Closed(k_Square));
	return sheet;
}
} // namespace

TEST(ForceField, UnseenUntilPulsed)
{
	auto sheet = ForceField();
	EXPECT_TRUE(sheet.Pulsed());
	EXPECT_EQ(sheet.GetLook(), LightSheet::Look::ForceField);
	EXPECT_FALSE(sheet.Showing());
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	EXPECT_TRUE(vertices.empty());
	EXPECT_TRUE(triangles.empty());
}

TEST(ForceField, PulseFallsOffAcrossTheGround)
{
	auto sheet = ForceField();
	// At the first corner, 50 up: the height doesn't count
	sheet.Pulse({0.0f, 50.0f, 0.0f}, 200.0f);
	const auto& strengths = sheet.Strengths();
	EXPECT_FLOAT_EQ(strengths[0], 1.0f);
	EXPECT_FLOAT_EQ(strengths[1], 0.5f);
	EXPECT_NEAR(strengths[2], 1.0f - std::sqrt(20000.0f) / 200.0f, 1e-6f);
	EXPECT_FLOAT_EQ(strengths[3], 0.5f);
	// The closing point is the first corner again
	EXPECT_FLOAT_EQ(strengths[4], 1.0f);
	// Pulses add up
	sheet.Pulse({0.0f, 0.0f, 0.0f}, 200.0f);
	EXPECT_FLOAT_EQ(sheet.Strengths()[0], 2.0f);
	// Points beyond the radius gain nothing
	auto far = ForceField();
	far.Pulse({0.0f, 0.0f, 0.0f}, 100.0f);
	EXPECT_FLOAT_EQ(far.Strengths()[1], 0.0f);
	EXPECT_FLOAT_EQ(far.Strengths()[2], 0.0f);
}

TEST(ForceField, FadesOverSecondsUntilUnseen)
{
	auto sheet = ForceField();
	sheet.Pulse({0.0f, 0.0f, 0.0f}, 200.0f);
	sheet.Update(1.0f);
	EXPECT_NEAR(sheet.Strengths()[0], std::exp(-1.0f), 1e-5f);
	// A full pulse shows for about 3.9 seconds: until it is down to 0.02
	auto timed = ForceField();
	timed.Pulse({0.0f, 0.0f, 0.0f}, 200.0f);
	float seconds = 0.0f;
	while (timed.Showing() && seconds < 10.0f)
	{
		timed.Update(0.01f);
		seconds += 0.01f;
	}
	EXPECT_NEAR(seconds, -std::log(0.02f), 0.02f);
}

TEST(ForceField, BuiltTwiceOverWithAWallOfLight)
{
	auto sheet = ForceField();
	sheet.Pulse({0.0f, 0.0f, 0.0f}, 200.0f);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	// Five points, three corners each, twice over; four triangles between each pair, twice over
	ASSERT_EQ(vertices.size(), 5u * 3u * 2u);
	EXPECT_EQ(triangles.size(), 4u * 12u * 2u);
	// The first point's foot, middle and top: 250 high, its light at full strength
	EXPECT_EQ(vertices[0].position, glm::vec3(0.0f));
	EXPECT_EQ(vertices[1].position, glm::vec3(0.0f, 125.0f, 0.0f));
	EXPECT_EQ(vertices[2].position, glm::vec3(0.0f, 250.0f, 0.0f));
	EXPECT_EQ(vertices[0].argb, 0xFF808080u);
	EXPECT_EQ(vertices[1].argb, 0xFF808080u);
	EXPECT_EQ(vertices[1].specularArgb, 0x20404040u);
	EXPECT_EQ(vertices[2].argb, 0x00808080u);
	EXPECT_EQ(vertices[0].uv.y, 0.0f);
	EXPECT_EQ(vertices[1].uv.y, 0.5f);
	EXPECT_EQ(vertices[2].uv.y, 1.0f);
	// The second point at half strength, its texture a hundredth of the way along for each unit
	EXPECT_EQ(vertices[3].argb, 0x7F808080u);
	EXPECT_NEAR(vertices[3].uv.x - vertices[0].uv.x, 1.0f, 1e-5f);
	// The second time over starts after the first, its texture sliding the other way
	EXPECT_EQ(triangles[48], 15u);
	EXPECT_EQ(vertices[15].position, vertices[0].position);
}

TEST(ForceField, TextureHardlySlides)
{
	auto sheet = ForceField();
	sheet.Pulse({0.0f, 0.0f, 0.0f}, 200.0f);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	// Each frame it slides back a frame's seconds and keeps a fifth of it: it settles a quarter of a frame back
	for (int frame = 0; frame < 20; ++frame)
	{
		sheet.Update(0.016f);
		sheet.Build(vertices, triangles);
	}
	EXPECT_NEAR(vertices[0].uv.x, -0.004f, 1e-5f);
	EXPECT_NEAR(vertices[15].uv.x, 0.004f, 1e-5f);
}

TEST(ForceField, MovesOntoOtherPointsKeepingStrengths)
{
	auto sheet = ForceField();
	sheet.Pulse({0.0f, 0.0f, 0.0f}, 200.0f);
	sheet.SetPoints({{0.0f, 0.0f, 0.0f}, {1000.0f, 0.0f, 0.0f}});
	EXPECT_EQ(sheet.Points(), 2u);
	EXPECT_FLOAT_EQ(sheet.Strengths()[1], 0.5f);
	// No more points than it has room for
	std::vector<glm::vec3> many(300, glm::vec3(0.0f));
	sheet.SetPoints(many);
	EXPECT_EQ(sheet.Points(), 256u);
}

TEST(ForceField, EdgeTurningPutsTheCameraFurtherIn)
{
	EXPECT_EQ(camera_zones::SearchRadius(false), 20.0f);
	EXPECT_EQ(camera_zones::SearchRadius(true), 50.0f);
}

TEST(ForceField, FlightsEndInsideTheFence)
{
	// Inside: unchanged
	const glm::vec3 inside {50.0f, 10.0f, 50.0f};
	EXPECT_EQ(camera_zones::FlightOriginInsideFence(k_Square, true, inside, {60.0f, 0.0f, 50.0f}), inside);
	// Outside, looking in: where the line meets the fence
	const auto moved = camera_zones::FlightOriginInsideFence(k_Square, true, {-50.0f, 10.0f, 50.0f}, {50.0f, 10.0f, 50.0f});
	EXPECT_NEAR(moved.x, 0.0f, 1e-4f);
	EXPECT_NEAR(moved.z, 50.0f, 1e-4f);
	// Without the fence nothing is moved
	const glm::vec3 outside {-50.0f, 10.0f, 50.0f};
	EXPECT_EQ(camera_zones::FlightOriginInsideFence(k_Square, false, outside, inside), outside);
}

TEST(ForceField, ScriptInfluenceOnlyInsideTheFence)
{
	EXPECT_TRUE(camera_zones::ScriptInfluenceCounts(k_Square, true, {50.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(camera_zones::ScriptInfluenceCounts(k_Square, true, {150.0f, 0.0f, 50.0f}));
	EXPECT_FALSE(camera_zones::ScriptInfluenceCounts(k_Square, true, {50.0f, 0.0f, -10.0f}));
	EXPECT_TRUE(camera_zones::ScriptInfluenceCounts(k_Square, false, {150.0f, 0.0f, 50.0f}));
}
