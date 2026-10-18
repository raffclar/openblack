/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/LandPointLight.h"

using namespace openblack;

namespace
{
/// Level n has the grey n, n, n
std::array<uint32_t, 256> GreyTable()
{
	std::array<uint32_t, 256> table {};
	for (uint32_t level = 0; level < table.size(); ++level)
	{
		table.at(level) = 0xFF000000u | (level << 16u) | (level << 8u) | level;
	}
	return table;
}

std::optional<land_point_light::Corners> NoLand(int32_t /*x*/, int32_t /*z*/)
{
	return std::nullopt;
}
} // namespace

TEST(LandPointLight, BlendRoundsEachChannelDown)
{
	EXPECT_EQ(land_point_light::Blend(0x00000000u, 0x00FF8001u, 128), 0xFF7F4000u);
	EXPECT_EQ(land_point_light::Blend(0x00FF8001u, 0x00000000u, 128), 0xFF7F4000u);
	EXPECT_EQ(land_point_light::Blend(0x000A0A0Au, 0x00090909u, 1), 0xFF090909u);
	EXPECT_EQ(land_point_light::Blend(0x12345678u, 0x00000000u, 0), 0xFF345678u);
}

TEST(LandPointLight, TheLightBlendsTheCornersAlongZThenX)
{
	const auto table = GreyTable();
	const land_point_light::CornersAt corners = [](int32_t /*x*/, int32_t /*z*/) {
		return std::optional(land_point_light::Corners {0, 100, 200, 40});
	};
	// A quarter along x and a half along z of the cell from 10 to 20 metres
	const auto light = land_point_light::LightAt({12.5f, 0.0f, 15.0f}, table, corners);
	const int32_t near = 0 + (((100 - 0) * 128) >> 8);
	const int32_t far = 200 + (((40 - 200) * 128) >> 8);
	const auto grey = static_cast<uint32_t>(near + (((far - near) * 64) >> 8));
	EXPECT_EQ(light, 0xFF000000u | (grey << 16u) | (grey << 8u) | grey);
	// Off the map, or without land, it is the brightest level. A point less than a cell before the map is in the first
	// cell, because the cell is cut towards zero.
	EXPECT_EQ(land_point_light::LightAt({-11.0f, 0.0f, 5.0f}, table, corners), table[255]);
	EXPECT_NE(land_point_light::LightAt({-1.0f, 0.0f, 5.0f}, table, corners), table[255]);
	EXPECT_EQ(land_point_light::LightAt({5.0f, 0.0f, 5120.0f}, table, corners), table[255]);
	EXPECT_EQ(land_point_light::LightAt({5.0f, 0.0f, 5.0f}, table, NoLand), table[255]);
}

TEST(LandPointLight, TheHazeScalesTheColourDownToKAtTheFarDepth)
{
	const land_point_light::Haze haze {.on = true, .nearDepth = 100.0f, .farDepth = 300.0f, .k = 64};
	EXPECT_EQ(land_point_light::Hazed(0x80FFFFFFu, 50.0f, haze), 0x80FFFFFFu);
	EXPECT_EQ(land_point_light::Hazed(0x80FFFFFFu, 1000.0f, haze), 0x803F3F3Fu);
	// Halfway it keeps 256 - 96 of 256
	EXPECT_EQ(land_point_light::Hazed(0x80FF8040u, 200.0f, haze),
	          0x80000000u | ((0xFFu * 160u >> 8u) << 16u) | ((0x80u * 160u >> 8u) << 8u) | (0x40u * 160u >> 8u));
	EXPECT_EQ(land_point_light::Hazed(0x80FFFFFFu, 1000.0f, {.on = false}), 0x80FFFFFFu);
	glm::mat4 clip(0.0f);
	clip[2][3] = 1.0f;
	clip[3][3] = 5.0f;
	EXPECT_FLOAT_EQ(land_point_light::DepthOf({1.0f, 2.0f, 30.0f}, clip), 35.0f);
}
