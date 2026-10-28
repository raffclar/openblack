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
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMindWorshipRules.h"
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

TEST(OracleComputerWorship, RestDesireOfTheDancers)
{
	const auto table = Loaded("computer_worship_rest.txt", 3000);
	ASSERT_TRUE(table.has_value());
	const auto count = ColumnOf(*table, "count");
	const auto villagers = ColumnOf(*table, "villagers");
	const auto first = ColumnOf(*table, "life_0");
	const auto desire = ColumnOf(*table, "desire");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		std::vector<float> desires;
		for (uint32_t i = 0; i < table->Bits(row, count); ++i)
		{
			const bool villager = ((table->Bits(row, villagers) >> i) & 1) != 0;
			desires.push_back(villager ? rules::LifeDesire(table->Float(row, first + i)) : 0.0f);
		}
		const float ours = rules::DancersRestDesire(desires);
		if (!Same(ours, table->Bits(row, desire)))
		{
			Differs(different, row, "the game gives " + Hex(table->Bits(row, desire)) + ", we give " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleComputerWorship, DiscipleThatATownWants)
{
	const auto table = Loaded("computer_worship_disciple.txt", 3000);
	ASSERT_TRUE(table.has_value());
	const auto firstCount = ColumnOf(*table, "disciples_0");
	const auto adults = ColumnOf(*table, "adults");
	const auto hasTotem = ColumnOf(*table, "has_totem");
	const auto share = ColumnOf(*table, "share");
	const auto firstDesire = ColumnOf(*table, "desire_0");
	const auto breeder = ColumnOf(*table, "breeder");
	const auto finder = ColumnOf(*table, "finder");
	const auto value = ColumnOf(*table, "value");
	const auto amount = ColumnOf(*table, "amount");
	constexpr uint32_t k_Unchanged = 0xFFFF;
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		std::array<uint8_t, 13> disciples {};
		std::array<float, 13> desires {};
		for (size_t i = 0; i < disciples.size(); ++i)
		{
			disciples.at(i) = static_cast<uint8_t>(table->Bits(row, firstCount + i));
			desires.at(i) = table->Float(row, firstDesire + i);
		}
		const float worshipShare = table->Bits(row, hasTotem) != 0 ? table->Float(row, share) : 0.0f;
		const auto choice = rules::TownDiscipleWant(
		    disciples, table->Bits(row, adults), worshipShare, desires, [&] { return table->Float(row, breeder); },
		    [&] { return table->Float(row, finder); });
		const float ours = choice.has_value() ? choice->value : 0.0f;
		const uint32_t ourAmount = choice.has_value() ? static_cast<uint32_t>(choice->work) : k_Unchanged;
		if (!Same(ours, table->Bits(row, value)) || ourAmount != table->Bits(row, amount))
		{
			Differs(different, row,
			        "the game gives " + Hex(table->Bits(row, value)) + " work " + std::to_string(table->Bits(row, amount)) +
			            ", we give " + oracle::Hex(ours) + " work " + std::to_string(ourAmount));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleComputerWorship, WantOfWorkersForASite)
{
	const auto table = Loaded("computer_worship_site_workers.txt", 3000);
	ASSERT_TRUE(table.has_value());
	const auto maxBuilders = ColumnOf(*table, "max_builders");
	const auto needed = ColumnOf(*table, "needed");
	const auto built = ColumnOf(*table, "built");
	const auto repair = ColumnOf(*table, "repair");
	const auto desire = ColumnOf(*table, "desire");
	const auto builders = ColumnOf(*table, "builders");
	const auto pull = ColumnOf(*table, "pull");
	const auto value = ColumnOf(*table, "value");
	const auto chosen = ColumnOf(*table, "chosen");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const rules::SiteFacts site {.maxBuilders = static_cast<int32_t>(table->Bits(row, maxBuilders)),
		                             .buildersNeeded = static_cast<int32_t>(table->Bits(row, needed)),
		                             .built = table->Bits(row, built) != 0,
		                             .repairDesire = table->Float(row, repair)};
		// The god keeps a site only with a value above none
		float ours = 0.0f;
		if (rules::TakesWorkers(site))
		{
			const float each = rules::SiteWorkersValue(table->Float(row, pull), table->Float(row, desire),
			                                           table->Bits(row, builders), site.maxBuilders);
			ours = each > 0.0f ? each : 0.0f;
		}
		const bool ourChoice = ours > 0.0f;
		if (!Same(ours, table->Bits(row, value)) || ourChoice != (table->Bits(row, chosen) != 0))
		{
			Differs(different, row, "the game gives " + Hex(table->Bits(row, value)) + ", we give " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleComputerWorship, WantOfWoodForASite)
{
	const auto table = Loaded("computer_worship_site_wood.txt", 3000);
	ASSERT_TRUE(table.has_value());
	const auto needed = ColumnOf(*table, "needed");
	const auto built = ColumnOf(*table, "built");
	const auto repair = ColumnOf(*table, "repair");
	const auto wood = ColumnOf(*table, "wood");
	const auto pull = ColumnOf(*table, "pull");
	const auto value = ColumnOf(*table, "value");
	const auto chosen = ColumnOf(*table, "chosen");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const rules::SiteFacts site {.built = table->Bits(row, built) != 0,
		                             .repairDesire = table->Float(row, repair),
		                             .woodNeeded = table->Float(row, needed)};
		float ours = 0.0f;
		if (rules::TakesWood(site))
		{
			const float each = rules::SiteWoodValue(table->Float(row, pull), table->Bits(row, wood), site.woodNeeded);
			ours = each > 0.0f ? each : 0.0f;
		}
		const bool ourChoice = ours > 0.0f;
		if (!Same(ours, table->Bits(row, value)) || ourChoice != (table->Bits(row, chosen) != 0))
		{
			Differs(different, row, "the game gives " + Hex(table->Bits(row, value)) + ", we give " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleComputerWorship, FreeBuilderAndTheBestThatItKeeps)
{
	const auto table = Loaded("computer_worship_free_builder.txt", 3000);
	ASSERT_TRUE(table.has_value());
	const auto handX = ColumnOf(*table, "hand_x");
	const auto handZ = ColumnOf(*table, "hand_z");
	const auto villagerX = ColumnOf(*table, "villager_x");
	const auto villagerZ = ColumnOf(*table, "villager_z");
	const auto power = ColumnOf(*table, "power");
	const auto life = ColumnOf(*table, "life");
	const auto pull = ColumnOf(*table, "pull");
	const auto best = ColumnOf(*table, "best");
	const auto result = ColumnOf(*table, "result");
	const auto bestOut = ColumnOf(*table, "best_out");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const map_coords::MapCoords hand {.x = static_cast<int32_t>(table->Bits(row, handX)),
		                                  .z = static_cast<int32_t>(table->Bits(row, handZ))};
		const map_coords::MapCoords villager {.x = static_cast<int32_t>(table->Bits(row, villagerX)),
		                                      .z = static_cast<int32_t>(table->Bits(row, villagerZ))};
		const float nearness =
		    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(hand, villager), table->Float(row, power));
		const float each = rules::FreeBuilderValue(table->Float(row, pull), table->Float(row, life), nearness);
		float kept = table->Float(row, best);
		const bool better = each > kept;
		if (better)
		{
			// The game keeps how near the villager is as the best, not its whole worth
			kept = nearness;
		}
		if (static_cast<uint32_t>(better) != table->Bits(row, result) || !Same(kept, table->Bits(row, bestOut)))
		{
			Differs(different, row, "the game keeps " + Hex(table->Bits(row, bestOut)) + ", we keep " + oracle::Hex(kept));
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleComputerWorship, PlaceOfAWorkerForASite)
{
	const auto table = Loaded("computer_worship_worker_drop.txt", 2000);
	ASSERT_TRUE(table.has_value());
	const auto x = ColumnOf(*table, "x");
	const auto z = ColumnOf(*table, "z");
	const auto altitude = ColumnOf(*table, "altitude");
	const auto radius = ColumnOf(*table, "radius");
	const auto outX = ColumnOf(*table, "out_x");
	const auto outZ = ColumnOf(*table, "out_z");
	const auto outAltitude = ColumnOf(*table, "out_altitude");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const map_coords::MapCoords building {.x = static_cast<int32_t>(table->Bits(row, x)),
		                                      .z = static_cast<int32_t>(table->Bits(row, z)),
		                                      .altitude = table->Float(row, altitude)};
		const float distance = rules::SiteWorkerDropRadius(table->Float(row, radius));
		const auto place = rules::SiteWorkerDropPlace(
		    building, map_coords::MapCoords {}, [distance](float angle) { return gutils::GetPosFromAngle(angle, distance); },
		    [](const map_coords::MapCoords& point) { return map_coords::InBounds(point); },
		    [](const map_coords::MapCoords& /*point*/) { return false; });
		if (static_cast<uint32_t>(place.x) != table->Bits(row, outX) ||
		    static_cast<uint32_t>(place.z) != table->Bits(row, outZ) || !Same(place.altitude, table->Bits(row, outAltitude)))
		{
			Differs(different, row,
			        "the game puts it at " + std::to_string(table->Bits(row, outX)) + ", " +
			            std::to_string(table->Bits(row, outZ)) + ", we put it at " + std::to_string(place.x) + ", " +
			            std::to_string(place.z));
		}
	}
	EXPECT_EQ(different, 0u);
}
