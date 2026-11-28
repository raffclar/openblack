/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Fuzz tests of the reader of a downloaded land: the ".map" file, its landscape, the land data and the paint of the
// block textures. The normal test program does a short run. The parser sanitizer program (OPENBLACK_PARSER_SANITIZERS)
// does the same tests under the address and undefined behaviour sanitizers, with more runs (OPENBLACK_FUZZ_RUNS).

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <LNDFile.h>
#include <OnlineMapFile.h>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/BlockTexture.h"
#include "3D/LandData.h"

using namespace openblack;

namespace
{

constexpr std::string_view k_Script = "VERSION(2.300000)\nLOAD_LANDSCAPE(\".\\Online Maps\\OBT_FUZZ.lnd\")\n";
constexpr size_t k_MapTexels = static_cast<size_t>(block_texture::k_Side) * block_texture::k_Side;
/// The runs of each fuzz test. The parser sanitizer program sets more runs at compile time.
#if defined(OPENBLACK_FUZZ_RUNS)
constexpr int k_Runs = OPENBLACK_FUZZ_RUNS;
#else
constexpr int k_Runs = 300;
#endif

/// A small land with all the parts that the reader reads: blocks, two countries, two materials, the noise, the bump map
/// and a coarse page
LandData SeedLand()
{
	LandData land;
	for (uint32_t i = 0; i < 3; ++i)
	{
		lnd::LNDBlock block {};
		block.blockX = 10 + i;
		block.blockZ = 12;
		block.index = i + 1;
		for (size_t c = 0; c < block.cells.size(); ++c)
		{
			block.cells.at(c).altitude = static_cast<uint8_t>(10 + ((c + i) % 40));
			block.cells.at(c).properties.country = static_cast<uint8_t>((c + i) % 2);
		}
		land.blocks.push_back(block);
		land.blockIndexLookup.at(((10 + i) * LandData::k_BlocksPerSide) + 12) = static_cast<uint16_t>(i + 1);
	}
	land.countries.resize(2);
	for (uint32_t c = 0; c < 2; ++c)
	{
		for (auto& entry : land.countries.at(c).materials)
		{
			entry.indices = {c, 1 - c};
			entry.coefficient = 0x80;
		}
	}
	land.materials.resize(2);
	land.noise.assign(k_MapTexels, 3);
	land.bump.assign(k_MapTexels, 0x80);
	lnd::LNDLowResolutionTexture page {};
	page.texels.assign(64, 7);
	land.coarsePages.push_back(page);
	return land;
}

/// Reads a ".map" file as the game reads a downloaded land, then paints a rectangle of 4 by 4 cells of each block. The
/// run selects the place of the rectangle, so that the runs paint all the cells. Gives true when the file read as a land.
/// A table place that the land data keeps names only a block that the land has.
bool ReadAndPaint(const std::vector<uint8_t>& bytes, int run)
{
	const auto file = onlinemap::Parse(std::as_bytes(std::span(bytes)));
	if (!file.has_value())
	{
		return false;
	}
	const auto* first = reinterpret_cast<const uint8_t*>(file->landscape.data());
	const std::vector<uint8_t> landscape(first, first + file->landscape.size());
	lnd::LNDFile lnd;
	if (lnd.Open(landscape) != lnd::LNDResult::Success)
	{
		return false;
	}
	const auto land = LandData::FromLnd(lnd);
	for (const auto index : land.blockIndexLookup)
	{
		EXPECT_LE(index, land.blocks.size());
	}
	if (land.noise.size() != k_MapTexels || land.bump.size() != k_MapTexels || land.countries.empty())
	{
		return true;
	}
	std::vector<uint16_t> materials(land.materials.size() * k_MapTexels);
	for (size_t i = 0; i < land.materials.size(); ++i)
	{
		std::memcpy(materials.data() + (i * k_MapTexels), land.materials[i].texels.data(), k_MapTexels * sizeof(uint16_t));
	}
	const block_texture::Sources sources {
	    .countries = land.countries,
	    .materials = materials,
	    .noise = land.noise,
	    .bump = land.bump,
	};
	constexpr int k_Cells = 4;
	constexpr int k_Places = block_texture::k_BlockCells - k_Cells + 1;
	const glm::ivec2 firstCell {run % k_Places, (run / k_Places) % k_Places};
	const glm::ivec2 cellCount {k_Cells, k_Cells};
	std::vector<uint8_t> rgba(static_cast<size_t>(k_Cells * block_texture::k_TexelsPerCell) * k_Cells *
	                          block_texture::k_TexelsPerCell * 4);
	for (const auto& block : land.blocks)
	{
		block_texture::PaintCells(block.cells, sources, firstCell, cellCount, rgba);
	}
	return true;
}

} // namespace

// Changed bytes of a land file, in its lengths, its landscape header and in other places, do not stop the reader, and
// the paint of a land that reads does not read past its tables.
TEST(MapParserFuzz, ChangedBytesOfALandFileDoNotStopTheReaderOrThePaint)
{
	const auto seed = *onlinemap::Write(k_Script, SeedLand().LndBytes());
	ASSERT_TRUE(ReadAndPaint(seed, 0));
	const size_t landscape = 8 + k_Script.size();
	std::mt19937 random(20261106);
	const auto pick = [&random](size_t from, size_t to) { return std::uniform_int_distribution<size_t>(from, to - 1)(random); };
	for (int run = 0; run < k_Runs; ++run)
	{
		auto bytes = seed;
		const auto changes = 1 + pick(0, 8);
		for (size_t i = 0; i < changes; ++i)
		{
			// Most changes go into the lengths, the landscape header and the first parts, which hold the counts
			size_t at = landscape + pick(0, 1200);
			if (pick(0, 3) == 0)
			{
				at = pick(0, bytes.size());
			}
			else if (pick(0, 8) == 0)
			{
				at = pick(0, 8);
			}
			bytes.at(at) = static_cast<uint8_t>(pick(0, 256));
		}
		if (pick(0, 10) == 0)
		{
			bytes.resize(pick(0, bytes.size()));
		}
		ReadAndPaint(bytes, run);
		if (testing::Test::HasFailure())
		{
			FAIL() << "run " << run;
		}
	}
}

// A land can name a block, a country or a material that it does not have. The reader and the paint then do not read past
// the tables of the land.
TEST(MapParserFuzz, IndexesPastTheTablesOfALandDoNotReadPastThem)
{
	const auto seedLand = SeedLand();
	std::mt19937 random(20261107);
	const auto pick = [&random](uint32_t from, uint32_t to) {
		return std::uniform_int_distribution<uint32_t>(from, to - 1)(random);
	};
	for (int run = 0; run < k_Runs; ++run)
	{
		auto land = seedLand;
		for (auto& block : land.blocks)
		{
			for (auto& cell : block.cells)
			{
				cell.properties.country = static_cast<uint8_t>(pick(0, 16));
			}
		}
		for (auto& country : land.countries)
		{
			for (auto& entry : country.materials)
			{
				// Small indexes near the count, and indexes whose product with the size of a material wraps round in
				// 32 bits
				const auto any = [&pick]() {
					switch (pick(0, 3))
					{
					case 0:
						return pick(0, 4);
					case 1:
						return pick(0, 0x10) << 16;
					default:
						return pick(0, 0xFFFF'FFFFu);
					}
				};
				entry.indices = {any(), any()};
				entry.coefficient = pick(0, 0x101);
			}
		}
		for (auto& index : land.blockIndexLookup)
		{
			if (pick(0, 64) == 0)
			{
				index = static_cast<uint16_t>(pick(0, 0x10000));
			}
		}
		const auto bytes = *onlinemap::Write(k_Script, land.LndBytes());
		ASSERT_TRUE(ReadAndPaint(bytes, run)) << "run " << run;
		if (testing::Test::HasFailure())
		{
			FAIL() << "run " << run;
		}
	}
}
