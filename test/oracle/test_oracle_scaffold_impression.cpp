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
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/TownDesire.h"
#include "ECS/ScaffoldImpressionRules.h"
#include "ECS/TownDesire.h"
#include "ECS/WorkshopRules.h"
#include "Magic/Impressiveness.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::ecs;

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
TEST(OracleScaffoldImpression, HowImpressiveAScaffoldIsMatchesTheGame)
{
	const auto table = oracle::Table::Load("scaffold_impressive.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t mismatches = 0;
	size_t notZero = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		// The town of the building keeps its wants as the town's desire does: the raw want and the two boosts
		components::TownDesire wants;
		const auto abodes = static_cast<size_t>(TownDesireInfo::ForAbodes);
		const auto civic = static_cast<size_t>(TownDesireInfo::ForCivicBuilding);
		wants.raw.at(abodes) = row.F("raw_abodes");
		wants.boost.at(abodes) = row.F("boost_abodes");
		wants.boostA.at(abodes) = row.F("cheat_abodes");
		wants.raw.at(civic) = row.F("raw_civic");
		wants.boost.at(civic) = row.F("boost_civic");
		wants.boostA.at(civic) = row.F("cheat_civic");
		std::optional<float> siteWant;
		if (row.U("on_site") != 0)
		{
			const auto kind = scaffold_impression::SiteWant(row.U("abode") != 0);
			siteWant = town_desire::GetRawDesire(wants, static_cast<size_t>(kind));
		}
		const float objectValue = magic::ObjectImpressiveValue({
		    .landBalance = row.F("balance"),
		    .impressiveValue = row.F("table_value"),
		    .reactionMultiplier = row.F("multiplier"),
		    .distance = row.F("distance"),
		    .maxDistance = row.F("reach"),
		    .boredom = row.F("boredom"),
		    .artefact = row.F("artefact"),
		});
		const float ours = scaffold_impression::ImpressiveValue(workshop::ValueOfScale(row.F("scale")), objectValue, siteWant);
		if (ours != 0.0f)
		{
			++notZero;
		}
		if (std::bit_cast<uint32_t>(ours) != row.U("result") && ++mismatches <= 10)
		{
			ADD_FAILURE() << "row " << index << ": ours " << ours << " game " << std::bit_cast<float>(row.U("result"));
		}
	}
	EXPECT_EQ(mismatches, 0u);
	// The data must hold many cases where the scaffold impresses, else it tests little
	EXPECT_GT(notZero, table->Rows() / 2);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(OracleScaffoldImpression, ThePeopleThatANewSiteImpressesMatchTheGame)
{
	const auto table = oracle::Table::Load("scaffold_spread.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	size_t mismatches = 0;
	size_t impressedRows = 0;
	for (size_t index = 0; index < table->Rows(); ++index)
	{
		const Row row {*table, index};
		const float reach = row.F("reach");
		const float proportion = row.F("proportion");
		std::vector<uint32_t> order;
		for (uint32_t t = 0; t < row.U("towns"); ++t)
		{
			const auto name = [t](const std::string& field) { return "t" + std::to_string(t) + "_" + field; };
			if (row.F(name("distance")) >= reach)
			{
				continue;
			}
			// The people of each abode, each with its number in the table
			std::vector<std::vector<entt::entity>> abodes;
			for (uint32_t a = 0; a < row.U(name("abodes")); ++a)
			{
				auto& people = abodes.emplace_back();
				for (uint32_t v = 0; v < row.U(name("a" + std::to_string(a))); ++v)
				{
					people.push_back(static_cast<entt::entity>((t * 100) + (a * 10) + v + 1));
				}
			}
			const auto count =
			    scaffold_impression::PeopleToImpress(row.U(name("adults")) + row.U(name("children")), proportion);
			for (const auto person : scaffold_impression::FirstPeople(abodes, count))
			{
				order.push_back(entt::to_integral(person));
			}
		}
		uint32_t sum = 0;
		for (size_t i = 0; i < order.size(); ++i)
		{
			sum += static_cast<uint32_t>(i + 1) * order[i];
		}
		if (!order.empty())
		{
			++impressedRows;
		}
		if ((order.size() != row.U("impressed") || sum != row.U("order_sum")) && ++mismatches <= 10)
		{
			ADD_FAILURE() << "row " << index << ": ours " << order.size() << " people, sum " << sum << "; game "
			              << row.U("impressed") << " people, sum " << row.U("order_sum");
		}
	}
	EXPECT_EQ(mismatches, 0u);
	EXPECT_GT(impressedRows, table->Rows() / 3);
}
