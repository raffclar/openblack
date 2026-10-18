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
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/LandShade.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

/// The land of the case script: three groups of 3 by 3 blocks
constexpr std::array<glm::ivec2, 3> k_Groups = {{{0, 0}, {14, 14}, {29, 29}}};
constexpr int32_t k_Shadows = 3;
constexpr int32_t k_Lights = 2;

std::vector<glm::ivec2> Blocks()
{
	std::vector<glm::ivec2> blocks;
	for (const auto group : k_Groups)
	{
		for (int32_t i = 0; i < 3; ++i)
		{
			for (int32_t j = 0; j < 3; ++j)
			{
				blocks.push_back(group + glm::ivec2(i, j));
			}
		}
	}
	return blocks;
}

uint8_t Luminosity(int32_t x, int32_t z)
{
	return static_cast<uint8_t>((x * 37 + z * 91 + (x * z) % 13 + 5) & 0xFF);
}

std::vector<land_shade::LaidBlock> Laid()
{
	std::vector<land_shade::LaidBlock> laid;
	for (const auto block : Blocks())
	{
		land_shade::LaidBlock one {.at = block, .luminosity = {}};
		for (int32_t i = 0; i < land_shade::k_BlockSide; ++i)
		{
			for (int32_t j = 0; j < land_shade::k_BlockSide; ++j)
			{
				one.luminosity.at(static_cast<size_t>((i * land_shade::k_BlockSide) + j)) =
				    Luminosity((block.x * land_shade::k_BlockStep) + i, (block.y * land_shade::k_BlockStep) + j);
			}
		}
		laid.push_back(one);
	}
	return laid;
}

/// The texels of a stamp's image from a seed, as the case script makes them
std::vector<uint8_t> Image(uint32_t seed, int32_t side, bool warm)
{
	std::vector<uint8_t> image(static_cast<size_t>(side) * static_cast<size_t>(side));
	uint32_t x = seed;
	for (auto& texel : image)
	{
		x = x * 1664525u + 1013904223u;
		texel = warm ? static_cast<uint8_t>((x >> 24u) % 48u) : static_cast<uint8_t>(x >> 24u);
	}
	return image;
}

uint32_t Hash(const land_shade::ShadedLand& land)
{
	uint32_t hash = 0x811C9DC5u;
	for (const auto block : Blocks())
	{
		for (const auto cell : *land.Find(block))
		{
			hash = (hash ^ cell) * 0x01000193u;
		}
	}
	return hash;
}

struct Columns
{
	const oracle::Table& table;
	[[nodiscard]] size_t operator()(const std::string& name) const
	{
		const auto column = table.Column(name);
		EXPECT_TRUE(column.has_value()) << name;
		return column.value_or(0);
	}
};

} // namespace

TEST(OracleLandShade, ShadowsAndLightsShadeTheLandAsTheGameDoes)
{
	const auto table = oracle::Table::Load("land_shade.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 1500u);
	const Columns c {*table};
	const auto laid = Laid();
	const auto blocks = Blocks();
	land_shade::ShadedLand land;
	land.Lay(laid);
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto view = table->Bits(row, c("view"));
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			land.SetInView(blocks[i], ((view >> i) & 1u) != 0);
		}
		const auto green = static_cast<uint8_t>(table->Bits(row, c("green")));
		std::vector<std::vector<uint8_t>> images;
		for (int32_t s = 0; s < k_Shadows; ++s)
		{
			const auto name = [s](const char* field) { return "s" + std::to_string(s) + "_" + field; };
			if (table->Bits(row, c(name("on"))) == 0)
			{
				continue;
			}
			const auto side = static_cast<int32_t>(table->Bits(row, c(name("side"))));
			const auto& image = images.emplace_back(Image(table->Bits(row, c(name("seed"))), side, false));
			glm::vec2 corner {table->Float(row, c(name("x"))), table->Float(row, c(name("z")))};
			if (table->Bits(row, c(name("centred"))) != 0)
			{
				corner = land_shade::Centred(corner, side);
			}
			land_shade::CastShadow(land, {.corner = corner,
			                              .image = image,
			                              .side = side,
			                              .alpha = land_shade::Alpha(table->Float(row, c(name("strength"))))});
		}
		for (int32_t l = 0; l < k_Lights; ++l)
		{
			const auto name = [l](const char* field) { return "l" + std::to_string(l) + "_" + field; };
			if (table->Bits(row, c(name("on"))) == 0)
			{
				continue;
			}
			const auto side = static_cast<int32_t>(table->Bits(row, c(name("side"))));
			const auto& image =
			    images.emplace_back(Image(table->Bits(row, c(name("seed"))), side, table->Bits(row, c(name("warm"))) != 0));
			land_shade::CastLight(land,
			                      {.corner = {table->Float(row, c(name("x"))), table->Float(row, c(name("z")))},
			                       .image = image,
			                       .side = side,
			                       .strength = table->Float(row, c(name("strength")))},
			                      green);
		}
		const auto shaded = Hash(land);
		land.Restore();
		const auto restored = Hash(land);
		if ((shaded != table->Bits(row, c("shaded_hash")) || restored != table->Bits(row, c("restored_hash"))) &&
		    ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": shaded " << std::hex << shaded << " game "
			              << table->Bits(row, c("shaded_hash")) << ", restored " << restored;
		}
	}
	EXPECT_EQ(failures, 0);
}

TEST(OracleLandShade, ALightsTexelsTakeTheGamesWarmLevels)
{
	const auto table = oracle::Table::Load("land_shade_warm.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_EQ(table->Rows(), 256u);
	const Columns c {*table};
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto texel = static_cast<uint8_t>(table->Bits(row, c("texel")));
		EXPECT_EQ(land_shade::WarmLevel(texel), table->Bits(row, c("level"))) << static_cast<int>(texel);
	}
}
