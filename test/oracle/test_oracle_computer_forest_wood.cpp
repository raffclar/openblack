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
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMindRules.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

namespace
{
constexpr uint32_t k_NoThing = 0xFFFFFFFFU;
constexpr uint32_t k_Forest = 1;
constexpr uint32_t k_LoneTree = 2;

/// The columns of one thing that the god saw
struct ThingColumns
{
	std::optional<size_t> kind, count, wood, radius, x, z, script;
};
} // namespace

TEST(OracleComputerForestWood, TreeForTheTownAsTheGameChoosesIt)
{
	const auto table = oracle::Table::Load("computer_forest_wood.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 600U);
	const auto handX = table->Column("handX");
	const auto handZ = table->Column("handZ");
	const auto pointX = table->Column("pointX");
	const auto pointZ = table->Column("pointZ");
	const auto power = table->Column("power");
	const auto pull = table->Column("pull");
	const auto value = table->Column("value");
	const auto index = table->Column("index");
	ASSERT_TRUE(handX && handZ && pointX && pointZ && power && pull && value && index);
	std::array<ThingColumns, 3> things;
	for (size_t i = 0; i < things.size(); ++i)
	{
		const auto n = std::to_string(i);
		things.at(i) = {table->Column("kind" + n),   table->Column("count" + n), table->Column("wood" + n),
		                table->Column("radius" + n), table->Column("x" + n),     table->Column("z" + n),
		                table->Column("script" + n)};
		const auto& c = things.at(i);
		ASSERT_TRUE(c.kind && c.count && c.wood && c.radius && c.x && c.z && c.script);
	}

	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const map_coords::MapCoords hand {.x = static_cast<int32_t>(table->Bits(row, *handX)),
		                                  .z = static_cast<int32_t>(table->Bits(row, *handZ)),
		                                  .altitude = 0.0f};
		// The god measures the trees from the hand point that it reported, as a map position
		const map_coords::MapCoords reported {.x = map_coords::ToFixed(table->Float(row, *pointX)),
		                                      .z = map_coords::ToFixed(table->Float(row, *pointZ)),
		                                      .altitude = 0.0f};
		const float influencePower = table->Float(row, *power);
		std::vector<float> wishes;
		std::vector<size_t> indices;
		std::vector<map_coords::MapCoords> places;
		for (size_t i = 0; i < things.size(); ++i)
		{
			const auto& c = things.at(i);
			const auto kind = table->Bits(row, *c.kind);
			const auto grown = table->Bits(row, *c.count);
			if (kind == 0 || table->Bits(row, *c.script) != 0 || (kind == k_Forest && grown == 0))
			{
				continue;
			}
			const map_coords::MapCoords tree {.x = static_cast<int32_t>(table->Bits(row, *c.x)),
			                                  .z = static_cast<int32_t>(table->Bits(row, *c.z)),
			                                  .altitude = 0.0f};
			SeenWood seen {.forest = kind == k_Forest,
			               .grownTrees = grown,
			               .treeWood = kind == k_Forest ? table->Float(row, *c.wood) : 0.0f,
			               .kindWood = kind == k_LoneTree ? table->Bits(row, *c.wood) : 0U,
			               .pull = kind == k_Forest ? table->Float(row, *pull) : 0.0f,
			               .distanceToEdge = gutils::GetDistanceInMetres(tree, reported) - table->Float(row, *c.radius)};
			wishes.push_back(SeenWoodWish(seen, influencePower));
			indices.push_back(i);
			places.push_back(tree);
		}
		const auto best = MostWanting(wishes);
		const uint32_t ourIndex = best.has_value() ? static_cast<uint32_t>(indices.at(*best)) : k_NoThing;
		const float ourValue =
		    best.has_value() ? ChosenTreeWish(gutils::GetDistanceInMetres(hand, places.at(*best)), influencePower) : 0.0f;
		const bool same = ourIndex == table->Bits(row, *index) && std::bit_cast<uint32_t>(ourValue) == table->Bits(row, *value);
		if (!same && different++ < 10)
		{
			ADD_FAILURE() << "row " << row << ": ours thing " << ourIndex << " value " << oracle::Hex(ourValue)
			              << ", the game's " << table->Bits(row, *index);
		}
	}
	EXPECT_EQ(different, 0U);
}
