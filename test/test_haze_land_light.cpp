/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Graphics/Haze.h"
#include "Particles/Creators/LightMap.h"
#include "Particles/PSysManager.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
haze::Params Noon()
{
	LandLightTable::Haze h;
	h.nearDistance = 400.0f;
	h.farDistance = 900.0f;
	h.k = 211.0f;
	h.colour = glm::vec3(63.0f, 70.0f, 65.0f);
	return haze::FromTable(h, true);
}
} // namespace

// ApplyObject: off or closer than near leaves everything; the diffuse (c f) >> 8 keeps its alpha; the colour is
// rounded to nearest even
TEST(Haze, ObjectFormula)
{
	const auto params = Noon();
	uint32_t diffuse = 0x80FF8040u;
	EXPECT_EQ(haze::ApplyObject(params, 399.0f, 0x12345678u, &diffuse), 0x12345678u);
	EXPECT_EQ(diffuse, 0x80FF8040u);
	auto off = params;
	off.on = false;
	EXPECT_EQ(haze::ApplyObject(off, 2000.0f, 7u, &diffuse), 7u);
	// past far: t = 1, f = k = 211, colour (63, 70, 65), alpha 0xFF
	EXPECT_EQ(haze::ApplyObject(params, 5000.0f, 0u, &diffuse), 0xFF3F4641u);
	EXPECT_EQ(diffuse, 0x80000000u | ((0xFFu * 211u) >> 8) << 16 | ((0x80u * 211u) >> 8) << 8 | ((0x40u * 211u) >> 8));
	// saturated sum with a specular
	EXPECT_EQ(haze::ApplyObject(params, 5000.0f, 0x00F00000u, nullptr), 0xFFFF4641u);
}

TEST(Haze, RoundHalfEvenAndClasses)
{
	EXPECT_EQ(haze::RoundHalfEven(2.5f), 2);
	EXPECT_EQ(haze::RoundHalfEven(3.5f), 4);
	const auto params = Noon();
	EXPECT_EQ(haze::BlockClass(params, {100, 100, 100, 100, 100, 100, 100, 100}), 0);
	EXPECT_EQ(haze::BlockClass(params, {100, 500, 100, 100, 100, 100, 100, 100}), 1);
	EXPECT_EQ(haze::BlockClass(params, {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000}), 2);
	// class 2: the packed (truncated) colour and f = k; the specular keeps its alpha
	uint32_t diffuse = 0xFF808080u;
	uint32_t specular = 0x10000000u | 0x010101u;
	haze::ApplyVertex(params, 2, 0.0f, diffuse, specular);
	EXPECT_EQ(specular, 0x10404742u);
	EXPECT_EQ(diffuse, 0xFF000000u | 0x696969u);
}

// BlockCorners: x / z centre + 80 +- 80; y 0..h x 0.67, or -h..h x 0.67 with LandRef
TEST(Haze, BlockCorners)
{
	const auto corners = haze::BlockCorners(glm::vec2(160.0f, 320.0f), 100.0f, false);
	float minY = 1e9f;
	float maxY = -1e9f;
	for (const auto& corner : corners)
	{
		EXPECT_TRUE(corner.x == 160.0f || corner.x == 320.0f);
		EXPECT_TRUE(corner.z == 320.0f || corner.z == 480.0f);
		minY = std::min(minY, corner.y);
		maxY = std::max(maxY, corner.y);
	}
	EXPECT_FLOAT_EQ(minY, 0.0f);
	EXPECT_FLOAT_EQ(maxY, 67.0f);
	const auto mirrored = haze::BlockCorners(glm::vec2(0.0f), 100.0f, true);
	EXPECT_FLOAT_EQ(mirrored[0].y, -67.0f);
	EXPECT_FLOAT_EQ(mirrored[2].y, 67.0f);
	// all 8 sign combinations, each once
	for (size_t i = 0; i < corners.size(); ++i)
	{
		for (size_t j = i + 1; j < corners.size(); ++j)
		{
			EXPECT_NE(corners[i], corners[j]);
		}
	}
}

// At is bilinear in bytes (z then x) with weights cell >> 8, AtCell the cell alone, off the map table[255]
TEST(LandLight, Samplers)
{
	land_light::Cells cells;
	cells.firstCell = glm::ivec2(0);
	cells.size = glm::ivec2(17);
	cells.cells.assign(17 * 17, 0);
	cells.covered.assign(17 * 17, 1);
	cells.blocks[0] = 1;
	// cell (0, 0) colour 0 luminosity 0; (0, 1) colour 0x000100 luminosity 255
	cells.cells[0] = 0x00000000u;
	cells.cells[17] = 0xFF000100u; // index z * size.x + x: (x 0, z 1)
	LandLightTable table;          // every entry 0 before a Build
	const auto mid = land_light::At(cells, table, glm::vec2(0.0f, 5.0f));
	EXPECT_EQ(mid.specular, 0xFF000000u); // 0 + (1 x 128 >> 8) = 0
	const auto cell = land_light::AtCell(cells, table, glm::ivec2(0, 1));
	EXPECT_EQ(cell.specular, 0xFF000100u);
	const auto off = land_light::At(cells, table, glm::vec2(-5.0f, 5.0f));
	EXPECT_EQ(off.specular, 0xFF000000u);
	EXPECT_EQ(land_light::LerpBytes(0x000000u, 0x0000FFu, 128), 0x00007Fu);
	EXPECT_EQ(land_light::LerpBytes(0x0000FFu, 0x000000u, 128), 0x00007Fu); // 255 + floor(-255 x 128 / 256) = 127
}

// a shadow stamp: luminosity = min(luminosity, max(0x30, 255 - (255 - r) alpha / 255)); a light stamp adds, capped
TEST(LandLight, Stamps)
{
	std::vector<uint8_t> covered(16 * 16, 1);
	std::vector<uint32_t> dwords(16 * 16, 0xC8000000u);
	std::vector<uint8_t> black(4 * 4, 0);
	land_light::Stamp shadow;
	shadow.position = glm::vec3(0.0f);
	shadow.texels = black.data();
	shadow.pitch = 4;
	shadow.alpha = 255;
	shadow.mode = 2;
	land_light::ApplyStamp(shadow, glm::ivec2(0), glm::ivec2(16), covered, dwords);
	EXPECT_EQ(dwords[0] >> 24, 0x30u);
	EXPECT_EQ(dwords[3] >> 24, 0xC8u); // only pitch - 1 cells along each axis
	std::vector<uint8_t> red(4 * 4 * 3, 0);
	for (size_t i = 0; i < red.size(); i += 3)
	{
		red[i] = 200;
	}
	land_light::Stamp light = shadow;
	light.texels = red.data();
	light.mode = 1;
	land_light::ApplyStamp(light, glm::ivec2(0), glm::ivec2(16), covered, dwords);
	land_light::ApplyStamp(light, glm::ivec2(0), glm::ivec2(16), covered, dwords);
	EXPECT_EQ((dwords[0] >> 16) & 0xFFu, 0xFFu); // texel R into the D3DCOLOR's red, 200 + 200 capped
	EXPECT_EQ(dwords[0] & 0xFFFFu, 0u);
	// AddStamp: centred by (pitch - 1) x 5, alpha x 255 clamped
	land_light::ClearStamps();
	ASSERT_TRUE(land_light::AddStamp(glm::vec3(100.0f, 0.0f, 100.0f), red.data(), 4, true, 2.0f, 1));
	EXPECT_FLOAT_EQ(land_light::GetStamps()[0].position.x, 85.0f);
	EXPECT_EQ(land_light::GetStamps()[0].alpha, 255);
	land_light::ClearStamps();
}

// the texel rows run along x and the bytes along z (sclouds.raw is read
// transposed against an image[z][x] layout); with the weights 0 (frac just under 1) each cell takes its own texel
TEST(LandLight, StampLayout)
{
	std::vector<uint8_t> covered(16 * 16, 1);
	std::vector<uint32_t> dwords(16 * 16, 0xFF000000u);
	// 3 x 3 grey, t(i, j) at i x 3 + j: t(1, 0) = 100 (one step along x), t(0, 1) = 200 (one step along z)
	std::vector<uint8_t> texels(3 * 3, 255);
	texels[1 * 3 + 0] = 100;
	texels[0 * 3 + 1] = 200;
	land_light::Stamp stamp;
	stamp.position = glm::vec3(9.99f, 0.0f, 9.99f); // cell 0, rounded(255 - 0.999 x 255) = 0
	stamp.texels = texels.data();
	stamp.pitch = 3;
	stamp.alpha = 255;
	stamp.mode = 2;
	land_light::ApplyStamp(stamp, glm::ivec2(0), glm::ivec2(16), covered, dwords);
	// index z x 16 + x
	EXPECT_EQ(dwords[0 * 16 + 1] >> 24, 100u); // cell (x 1, z 0) takes t(1, 0)
	EXPECT_EQ(dwords[1 * 16 + 0] >> 24, 200u); // cell (x 0, z 1) takes t(0, 1)
	EXPECT_EQ(dwords[0] >> 24, 255u);
	EXPECT_EQ(dwords[2] >> 24, 255u); // pitch - 1 = 2 cells
}

// one stamp per light map atom and frame (+ (10, 0, 10), centred,
// mode 1 for 3 bytes per texel); other atoms none
TEST(LandLight, LightMapAtomsStampOnce)
{
	land_light::ClearStamps();
	psys::LightMapCreator lightMap;
	auto bitmap = std::make_shared<graphics::frame_anim::StackedFrames>();
	bitmap->pitch = 5;
	bitmap->frames = 1;
	bitmap->channels = 3;
	bitmap->data.assign(5 * 5 * 3, 100);
	lightMap.bitmap = bitmap;
	lightMap.numFrames = 1;
	psys::LightMapCreator noBitmap;
	psys::manager::Drawable drawable;
	drawable.atoms.push_back(
	    {&lightMap, glm::vec3(100.0f, 5.0f, 200.0f), glm::mat3(1.0f), 1.0f, 1.0f, 255.0f, 0.0f, {}, 0, nullptr});
	drawable.atoms.push_back(
	    {&lightMap, glm::vec3(300.0f, 5.0f, 200.0f), glm::mat3(1.0f), 1.0f, 1.0f, 51.0f, 0.0f, {}, 0, nullptr});
	drawable.atoms.push_back({&noBitmap, glm::vec3(0.0f), glm::mat3(1.0f), 1.0f, 1.0f, 255.0f, 0.0f, {}, 0, nullptr});
	EXPECT_EQ(psys::light_map_atoms::Stamp({drawable}), 2);
	const auto& stamps = land_light::GetStamps();
	ASSERT_EQ(stamps.size(), 2u);
	EXPECT_FLOAT_EQ(stamps[0].position.x, 100.0f + 10.0f - 20.0f);
	EXPECT_FLOAT_EQ(stamps[0].position.z, 200.0f + 10.0f - 20.0f);
	EXPECT_EQ(stamps[0].mode, 1);
	EXPECT_EQ(stamps[0].alpha, 255);
	EXPECT_EQ(stamps[1].alpha, 51);
	// once a frame: a second SubmitFrame before ClearStamps adds nothing (no effects run here either way)
	const auto frame = land_light::StampFrame();
	psys::light_map_atoms::SubmitFrame();
	psys::light_map_atoms::SubmitFrame();
	EXPECT_EQ(land_light::GetStamps().size(), 2u);
	land_light::ClearStamps();
	EXPECT_EQ(land_light::StampFrame(), frame + 1);
}
