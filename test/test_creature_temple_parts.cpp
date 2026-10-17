/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureTempleParts.h"
#include "ECS/MapCells.h"

using namespace openblack;

namespace
{
/// The outline of a small model, a unit box two units a side, set down at a place on the land
ecs::map_cells::Outline SmallOutlineAt(glm::vec3 position)
{
	return ecs::map_cells::OutlineOfBox(position, glm::mat3(1.0f), 1.0f, glm::vec3(0.0f), glm::vec3(1.0f));
}
} // namespace

// An altar stands in the cell under it, as a building does, and in no cell far from it
TEST(CreatureTempleParts, AnAltarStandsInTheCellsThatItCovers)
{
	const auto outline = SmallOutlineAt({1005.0f, 0.0f, 2005.0f});
	EXPECT_TRUE(creature_temple_parts::AltarIn(outline, {100, 200}));
	EXPECT_FALSE(creature_temple_parts::AltarIn(outline, {103, 200}));
	EXPECT_FALSE(creature_temple_parts::AltarIn(outline, {100, 197}));
	// Each cell that the map gives for the outline has the altar
	for (const auto cell : ecs::map_cells::CellsCovered(outline))
	{
		EXPECT_TRUE(creature_temple_parts::AltarIn(outline, cell));
	}
}

// The altars in a cell come first, as the newest of the things that stay put, then the things of the map in its order
TEST(CreatureTempleParts, TheAltarsInACellAreSeenFirst)
{
	const std::array altars {entt::entity {7}, entt::entity {8}};
	const std::array inCell {entt::entity {1}, entt::entity {2}, entt::entity {3}};
	const auto seen = creature_temple_parts::ThingsSeenInCell(altars, inCell);
	const std::vector expected {entt::entity {7}, entt::entity {8}, entt::entity {1}, entt::entity {2}, entt::entity {3}};
	EXPECT_EQ(seen, expected);
	EXPECT_EQ(creature_temple_parts::ThingsSeenInCell({}, inCell), std::vector<entt::entity>(inCell.begin(), inCell.end()));
	EXPECT_TRUE(creature_temple_parts::ThingsSeenInCell({}, {}).empty());
}
