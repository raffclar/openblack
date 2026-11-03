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

#include <gtest/gtest.h>

#include "Enums.h"
#include "InfoConstants.h"
#include "Magic/AreaEffect.h"
#include "Magic/SpellRules.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

// The weight of each alignment kind of object, in the order of the alignment kinds
constexpr std::array k_Weights {
    &GAlignmentInfo::animalNice, &GAlignmentInfo::animalNasty, &GAlignmentInfo::creature,     &GAlignmentInfo::priest,
    &GAlignmentInfo::skeleton,   &GAlignmentInfo::villager,    &GAlignmentInfo::building,     &GAlignmentInfo::plant,
    &GAlignmentInfo::field,      &GAlignmentInfo::feature,     &GAlignmentInfo::mobileObject, &GAlignmentInfo::land,
    &GAlignmentInfo::script,     &GAlignmentInfo::unimportant,
};

} // namespace

// The waiting alignment takes the damped change of each kind of effect one at a time, crush, hit, heal, fly away and
// then the heat, as the game's effect update adds them
TEST(OracleEffectAlignmentOrder, PendingAlignmentAsInTheGame)
{
	const auto table = oracle::Table::Load("effect_alignment_order.txt");
	ASSERT_TRUE(table.has_value());
	const std::array names {"alignment", "pending", "life_before", "life_now",   "addition",     "kind",
	                        "crush",     "hit",     "heal",        "fly_away",   "burn_damage",  "w_burn",
	                        "w_crush",   "w_hit",   "w_heal",      "w_fly_away", "pending_after"};
	std::array<size_t, names.size()> column {};
	for (size_t i = 0; i < names.size(); ++i)
	{
		const auto found = table->Column(names.at(i));
		ASSERT_TRUE(found.has_value()) << names.at(i);
		column.at(i) = *found;
	}
	ASSERT_GT(table->Rows(), 0u);
	const auto value = [&table, &column](size_t row, size_t i) { return table->Float(row, column.at(i)); };
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto kind = table->Bits(row, column.at(5));
		ASSERT_LT(kind, k_Weights.size());
		std::array<GAlignmentInfo, 5> alignmentTable {};
		for (size_t effect = 0; effect < alignmentTable.size(); ++effect)
		{
			alignmentTable.at(effect).*k_Weights.at(kind) = value(row, 11 + effect);
		}
		magic::EffectValues values;
		values[magic::EffectKind::Crush] = value(row, 6);
		values[magic::EffectKind::Hit] = value(row, 7);
		values[magic::EffectKind::Heal] = value(row, 8);
		values[magic::EffectKind::FlyAway] = value(row, 9);
		const float lifeChange = value(row, 2) - value(row, 3);
		const auto terms = magic::EffectAlignmentChanges(values, value(row, 10), static_cast<AlignmentType>(kind), lifeChange,
		                                                 alignmentTable, value(row, 4));
		const float ours =
		    magic::AddEffectAlignmentChanges(value(row, 1), magic::DampEffectAlignmentChanges(terms, value(row, 0)));
		const float game = value(row, 16);
		if (std::bit_cast<uint32_t>(ours) != std::bit_cast<uint32_t>(game))
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << oracle::Hex(game) << ", ours gives "
				              << oracle::Hex(ours);
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
