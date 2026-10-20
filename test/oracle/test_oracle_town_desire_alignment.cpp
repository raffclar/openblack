/******************************************************************************
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

#include <gtest/gtest.h>

#include "ECS/TownDesireAlignment.h"
#include "OracleData.h"

using namespace openblack;
namespace alignment = openblack::ecs::town_desire_alignment;
using ecs::components::k_TownDesireCount;

// The data comes from tools/oracle/cases/town_desire_alignment_cases.py: the game's own function on the real FPU.

namespace
{
constexpr size_t k_Players = 8;

std::optional<std::array<size_t, k_TownDesireCount>> Columns(const oracle::Table& table, const std::string& prefix)
{
	std::array<size_t, k_TownDesireCount> columns {};
	for (size_t d = 0; d < k_TownDesireCount; ++d)
	{
		const auto column = table.Column(prefix + std::to_string(d));
		if (!column)
		{
			return std::nullopt;
		}
		columns.at(d) = *column;
	}
	return columns;
}

std::array<float, k_TownDesireCount> Floats(const oracle::Table& table, size_t row,
                                            const std::array<size_t, k_TownDesireCount>& columns)
{
	std::array<float, k_TownDesireCount> values {};
	for (size_t d = 0; d < k_TownDesireCount; ++d)
	{
		values.at(d) = table.Float(row, columns.at(d));
	}
	return values;
}

std::array<uint32_t, k_TownDesireCount> Words(const oracle::Table& table, size_t row,
                                              const std::array<size_t, k_TownDesireCount>& columns)
{
	std::array<uint32_t, k_TownDesireCount> values {};
	for (size_t d = 0; d < k_TownDesireCount; ++d)
	{
		values.at(d) = table.Bits(row, columns.at(d));
	}
	return values;
}
} // namespace

TEST(OracleTownDesireAlignment, TheWantsOfATownMoveItsPlayersAlignment)
{
	const auto table = oracle::Table::Load("town_desire_alignment.txt");
	ASSERT_TRUE(table.has_value());
	const auto boostA = Columns(*table, "boost_a");
	const auto desire = Columns(*table, "desire");
	const auto boost = Columns(*table, "boost");
	const auto turns = Columns(*table, "turns");
	const auto after = Columns(*table, "after");
	const auto importance = Columns(*table, "importance");
	const auto maxTurns = Columns(*table, "max_turns");
	const auto newTurns = Columns(*table, "new_turns");
	const auto children = table->Column("children");
	const auto adults = table->Column("adults");
	const auto maxChange = table->Column("max_change");
	const auto change = table->Column("change");
	ASSERT_TRUE(boostA && desire && boost && turns && after && importance && maxTurns && newTurns && children && adults &&
	            maxChange && change);
	std::array<size_t, k_Players> active {};
	std::array<size_t, k_Players> people {};
	for (size_t p = 0; p < k_Players; ++p)
	{
		const auto a = table->Column("active" + std::to_string(p));
		const auto n = table->Column("people" + std::to_string(p));
		ASSERT_TRUE(a && n);
		active.at(p) = *a;
		people.at(p) = *n;
	}
	ASSERT_GE(table->Rows(), 800u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto a = Floats(*table, row, *boostA);
		const auto d = Floats(*table, row, *desire);
		const auto b = Floats(*table, row, *boost);
		std::array<alignment::DesireRule, k_TownDesireCount> rules {};
		const auto levels = Floats(*table, row, *after);
		const auto weights = Floats(*table, row, *importance);
		const auto most = Words(*table, row, *maxTurns);
		for (size_t i = 0; i < k_TownDesireCount; ++i)
		{
			rules.at(i) = {.affectsAfter = levels.at(i), .importance = weights.at(i), .maxTurns = most.at(i)};
		}
		// Only the active players count, and the neutral player always
		uint32_t all = 0;
		for (size_t p = 0; p < k_Players; ++p)
		{
			if (p + 1 == k_Players || table->Bits(row, active.at(p)) != 0)
			{
				all += table->Bits(row, people.at(p));
			}
		}
		auto counters = Words(*table, row, *turns);
		const float ours = alignment::Change(counters, {.boostA = a, .desire = d, .boost = b}, rules,
		                                     {.town = table->Bits(row, *children) + table->Bits(row, *adults), .all = all},
		                                     table->Float(row, *maxChange));
		const auto gameTurns = Words(*table, row, *newTurns);
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *change) || counters != gameTurns)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << table->Float(row, *change) << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u) << "of " << table->Rows() << " rows";
}
