/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>
#include <numeric>
#include <span>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/BlockTexture.h"
#include "Common/WorkerPool.h"

namespace block_texture = openblack::block_texture;
using openblack::lnd::LNDCell;
using openblack::lnd::LNDCountry;

namespace
{
constexpr size_t k_MapTexels = static_cast<size_t>(block_texture::k_Side) * block_texture::k_Side;
constexpr uint16_t k_White = 0x7FFF;

/// A country painting every height with the one material
LNDCountry SingleMaterialCountry(uint32_t material)
{
	LNDCountry country {};
	for (auto& entry : country.materials)
	{
		entry.indices = {material, material};
		entry.coefficient = 0;
	}
	return country;
}

std::vector<LNDCell> FlatCells(uint8_t altitude)
{
	std::vector<LNDCell> cells(static_cast<size_t>(block_texture::k_CellsPerSide * block_texture::k_CellsPerSide));
	for (auto& cell : cells)
	{
		cell.altitude = altitude;
	}
	return cells;
}

std::array<uint8_t, 4> TexelAt(const std::vector<uint8_t>& rgba, int x, int z)
{
	const auto index = static_cast<size_t>((x * block_texture::k_Side) + z) * 4;
	return {rgba[index], rgba[index + 1], rgba[index + 2], rgba[index + 3]};
}
} // namespace

TEST(BlockTexture, ConeWeightsAddUpTo255)
{
	for (const auto& weights : block_texture::ComputeConeWeights())
	{
		EXPECT_EQ(std::accumulate(weights.begin(), weights.end(), 0), 255);
	}
}

TEST(BlockTexture, ConeWeightsFavourTheNearestCorner)
{
	const auto weights = block_texture::ComputeConeWeights();
	EXPECT_EQ(weights[0], (std::array<uint8_t, 4> {255, 0, 0, 0}));
	// In the middle the corners are equally far: what truncation leaves goes to the last of them
	EXPECT_EQ(weights[(8 * 16) + 8], (std::array<uint8_t, 4> {63, 63, 63, 66}));
}

TEST(BlockTexture, CoastAlphaIsClearAtSeaLevelAndOpaqueInland)
{
	EXPECT_EQ(block_texture::CoastAlpha(0xFF, 0), 0);
	EXPECT_EQ(block_texture::CoastAlpha(0x400, 0), 15);
	EXPECT_EQ(block_texture::CoastAlpha(0xFFFF, 0x80), 15);
}

TEST(BlockTexture, CoastAlphaFollowsTheNoise)
{
	EXPECT_EQ(block_texture::CoastAlpha(0x300, 0), 10);
	// The noise counts as signed: high bytes lower the coast, low ones raise it
	EXPECT_EQ(block_texture::CoastAlpha(0x300, 0x80), 0);
	EXPECT_EQ(block_texture::CoastAlpha(0x300, 0x7F), 15);
}

TEST(BlockTexture, CountryTexelIsShadedByTheBumpMap)
{
	const auto country = SingleMaterialCountry(0);
	const std::vector<uint16_t> materials(k_MapTexels, k_White);
	// A bump of a half keeps white just within 4 bits; a quarter halves it again
	EXPECT_EQ(block_texture::CountryTexel(country, 0x400, 0, 0x80, materials, 0), 0xFFFF);
	EXPECT_EQ(block_texture::CountryTexel(country, 0x400, 0, 0x40, materials, 0), 0xF777);
	// Under altitude 1 nothing is painted
	EXPECT_EQ(block_texture::CountryTexel(country, 0xFF, 0, 0x80, materials, 0), 0);
}

TEST(BlockTexture, CountryTexelBlendsTwoMaterialsByTheCoefficient)
{
	LNDCountry country {};
	for (auto& entry : country.materials)
	{
		entry.indices = {0, 1};
		entry.coefficient = 0x100;
	}
	std::vector<uint16_t> materials(k_MapTexels * 2, 0);
	std::fill_n(materials.begin(), k_MapTexels, k_White);
	// All of the first material, none of the second
	EXPECT_EQ(block_texture::CountryTexel(country, 0x400, 0, 0x80, materials, 0), 0xFFFF);
	for (auto& entry : country.materials)
	{
		entry.coefficient = 0;
	}
	EXPECT_EQ(block_texture::CountryTexel(country, 0x400, 0, 0x80, materials, 0), 0xF000);
}

TEST(BlockTexture, BlendCornersWeighsEachChannel)
{
	EXPECT_EQ(block_texture::BlendCorners({0xFFFF, 0, 0, 0}, {255, 0, 0, 0}), 0xFFFF);
	EXPECT_EQ(block_texture::BlendCorners({0xFFFF, 0, 0, 0}, {63, 63, 63, 66}), 0x3333);
}

TEST(BlockTexture, BuildBlockPaintsHighLandOpaque)
{
	const std::array countries = {SingleMaterialCountry(0)};
	const std::vector<uint16_t> materials(k_MapTexels, k_White);
	const std::vector<uint8_t> noise(k_MapTexels, 0);
	const std::vector<uint8_t> bump(k_MapTexels, 0x80);
	const auto cells = FlatCells(10);
	std::vector<uint8_t> rgba(block_texture::k_BlockBytes, 0);
	block_texture::BuildBlock(cells, {countries, materials, noise, bump}, rgba);
	EXPECT_EQ(TexelAt(rgba, 0, 0), (std::array<uint8_t, 4> {255, 255, 255, 255}));
	EXPECT_EQ(TexelAt(rgba, 255, 255), (std::array<uint8_t, 4> {255, 255, 255, 255}));
}

TEST(BlockTexture, BuildBlockLeavesTheSeaClear)
{
	const std::array countries = {SingleMaterialCountry(0)};
	const std::vector<uint16_t> materials(k_MapTexels, k_White);
	const std::vector<uint8_t> noise(k_MapTexels, 0);
	const std::vector<uint8_t> bump(k_MapTexels, 0x80);
	auto cells = FlatCells(10);
	// Open sea in the cell at x 1, z 2
	cells[(1 * block_texture::k_CellsPerSide) + 2].flags = 0x02;
	std::vector<uint8_t> rgba(block_texture::k_BlockBytes, 0xAA);
	block_texture::BuildBlock(cells, {countries, materials, noise, bump}, rgba);
	EXPECT_EQ(TexelAt(rgba, 16, 32), (std::array<uint8_t, 4> {0, 0, 0, 0}));
	EXPECT_EQ(TexelAt(rgba, 31, 47), (std::array<uint8_t, 4> {0, 0, 0, 0}));
	EXPECT_EQ(TexelAt(rgba, 32, 32), (std::array<uint8_t, 4> {255, 255, 255, 255}));

	// Land at sea level is clear too
	const auto sea = FlatCells(0);
	block_texture::BuildBlock(sea, {countries, materials, noise, bump}, rgba);
	EXPECT_EQ(TexelAt(rgba, 100, 100), (std::array<uint8_t, 4> {0, 0, 0, 0}));
}

TEST(BlockTexture, PaintCellsPaintsARectangleAsTheWholeBlockWould)
{
	const std::array countries = {SingleMaterialCountry(0)};
	std::vector<uint16_t> materials(k_MapTexels);
	std::iota(materials.begin(), materials.end(), uint16_t {0});
	std::vector<uint8_t> noise(k_MapTexels);
	std::iota(noise.begin(), noise.end(), uint8_t {0});
	const std::vector<uint8_t> bump(k_MapTexels, 0x80);
	auto cells = FlatCells(10);
	// A slope and a cell of open sea, so every texel differs
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].altitude = static_cast<uint8_t>(2 + (i % 23));
	}
	cells[(5 * block_texture::k_CellsPerSide) + 6].flags = 0x02;
	const block_texture::Sources sources {countries, materials, noise, bump};
	std::vector<uint8_t> whole(block_texture::k_BlockBytes, 0);
	block_texture::BuildBlock(cells, sources, whole);

	// Cells x 4 to 6 and z 5 to 8: 48 rows of 64 texels
	const glm::ivec2 first {4, 5};
	const glm::ivec2 count {3, 4};
	std::vector<uint8_t> part(static_cast<size_t>(count.x * count.y) * 16 * 16 * 4, 0xAA);
	block_texture::PaintCells(cells, sources, first, count, part);
	for (int x = 0; x < count.x * 16; ++x)
	{
		for (int z = 0; z < count.y * 16; ++z)
		{
			const auto fromPart = static_cast<size_t>((x * count.y * 16) + z) * 4;
			const auto fromWhole = static_cast<size_t>(((first.x * 16 + x) * block_texture::k_Side) + (first.y * 16 + z)) * 4;
			ASSERT_TRUE(std::equal(part.begin() + static_cast<std::ptrdiff_t>(fromPart),
			                       part.begin() + static_cast<std::ptrdiff_t>(fromPart + 4),
			                       whole.begin() + static_cast<std::ptrdiff_t>(fromWhole)))
			    << "texel x " << x << " z " << z;
		}
	}
}

TEST(BlockTexture, BuildBlocksPaintsEachBlockAsBuildBlockIntoItsMemory)
{
	const std::array countries = {SingleMaterialCountry(0), SingleMaterialCountry(1)};
	std::vector<uint16_t> materials(k_MapTexels * 2);
	std::iota(materials.begin(), materials.end(), uint16_t {0});
	std::vector<uint8_t> noise(k_MapTexels);
	std::iota(noise.begin(), noise.end(), uint8_t {0});
	const std::vector<uint8_t> bump(k_MapTexels, 0x70);
	const block_texture::Sources sources {countries, materials, noise, bump};
	// Blocks of other slopes and countries, more than a batch of the lanes
	std::vector<std::vector<LNDCell>> blocks;
	for (size_t block = 0; block < 11; ++block)
	{
		auto cells = FlatCells(0);
		for (size_t i = 0; i < cells.size(); ++i)
		{
			cells[i].altitude = static_cast<uint8_t>(1 + ((i + block * 7) % 29));
			cells[i].properties.country = static_cast<uint8_t>((i / 5 + block) % 2);
		}
		blocks.push_back(std::move(cells));
	}
	openblack::WorkerPool pool(2);
	std::vector<std::vector<uint8_t>> painted(blocks.size(), std::vector<uint8_t>(block_texture::k_BlockBytes, 0xCD));
	block_texture::BuildBlocks(
	    blocks.size(), [&blocks](size_t block) { return std::span<const LNDCell>(blocks[block]); }, sources, pool,
	    [&painted](size_t block) { return std::span<uint8_t>(painted[block]); });
	for (size_t block = 0; block < blocks.size(); ++block)
	{
		std::vector<uint8_t> expected(block_texture::k_BlockBytes, 0);
		block_texture::BuildBlock(blocks[block], sources, expected);
		EXPECT_TRUE(std::ranges::equal(painted[block], expected)) << "block " << block;
	}
}

namespace
{
/// The texels of a rectangle painted one texel at a time, each from its own cell, as the land's painter did before it
/// went cell by cell: the result to compare with
std::vector<uint16_t> PaintTexelByTexel(std::span<const LNDCell> cells, const block_texture::Sources& sources,
                                        glm::ivec2 firstCell, glm::ivec2 cellCount)
{
	constexpr int k_Per = block_texture::k_TexelsPerCell;
	const auto rows = cellCount.x * k_Per;
	const auto columns = cellCount.y * k_Per;
	std::vector<uint16_t> texels(static_cast<size_t>(rows) * columns, 0);
	const auto weightsOf = block_texture::ComputeConeWeights();
	const auto countryOf = [&sources](const LNDCell& cell) -> const LNDCountry& {
		return sources.countries[std::min<size_t>(cell.properties.country, sources.countries.size() - 1)];
	};
	for (int x = firstCell.x * k_Per; x < (firstCell.x * k_Per) + rows; ++x)
	{
		for (int z = firstCell.y * k_Per; z < (firstCell.y * k_Per) + columns; ++z)
		{
			const auto cellIndex = static_cast<size_t>(((x / k_Per) * block_texture::k_CellsPerSide) + (z / k_Per));
			if ((cells[cellIndex].flags & 0x02) != 0)
			{
				continue;
			}
			const std::array corners = {&cells[cellIndex], &cells[cellIndex + 1],
			                            &cells[cellIndex + block_texture::k_CellsPerSide + 1],
			                            &cells[cellIndex + block_texture::k_CellsPerSide]};
			const auto& weights = weightsOf.at(static_cast<size_t>(((x % k_Per) * k_Per) + (z % k_Per)));
			int32_t h = 0;
			for (size_t k = 0; k < corners.size(); ++k)
			{
				h += static_cast<int32_t>(weights.at(k)) * corners.at(k)->altitude;
			}
			const auto index = static_cast<size_t>((x * block_texture::k_Side) + z);
			std::array<uint16_t, 4> painted {};
			for (size_t k = 0; k < corners.size(); ++k)
			{
				painted.at(k) = block_texture::CountryTexel(countryOf(*corners.at(k)), h, sources.noise[index],
				                                            sources.bump[index], sources.materials, index);
			}
			const bool one = std::ranges::all_of(
			    corners, [&corners](const LNDCell* c) { return c->properties.country == corners[0]->properties.country; });
			texels[static_cast<size_t>(((x - (firstCell.x * k_Per)) * columns) + (z - (firstCell.y * k_Per)))] =
			    one ? painted[0] : block_texture::BlendCorners(painted, weights);
		}
	}
	return texels;
}
} // namespace

TEST(BlockTexture, PaintCellTexelsMatchesThePaintOfOneTexelAtATime)
{
	std::array<LNDCountry, 3> countries {SingleMaterialCountry(0), SingleMaterialCountry(1), {}};
	// The third country mixes two materials, by a coefficient that changes with the height
	for (size_t level = 0; level < countries[2].materials.size(); ++level)
	{
		countries[2].materials.at(level).indices = {static_cast<uint32_t>(level % 2), static_cast<uint32_t>((level + 1) % 3)};
		countries[2].materials.at(level).coefficient = static_cast<uint32_t>((level * 37) % 256);
	}
	std::vector<uint16_t> materials(k_MapTexels * 3);
	for (size_t i = 0; i < materials.size(); ++i)
	{
		materials[i] = static_cast<uint16_t>((i * 2654435761u) >> 7);
	}
	std::vector<uint8_t> noise(k_MapTexels);
	std::vector<uint8_t> bump(k_MapTexels);
	for (size_t i = 0; i < k_MapTexels; ++i)
	{
		noise[i] = static_cast<uint8_t>((i * 31) ^ (i >> 5));
		bump[i] = static_cast<uint8_t>(0x60 + ((i * 7) % 0x50));
	}
	const block_texture::Sources sources {countries, materials, noise, bump};
	auto cells = FlatCells(0);
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].altitude = static_cast<uint8_t>((i * 13) % 9);
		cells[i].properties.country = static_cast<uint8_t>((i / 3) % 3);
		// Some cells are open sea
		cells[i].flags = static_cast<uint8_t>(i % 11 == 0 ? 0x02 : 0);
	}
	for (const auto& [first, count] :
	     {std::pair(glm::ivec2(0, 0), glm::ivec2(16, 16)), std::pair(glm::ivec2(3, 5), glm::ivec2(4, 7))})
	{
		std::vector<uint16_t> texels(static_cast<size_t>(count.x * count.y) * block_texture::k_TexelsPerCell *
		                             block_texture::k_TexelsPerCell);
		block_texture::PaintCellTexels(cells, sources, first, count, texels);
		EXPECT_EQ(texels, PaintTexelByTexel(cells, sources, first, count)) << "first " << first.x << " " << first.y;
	}
}

TEST(BlockTexture, TheLowResolutionTakesEverySecondTexelOfTheFullTexture)
{
	const std::array countries = {SingleMaterialCountry(0), SingleMaterialCountry(1)};
	std::vector<uint16_t> materials(k_MapTexels * 2);
	std::iota(materials.begin(), materials.end(), uint16_t {0});
	std::vector<uint8_t> noise(k_MapTexels);
	std::iota(noise.begin(), noise.end(), uint8_t {0});
	const std::vector<uint8_t> bump(k_MapTexels, 0x90);
	auto cells = FlatCells(10);
	// A slope, a cell of open sea and cells where two countries meet, so that the corners blend
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].altitude = static_cast<uint8_t>(2 + (i % 23));
		cells[i].properties.country = static_cast<uint8_t>((i / 5) % 2);
	}
	cells[(5 * block_texture::k_CellsPerSide) + 6].flags = 0x02;
	block_texture::Sources sources {countries, materials, noise, bump};
	std::vector<uint8_t> full(block_texture::k_BlockBytes, 0);
	block_texture::BuildBlock(cells, sources, full);

	sources.resolution = block_texture::Resolution::Low;
	ASSERT_EQ(block_texture::SideOf(sources.resolution), 128);
	ASSERT_EQ(block_texture::TexelsPerCellOf(sources.resolution), 8);
	std::vector<uint8_t> low(block_texture::BlockBytesOf(sources.resolution), 0xAA);
	block_texture::BuildBlock(cells, sources, low);
	for (int x = 0; x < block_texture::k_LowSide; ++x)
	{
		for (int z = 0; z < block_texture::k_LowSide; ++z)
		{
			const auto fromLow = static_cast<size_t>((x * block_texture::k_LowSide) + z) * 4;
			ASSERT_EQ((std::array {low[fromLow], low[fromLow + 1], low[fromLow + 2], low[fromLow + 3]}),
			          TexelAt(full, x * 2, z * 2))
			    << "texel x " << x << " z " << z;
		}
	}

	// A rectangle of cells at the low resolution is the same part of the low texture
	const glm::ivec2 first {4, 5};
	const glm::ivec2 count {3, 4};
	std::vector<uint8_t> part(static_cast<size_t>(count.x * count.y) * 8 * 8 * 4, 0xAA);
	block_texture::PaintCells(cells, sources, first, count, part);
	for (int x = 0; x < count.x * 8; ++x)
	{
		for (int z = 0; z < count.y * 8; ++z)
		{
			const auto fromPart = static_cast<size_t>((x * count.y * 8) + z) * 4;
			const auto fromLow = static_cast<size_t>(((first.x * 8 + x) * block_texture::k_LowSide) + (first.y * 8 + z)) * 4;
			ASSERT_TRUE(std::equal(part.begin() + static_cast<std::ptrdiff_t>(fromPart),
			                       part.begin() + static_cast<std::ptrdiff_t>(fromPart + 4),
			                       low.begin() + static_cast<std::ptrdiff_t>(fromLow)))
			    << "texel x " << x << " z " << z;
		}
	}
}
