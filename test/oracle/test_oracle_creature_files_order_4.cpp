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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureThrow.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
/// The ground height of the case script's Altitude
float Altitude(int32_t x, int32_t z)
{
	return static_cast<float>((x ^ z) & 0xFFFF);
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

// The aim of a creature that throws at a thing, a villager or a creature: the two flight times and the places ahead
TEST(OracleCreatureFilesOrder4, TheThrowAimIsTheGames)
{
	const auto table = oracle::Table::Load("creature_throw_aim.txt");
	ASSERT_TRUE(table.has_value());
	const auto kind = table->Column("kind");
	const auto thrower = table->Column("thrower_x");
	const auto mapX = table->Column("map_x");
	const auto height = table->Column("height");
	const auto step = table->Column("step_x");
	const auto finalState = table->Column("final");
	const auto position = table->Column("pos_x");
	const auto heading = table->Column("heading");
	const auto speed = table->Column("speed");
	const auto size = table->Column("size");
	const auto aim = table->Column("aim_x");
	const auto seconds = table->Column("seconds");
	ASSERT_TRUE(kind && thrower && mapX && height && step && finalState && position && heading && speed && size && aim &&
	            seconds);
	ASSERT_GE(table->Rows(), 1500u);
	using Kind = creature_throw::ThrowTarget::Kind;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		creature_throw::ThrowTarget target {
		    .kind = static_cast<Kind>(table->Bits(row, *kind)),
		    .mapPosition = {static_cast<int32_t>(table->Bits(row, *mapX)), static_cast<int32_t>(table->Bits(row, *mapX + 1))},
		    .height = table->Float(row, *height),
		    .step = {static_cast<int32_t>(table->Bits(row, *step)), static_cast<int32_t>(table->Bits(row, *step + 1))},
		    .walking = table->Bits(row, *finalState) == 0,
		    .position = {table->Float(row, *position), table->Float(row, *position + 1), table->Float(row, *position + 2)},
		    .heading = table->Float(row, *heading),
		    .speed = table->Float(row, *speed),
		};
		if (target.kind == Kind::Creature)
		{
			// A creature's height is its size times 15
			target.height = table->Float(row, *size) * 15.0f;
		}
		const glm::vec3 creature {table->Float(row, *thrower), table->Float(row, *thrower + 1),
		                          table->Float(row, *thrower + 2)};
		const auto got = creature_throw::AimAt(creature, target, Altitude);
		EXPECT_EQ(Bits(got.point.x), table->Bits(row, *aim)) << "row " << row;
		EXPECT_EQ(Bits(got.point.y), table->Bits(row, *aim + 1)) << "row " << row;
		EXPECT_EQ(Bits(got.point.z), table->Bits(row, *aim + 2)) << "row " << row;
		EXPECT_EQ(Bits(got.seconds), table->Bits(row, *seconds)) << "row " << row;
	}
}
