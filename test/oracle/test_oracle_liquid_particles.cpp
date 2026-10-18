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
#include <bit>
#include <optional>
#include <string>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/LandPointLight.h"
#include "Creature/CreaturePuke.h"
#include "OracleData.h"
#include "Particles/LiquidParticles.h"

using namespace openblack;
using liquid_particles::Kind;
using liquid_particles::Particle;

namespace
{

/// The land that the case script lays out: 4 by 4 blocks of 16 cells, each corner at an altitude of 10 + its x
constexpr int32_t k_LandCells = 64;
constexpr float k_HeightUnit = 0.67f;
constexpr float k_AltitudeFraction = 1.0f / 256.0f;

int32_t Luminosity(int32_t x, int32_t z)
{
	return (x * 37 + z * 91 + (x * z) % 13 + 5) & 0xFF;
}

/// The light table of the case script
std::array<uint32_t, 256> LightTable()
{
	std::array<uint32_t, 256> table {};
	for (uint32_t level = 0; level < table.size(); ++level)
	{
		table.at(level) = 0xFF000000u | (((level * 7 + 13) & 0xFFu) << 16u) | (((level * 29 + 101) & 0xFFu) << 8u) |
		                  (((255 - level) * 3) & 0xFFu);
	}
	return table;
}

/// The height of the slope at a point of the map in 16.16, as the game's land gives it: 0 off the blocks
float Altitude(int32_t x, int32_t z)
{
	const auto cellX = static_cast<int16_t>(static_cast<uint32_t>(x) >> 16u);
	const auto cellZ = static_cast<int16_t>(static_cast<uint32_t>(z) >> 16u);
	if (cellX < 0 || cellZ < 0 || cellX >= k_LandCells || cellZ >= k_LandCells)
	{
		return 0.0f;
	}
	const auto fractionX = static_cast<int32_t>((static_cast<uint32_t>(x) & 0xFFFFu) >> 8u);
	const int32_t height = ((10 + cellX) << 8) + fractionX;
	return static_cast<float>(height) * k_HeightUnit * k_AltitudeFraction;
}

std::optional<land_point_light::Corners> CornersOf(int32_t x, int32_t z)
{
	if (x >= k_LandCells || z >= k_LandCells)
	{
		return std::nullopt;
	}
	const auto at = [](int32_t cx, int32_t cz) { return static_cast<uint8_t>(Luminosity(cx, cz)); };
	return land_point_light::Corners {at(x, z), at(x, z + 1), at(x + 1, z), at(x + 1, z + 1)};
}

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && std::bit_cast<float>(game) == 0.0f);
}

struct Columns
{
	const oracle::Table& table;
	[[nodiscard]] size_t operator()(const char* name) const
	{
		const auto column = table.Column(name);
		EXPECT_TRUE(column.has_value()) << name;
		return column.value_or(0);
	}
};

} // namespace

TEST(OracleLiquidParticles, AParticleStepsAsTheGameDoes)
{
	const auto table = oracle::Table::Load("liquid_step.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 2000u);
	const Columns c {*table};
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		Particle particle {
		    .position = {table->Float(row, c("x")), table->Float(row, c("y")), table->Float(row, c("z"))},
		    .velocity = {table->Float(row, c("vx")), table->Float(row, c("vy")), table->Float(row, c("vz"))},
		    .size = table->Float(row, c("size")),
		    .age = table->Float(row, c("age")),
		    .kind = static_cast<Kind>(table->Bits(row, c("kind"))),
		    .onLand = table->Bits(row, c("on_land")) != 0,
		};
		const auto draw = static_cast<int32_t>(table->Bits(row, c("draw")));
		const bool kept =
		    liquid_particles::Step(particle, table->Float(row, c("seconds")), Altitude, [draw]() { return draw; });
		const bool same =
		    kept == (table->Bits(row, c("gone")) == 0) && Same(particle.position.x, table->Bits(row, c("x_out"))) &&
		    Same(particle.position.y, table->Bits(row, c("y_out"))) &&
		    Same(particle.position.z, table->Bits(row, c("z_out"))) &&
		    Same(particle.velocity.x, table->Bits(row, c("vx_out"))) &&
		    Same(particle.velocity.y, table->Bits(row, c("vy_out"))) &&
		    Same(particle.velocity.z, table->Bits(row, c("vz_out"))) &&
		    particle.onLand == (table->Bits(row, c("on_land_out")) != 0) && Same(particle.age, table->Bits(row, c("age_out")));
		if (!same && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": kind " << table->Bits(row, c("kind")) << " y " << particle.position.y << " vx "
			              << particle.velocity.x;
		}
	}
	EXPECT_EQ(failures, 0);
}

TEST(OracleLiquidParticles, AParticleIsDrawnAtTheGamesSizeAndFrame)
{
	const auto table = oracle::Table::Load("liquid_draw.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 1000u);
	const Columns c {*table};
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		Particle particle {
		    .size = table->Float(row, c("size")),
		    .age = table->Float(row, c("age")),
		    .variant = static_cast<int32_t>(table->Bits(row, c("variant"))),
		    .kind = static_cast<Kind>(table->Bits(row, c("kind"))),
		};
		if (particle.kind == Kind::Liquid)
		{
			particle.variant = liquid_particles::LiquidFrame(static_cast<int32_t>(table->Bits(row, c("draw"))));
		}
		const bool same = Same(liquid_particles::DrawnSize(particle), table->Bits(row, c("size_out"))) &&
		                  static_cast<uint32_t>(liquid_particles::Frame(particle)) == table->Bits(row, c("frame_out"));
		if (!same && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": kind " << table->Bits(row, c("kind")) << " size "
			              << liquid_particles::DrawnSize(particle) << " frame " << liquid_particles::Frame(particle);
		}
	}
	EXPECT_EQ(failures, 0);
}

TEST(OracleLiquidParticles, TheLandsLightAtAPointIsTheGames)
{
	const auto table = oracle::Table::Load("land_light.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 2000u);
	const auto light = LightTable();
	const Columns c {*table};
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec3 point {table->Float(row, c("x")), 0.0f, table->Float(row, c("z"))};
		const auto ours = land_point_light::LightAt(point, light, CornersOf);
		if (ours != table->Bits(row, c("light")) && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": " << std::hex << ours << " game " << table->Bits(row, c("light"));
		}
	}
	EXPECT_EQ(failures, 0);
}

TEST(OracleLiquidParticles, TheHazeDimsAColourAsTheGameDoes)
{
	const auto table = oracle::Table::Load("land_haze.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 2000u);
	const Columns c {*table};
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		glm::mat4 clip(0.0f);
		clip[0][3] = table->Float(row, c("cx"));
		clip[1][3] = table->Float(row, c("cy"));
		clip[2][3] = table->Float(row, c("cz"));
		clip[3][3] = table->Float(row, c("cw"));
		const glm::vec3 point {table->Float(row, c("x")), table->Float(row, c("y")), table->Float(row, c("z"))};
		const land_point_light::Haze haze {
		    .on = table->Bits(row, c("on")) != 0,
		    .nearDepth = table->Float(row, c("near")),
		    .farDepth = table->Float(row, c("far")),
		    .k = static_cast<int32_t>(table->Bits(row, c("k"))),
		};
		const auto ours = land_point_light::Hazed(table->Bits(row, c("colour")), land_point_light::DepthOf(point, clip), haze);
		if (ours != table->Bits(row, c("colour_out")) && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": " << std::hex << ours << " game " << table->Bits(row, c("colour_out"));
		}
	}
	EXPECT_EQ(failures, 0);
}

TEST(OracleLiquidParticles, ATurnOfPukeAddsTheGamesDrops)
{
	const auto table = oracle::Table::Load("creature_puke.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 1000u);
	const auto light = LightTable();
	const Columns c {*table};
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec3 place {table->Float(row, c("px")), table->Float(row, c("py")), table->Float(row, c("pz"))};
		glm::mat4 clip(0.0f);
		clip[0][3] = table->Float(row, c("cx"));
		clip[1][3] = table->Float(row, c("cy"));
		clip[2][3] = table->Float(row, c("cz"));
		clip[3][3] = table->Float(row, c("cw"));
		const land_point_light::Haze haze {
		    .on = table->Bits(row, c("on")) != 0,
		    .nearDepth = table->Float(row, c("near")),
		    .farDepth = table->Float(row, c("far")),
		    .k = static_cast<int32_t>(table->Bits(row, c("k"))),
		};
		const auto lit = land_point_light::Hazed(land_point_light::LightAt(place, light, CornersOf),
		                                         land_point_light::DepthOf(place, clip), haze);
		const auto size = table->Float(row, c("size"));
		const auto draw = table->Bits(row, c("draw"));
		const auto drops = creature_puke::DropsOf(size, lit, [draw](int32_t /*limit*/) { return draw; });
		const auto& drop = drops.front();
		const bool same =
		    table->Bits(row, c("count")) == static_cast<uint32_t>(creature_puke::k_DropsPerTurn) &&
		    Same(drop.velocity.x, table->Bits(row, c("vx_out"))) && Same(drop.velocity.y, table->Bits(row, c("vy_out"))) &&
		    Same(drop.velocity.z, table->Bits(row, c("vz_out"))) && drop.argb == table->Bits(row, c("argb_out")) &&
		    Same(creature_puke::DropSize(size), table->Bits(row, c("size_out"))) &&
		    table->Bits(row, c("kind_out")) == static_cast<uint32_t>(Kind::Liquid) &&
		    table->Bits(row, c("variant_out")) == static_cast<uint32_t>(liquid_particles::StartVariant(Kind::Liquid, {}));
		if (!same && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": argb " << std::hex << drop.argb << " game " << table->Bits(row, c("argb_out"));
		}
	}
	EXPECT_EQ(failures, 0);
}
