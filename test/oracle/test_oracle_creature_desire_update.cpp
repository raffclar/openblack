/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <string>

#include <gtest/gtest.h>

#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "OracleData.h"

using namespace openblack;
using creature_desires::Desire;

namespace
{

constexpr size_t k_SourceCount = 4;
constexpr std::array<uint32_t, k_SourceCount> k_SourceTypes {2, 3, 4, 6};
constexpr float k_TurnsPerSecond = 10.0f;

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && std::bit_cast<float>(game) == 0.0f);
}

/// The column, or a failure that names it
size_t ColumnOf(const oracle::Table& table, const std::string& name)
{
	const auto column = table.Column(name);
	EXPECT_TRUE(column.has_value()) << name;
	return column.value_or(0);
}

} // namespace

TEST(OracleCreatureDesireUpdate, DataIsComplete)
{
	const auto table = oracle::Table::Load("creature_desire_update.txt");
	ASSERT_TRUE(table.has_value());
	EXPECT_GT(table->Rows(), 5000u);
}

TEST(OracleCreatureDesireUpdate, ATurnOfTheDesiresAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("creature_desire_update.txt");
	ASSERT_TRUE(table.has_value());
	const auto column = [&table](const std::string& name) { return ColumnOf(*table, name); };
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto f = [&](const std::string& name) { return table->Float(row, column(name)); };
		const auto bits = [&](const std::string& name) { return table->Bits(row, column(name)); };

		creature_desires::Desires desires;
		for (auto& state : desires.desires)
		{
			state.activated = false;
		}
		auto& desire = desires[Desire::Impress];
		desire.activated = true;
		desire.value = f("value");
		desire.decay = f("decay");
		desire.increaseSeconds = f("increase_seconds");
		desire.max = f("maximum");
		desire.suppressedTurns = bits("suppressed");
		desires.sum = f("sum");
		const auto sources = bits("sources");
		for (size_t i = 0; i < sources && i < k_SourceCount; ++i)
		{
			const auto index = std::to_string(i);
			desire.sources.push_back({
			    .type = k_SourceTypes.at(i),
			    .value = f("value" + index),
			    .threshold = f("threshold" + index),
			    .multiplier = f("multiplier" + index),
			    .drive = f("drive" + index),
			});
		}

		creature_desires::UpdateSources(desires, {});
		const auto impress = creature_desire_sources::ImpressAfterTurn(f("impress"));
		creature_desires::UpdateDesires(desires, k_TurnsPerSecond, f("floor"));

		bool same = Same(desire.value, bits("out_value")) && desire.suppressedTurns == bits("out_suppressed") &&
		            Same(desires.sum, bits("out_sum")) && Same(impress, bits("out_impress"));
		for (size_t i = 0; i < k_SourceCount; ++i)
		{
			const auto index = std::to_string(i);
			// A source that is not in the list keeps its numbers
			const auto value = i < sources ? desire.sources.at(i).value : f("value" + index);
			const auto drive = i < sources ? desire.sources.at(i).drive : f("drive" + index);
			same = same && Same(value, bits("out_value" + index)) && Same(drive, bits("out_drive" + index));
		}
		if (!same && ++different <= 20)
		{
			ADD_FAILURE() << "row " << row << ": the game gives value " << oracle::Hex(f("out_value")) << " sum "
			              << oracle::Hex(f("out_sum")) << ", ours gives value " << oracle::Hex(desire.value) << " sum "
			              << oracle::Hex(desires.sum);
		}
	}
	EXPECT_EQ(different, 0u);
}
