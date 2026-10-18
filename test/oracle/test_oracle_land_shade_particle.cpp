/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <span>
#include <string>
#include <vector>

#include <StackedBitmap.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandShade.h"
#include "OracleData.h"
#include "Particles/ParticleDrawFrame.h"

using namespace openblack;

namespace
{

/// A particle's stamp is centred this far past its particle along x and z, as the particles' draw frame lays it
constexpr glm::vec3 k_ParticleShift {10.0f, 0.0f, 10.0f};

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

TEST(OracleLandShadeParticle, AParticlesStampTakesTheGamesPlaceAlphaAndMode)
{
	const auto table = oracle::Table::Load("land_shade_particle.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GE(table->Rows(), 2000u);
	const Columns c {*table};
	int failures = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto kind = static_cast<int>(table->Bits(row, c("kind")));
		const auto side = static_cast<int>(table->Bits(row, c("side")));
		const auto frames = static_cast<int>(table->Bits(row, c("frames")));
		const auto frame = static_cast<int>(table->Bits(row, c("frame")));
		const bool hasData = table->Bits(row, c("has_data")) != 0;

		// The image as the game's resources keep it: grey (a shadow) or colour, so many frames
		psys::StackedBitmap bitmap {.pitch = side, .frames = frames, .channels = kind, .data = {}};
		const bool stamps = (kind == 1 || kind == 3) && hasData;
		if (stamps)
		{
			bitmap.data.assign(bitmap.FrameSize() * static_cast<size_t>(frames), 0);
		}
		const auto texels = stamps ? bitmap.Frame(frame) : std::span<const uint8_t> {};
		const bool added = table->Bits(row, c("added")) != 0;
		if (added != !texels.empty())
		{
			if (++failures <= 5)
			{
				ADD_FAILURE() << "row " << row << ": added " << added;
			}
			continue;
		}
		if (!added)
		{
			continue;
		}

		const glm::vec3 point {table->Float(row, c("x")), table->Float(row, c("y")), table->Float(row, c("z"))};
		const auto shadow = land_shade::ParticleShadow(point + k_ParticleShift, texels, side, table->Float(row, c("strength")));
		const auto first = static_cast<uint32_t>(texels.data() - bitmap.data.data());
		const bool same = std::bit_cast<uint32_t>(shadow.corner.x) == table->Bits(row, c("corner_x")) &&
		                  std::bit_cast<uint32_t>(shadow.corner.y) == table->Bits(row, c("corner_z")) &&
		                  shadow.alpha == table->Bits(row, c("alpha")) && first == table->Bits(row, c("first_texel")) &&
		                  static_cast<uint32_t>(shadow.side) == table->Bits(row, c("stamp_side")) &&
		                  (kind == 1 ? 2u : 1u) == table->Bits(row, c("mode"));
		if (!same && ++failures <= 5)
		{
			ADD_FAILURE() << "row " << row << ": corner " << shadow.corner.x << ", " << shadow.corner.y << " alpha "
			              << static_cast<int>(shadow.alpha) << " first " << first;
		}
	}
	EXPECT_EQ(failures, 0);
}

TEST(OracleLandShadeParticle, AStampsStrengthComesFromItsAlphaAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("land_shade_particle_strength.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_EQ(table->Rows(), 256u);
	const Columns c {*table};
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto alpha = static_cast<uint8_t>(table->Bits(row, c("alpha")));
		EXPECT_EQ(std::bit_cast<uint32_t>(particles::draw::StampStrength(alpha)), table->Bits(row, c("strength")))
		    << static_cast<int>(alpha);
	}
}
