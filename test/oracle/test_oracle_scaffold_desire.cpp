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
#include <string>

#include <gtest/gtest.h>

#include "ECS/PlannedBuildingRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::ecs::planned_building;

namespace
{

/// The columns of a table row, read by name
struct Row
{
	const oracle::Table& table;
	size_t row;

	[[nodiscard]] float F(const std::string& name) const { return table.Float(row, table.Column(name).value_or(0)); }
	[[nodiscard]] uint32_t U(const std::string& name) const { return table.Bits(row, table.Column(name).value_or(0)); }
};

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleScaffoldDesire, TheWantOfABuildingMatchesTheGameWithTheScaffoldValue)
{
	const auto table = oracle::Table::Load("scaffold_desire.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t mismatches = 0;
	size_t wanted = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		TownForPlans town {
		    .neutral = row.U("neutral") != 0,
		    .adultRoom = std::bit_cast<int32_t>(row.U("adult_room")),
		    .homeless = row.U("homeless"),
		    .adults = row.U("adults"),
		    .children = row.U("children"),
		    .wonderDesire = row.F("wonder_desire"),
		    .wonderTrigger = row.F("wonder_trigger"),
		};
		for (size_t number = 0; number < town.built.size(); ++number)
		{
			town.built.at(number) = static_cast<uint8_t>(row.U("built" + std::to_string(number)));
		}
		const PlannedKind plan {
		    .type = static_cast<AbodeType>(row.U("type")),
		    .number = static_cast<AbodeNumber>(row.U("number")),
		    .desireToBeBuilt = row.F("desire"),
		    .maxAdults = row.U("max_adults"),
		    .sitesOfSameType = row.U("sites_same"),
		    .scaffoldsRequired = row.U("scaffolds"),
		};
		const float ours = DesireToBeBuilt(plan, town, row.U("value"));
		if (ours != 0.0f)
		{
			++wanted;
		}
		if (std::bit_cast<uint32_t>(ours) != row.U("result") && ++mismatches <= 10)
		{
			ADD_FAILURE() << "row " << index << " type " << row.U("type") << " value " << row.U("value") << ": ours " << ours
			              << " game " << std::bit_cast<float>(row.U("result"));
		}
	}
	EXPECT_EQ(mismatches, 0u);
	// The data must hold many cases where the town wants the building, else it tests little
	EXPECT_GT(wanted, table->Rows() / 4);
}
