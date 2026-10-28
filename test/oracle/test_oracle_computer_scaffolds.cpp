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

#include <bit>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMindScaffoldRules.h"
#include "ECS/WorkshopRules.h"
#include "OracleData.h"

using namespace openblack;
namespace rules = openblack::computer_mind_rules;

namespace
{

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && std::bit_cast<float>(game) == 0.0f);
}

/// The column, or a failure that names it
size_t ColumnOf(const oracle::Table& table, const char* name)
{
	const auto column = table.Column(name);
	EXPECT_TRUE(column.has_value()) << name;
	return column.value_or(0);
}

/// A table with at least a number of rows
std::optional<oracle::Table> Loaded(const char* name, size_t rows)
{
	auto table = oracle::Table::Load(name);
	EXPECT_TRUE(table.has_value()) << name;
	if (table.has_value())
	{
		EXPECT_GE(table->Rows(), rows) << name;
	}
	return table;
}

/// Counts a difference, and names the first few
void Differs(size_t& different, size_t row, const std::string& what)
{
	++different;
	if (different <= 20)
	{
		ADD_FAILURE() << "row " << row << ": " << what;
	}
}

std::string Hex(uint32_t bits)
{
	return oracle::Hex(std::bit_cast<float>(bits));
}

} // namespace

TEST(OracleComputerScaffolds, ValueOfAScaffoldToUse)
{
	const auto table = Loaded("computer_scaffolds_use.txt", 3000);
	ASSERT_TRUE(table.has_value());
	const auto handX = ColumnOf(*table, "hand_x");
	const auto handZ = ColumnOf(*table, "hand_z");
	const auto scaffoldX = ColumnOf(*table, "scaffold_x");
	const auto scaffoldZ = ColumnOf(*table, "scaffold_z");
	const auto power = ColumnOf(*table, "power");
	const auto value = ColumnOf(*table, "value");
	const auto scale = ColumnOf(*table, "scale");
	const auto abodes = ColumnOf(*table, "abodes");
	const auto civic = ColumnOf(*table, "civic");
	const auto places = ColumnOf(*table, "adult_places");
	const auto pull = ColumnOf(*table, "pull");
	const auto best = ColumnOf(*table, "best");
	const auto found = ColumnOf(*table, "found");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const map_coords::MapCoords hand {.x = static_cast<int32_t>(table->Bits(row, handX)),
		                                  .z = static_cast<int32_t>(table->Bits(row, handZ))};
		const map_coords::MapCoords scaffold {.x = static_cast<int32_t>(table->Bits(row, scaffoldX)),
		                                      .z = static_cast<int32_t>(table->Bits(row, scaffoldZ))};
		// The worth comes back from the scale as the game reads it
		const auto worth = ecs::workshop::ValueOfScale(table->Float(row, scale));
		if (worth != table->Bits(row, value))
		{
			Differs(different, row, "the worth " + std::to_string(worth));
		}
		const float reach = table->Float(row, power) + rules::k_ScaffoldReachMore;
		const float nearness = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(hand, scaffold), reach);
		const rules::ScaffoldTownFacts town {.wantsAbodes = table->Float(row, abodes),
		                                     .wantsCivic = table->Float(row, civic),
		                                     .adultPlaces = static_cast<int32_t>(table->Bits(row, places))};
		const float score = table->Float(row, pull) * rules::ScaffoldUseValue(worth, town, nearness);
		const bool better = score > 0.0f;
		const float kept = better ? score : 0.0f;
		if (static_cast<uint32_t>(better) != table->Bits(row, found) || !Same(kept, table->Bits(row, best)))
		{
			Differs(different, row, "the game finds " + Hex(table->Bits(row, best)) + ", we find " + oracle::Hex(kept));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleComputerScaffolds, WantOfAWorkshopForWood)
{
	const auto table = Loaded("computer_scaffolds_fill.txt", 2000);
	ASSERT_TRUE(table.has_value());
	const auto functional = ColumnOf(*table, "functional");
	const auto room = ColumnOf(*table, "room");
	const auto wood = ColumnOf(*table, "wood");
	const auto woodValue = ColumnOf(*table, "wood_value");
	const auto most = ColumnOf(*table, "most");
	const auto value = ColumnOf(*table, "value");
	const auto chosen = ColumnOf(*table, "chosen");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		// A workshop counts when it works, has room (not 0, as a signed number) and less wood than a scaffold needs
		const auto space = static_cast<int32_t>(table->Bits(row, room));
		const bool counts =
		    table->Bits(row, functional) != 0 && space != 0 && table->Bits(row, wood) < table->Bits(row, woodValue);
		const float ours = counts ? rules::WorkshopFillValue(space, table->Float(row, most)) : 0.0f;
		if (static_cast<uint32_t>(counts) != table->Bits(row, chosen) || !Same(ours, table->Bits(row, value)))
		{
			Differs(different, row, "the game wants " + Hex(table->Bits(row, value)) + ", we want " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(different, 0u);
}
