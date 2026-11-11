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
#include <string_view>

#include <gtest/gtest.h>

#include "Creature/CreaturePhysiology.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::creature_physiology;

namespace
{
/// The plan actions that change the pace of the body, by their numbers in the game's action table
constexpr uint32_t k_SleepAtHome = 0x11;
constexpr uint32_t k_RestToGetBetter = 0x3B;
constexpr uint32_t k_SleepByObject = 0x4E;
} // namespace

// One turn of the body as time passes: age, strength, warmth, fatness, energy, exhaustion, dehydration and size
TEST(OracleCreatureBodyTurn, TheBodyTurnIsTheGames)
{
	const auto table = oracle::Table::Load("creature_body_turn.txt");
	ASSERT_TRUE(table.has_value());
	const auto column = [&table](std::string_view name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	const auto age = column("age");
	const auto turns = column("turns");
	const auto strength = column("strength");
	const auto warmth = column("warmth");
	const auto fatness = column("fatness");
	const auto energy = column("energy");
	const auto exhaustion = column("exhaustion");
	const auto dehydration = column("dehydration");
	const auto size = column("size");
	const auto shown = column("shown");
	const auto phase = column("phase");
	const auto action = column("action");
	const auto moving = column("moving");
	const auto temperature = column("temperature");
	const auto comfort = column("comfort");
	const auto ageSeconds = column("age_seconds");
	const auto growMinutes = column("grow_minutes");
	const auto lowEnergy = column("low_energy");
	const auto exhaustionRate = column("exhaustion_rate");
	const auto dehydrateSeconds = column("dehydrate_seconds");
	const auto strengthDecay = column("strength_decay");
	const auto energyDrain = column("energy_drain");
	const auto fatBurn = column("fat_burn");
	const auto ageAfter = column("age_after");
	const auto strengthAfter = column("strength_after");
	const auto warmthAfter = column("warmth_after");
	const auto fatnessAfter = column("fatness_after");
	const auto energyAfter = column("energy_after");
	const auto exhaustionAfter = column("exhaustion_after");
	const auto dehydrationAfter = column("dehydration_after");
	const auto sizeAfter = column("size_after");
	ASSERT_GE(table->Rows(), 3000u);
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		Species species;
		species.comfortTemperature = table->Float(row, comfort);
		species.secondsPerAgeTick = table->Bits(row, ageSeconds);
		species.growUpMinutes = table->Float(row, growMinutes);
		species.lowEnergyThreshold = table->Float(row, lowEnergy);
		species.exhaustionRate = table->Float(row, exhaustionRate);
		species.secondsToDehydrate = table->Float(row, dehydrateSeconds);
		species.strengthDecay = table->Float(row, strengthDecay);
		species.energyDrain = table->Float(row, energyDrain);
		species.fatBurn = table->Float(row, fatBurn);
		Needs needs;
		needs.age = table->Bits(row, age);
		// The game counts the turn as lived before the body's turn; openblack counts it in the body's turn
		needs.turns = table->Bits(row, turns) - 1;
		needs.warmth = table->Float(row, warmth);
		needs.energy = table->Float(row, energy);
		needs.exhaustion = table->Float(row, exhaustion);
		needs.dehydration = table->Float(row, dehydration);
		Shape shape {.fatness = table->Float(row, fatness),
		             .strength = table->Float(row, strength),
		             .size = table->Float(row, size),
		             .penSize = table->Float(row, shown)};
		const auto plan = table->Bits(row, action);
		const Turn turn {.moving = table->Bits(row, moving) != 0,
		                 .slowEnergyUse = plan == k_SleepAtHome || plan == k_RestToGetBetter,
		                 .fastGrowth = plan == k_SleepAtHome || plan == k_SleepByObject,
		                 .phase = table->Bits(row, phase),
		                 .carriedWeight = std::nullopt,
		                 .temperature = table->Float(row, temperature),
		                 .turnsPerSecond = 10.0f};
		TickTurn(needs, shape, species, turn);
		EXPECT_EQ(needs.age, table->Bits(row, ageAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(shape.strength), table->Bits(row, strengthAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(needs.warmth), table->Bits(row, warmthAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(shape.fatness), table->Bits(row, fatnessAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(needs.energy), table->Bits(row, energyAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(needs.exhaustion), table->Bits(row, exhaustionAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(needs.dehydration), table->Bits(row, dehydrationAfter)) << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(shape.size), table->Bits(row, sizeAfter)) << "row " << row;
	}
}
