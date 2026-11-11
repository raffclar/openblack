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
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureDeedAlignment.h"
#include "Magic/AreaEffect.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
/// The desire of a hunger plan, and the number of alignment kinds in the table
constexpr uint32_t k_Hunger = 4;
constexpr uint32_t k_AlignmentKinds = 14;
/// The plan's object in the data: 2 is a villager
constexpr uint32_t k_Villager = 2;

struct Alignment
{
	float value;
	float pending;
};

/// The deed of the plan, as AlignmentSystem gets it from the plan and the tables
creature_deed_alignment::Deed DeedOf(bool valid, uint32_t desire, float desireChange, uint32_t object, uint32_t kind,
                                     float weight)
{
	creature_deed_alignment::Deed deed;
	if (!valid)
	{
		return deed;
	}
	deed.desireChange = desireChange;
	if (desire == k_Hunger && object == k_Villager && kind < k_AlignmentKinds)
	{
		deed.eatenVillagerWeight = weight;
	}
	return deed;
}

/// One creature turn, in the order of AlignmentSystem::CreatureTurn: the deed adds to the change that waits, then the
/// alignment takes a step
float Turn(Alignment& alignment, const creature_deed_alignment::Deed& deed, float scale, float mostChange)
{
	alignment.pending += creature_deed_alignment::TurnChange(deed, scale, alignment.value);
	const float pendingAfterDeed = alignment.pending;
	alignment.value = magic::StepPendingAlignment(alignment.value, alignment.pending, mostChange);
	return pendingAfterDeed;
}

bool Same(float a, float b)
{
	return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}
} // namespace

// The plan of the creature moves the change that waits, and the turn moves the alignment, to the same bits as the game
TEST(OracleCreatureDeedAlignment, OneTurnAsInTheGame)
{
	const auto table = oracle::Table::Load("creature_deed_alignment.txt");
	ASSERT_TRUE(table.has_value());
	const std::array names {"alignment", "pending", "valid",  "desire",       "desire_change",   "scale",        "most_change",
	                        "object",    "kind",    "weight", "pending_deed", "alignment_after", "pending_after"};
	std::array<size_t, names.size()> column {};
	for (size_t i = 0; i < names.size(); ++i)
	{
		const auto found = table->Column(names.at(i));
		ASSERT_TRUE(found.has_value()) << names.at(i);
		column.at(i) = *found;
	}
	ASSERT_GT(table->Rows(), 0u);
	const auto value = [&table, &column](size_t row, size_t i) { return table->Float(row, column.at(i)); };
	const auto bits = [&table, &column](size_t row, size_t i) { return table->Bits(row, column.at(i)); };
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		Alignment alignment {.value = value(row, 0), .pending = value(row, 1)};
		const auto deed = DeedOf(bits(row, 2) != 0, bits(row, 3), value(row, 4), bits(row, 7), bits(row, 8), value(row, 9));
		const float pendingDeed = Turn(alignment, deed, value(row, 5), value(row, 6));
		if (!Same(pendingDeed, value(row, 10)) || !Same(alignment.value, value(row, 11)) ||
		    !Same(alignment.pending, value(row, 12)))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(value(row, 10)) << " "
				              << oracle::Hex(value(row, 11)) << ", ours gives " << oracle::Hex(pendingDeed) << " "
				              << oracle::Hex(alignment.value);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}

// Many turns of one plan: a creature that eats villagers goes evil at the game's rate, to the same bits at each step
TEST(OracleCreatureDeedAlignment, DriftAsInTheGame)
{
	const auto table = oracle::Table::Load("creature_deed_alignment_drift.txt");
	ASSERT_TRUE(table.has_value());
	const std::array names {"start", "valid", "desire", "desire_change", "object", "weight", "turn", "alignment"};
	std::array<size_t, names.size()> column {};
	for (size_t i = 0; i < names.size(); ++i)
	{
		const auto found = table->Column(names.at(i));
		ASSERT_TRUE(found.has_value()) << names.at(i);
		column.at(i) = *found;
	}
	ASSERT_GT(table->Rows(), 0u);
	const auto value = [&table, &column](size_t row, size_t i) { return table->Float(row, column.at(i)); };
	const auto bits = [&table, &column](size_t row, size_t i) { return table->Bits(row, column.at(i)); };
	// The species values of info.dat, as the case script writes them
	constexpr float k_Scale = 0.2f;
	constexpr float k_MostChange = 0.0001f;
	constexpr uint32_t k_Kind = 5;
	size_t different = 0;
	std::optional<uint32_t> chainStart;
	Alignment alignment {};
	uint32_t turn = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		// A new chain starts where the turn count goes back
		if (!chainStart.has_value() || bits(row, 6) <= turn)
		{
			chainStart = bits(row, 0);
			alignment = {.value = value(row, 0), .pending = 0.0f};
			turn = 0;
		}
		const auto deed = DeedOf(bits(row, 1) != 0, bits(row, 2), value(row, 3), bits(row, 4), k_Kind, value(row, 5));
		for (; turn < bits(row, 6); ++turn)
		{
			Turn(alignment, deed, k_Scale, k_MostChange);
		}
		if (!Same(alignment.value, value(row, 7)))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << " turn " << turn << ": the game gives " << oracle::Hex(value(row, 7))
				              << ", ours gives " << oracle::Hex(alignment.value);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
