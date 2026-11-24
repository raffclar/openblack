/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The registry marks each row of the components of k_MarksHashRows that a write can change, for the hash of the world

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/FixedOutline.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/MapPlace.h"
#include "ECS/Reflection/RowMarks.h"
#include "ECS/Registry.h"

using openblack::ecs::Registry;
using openblack::ecs::components::FixedOutline;
using openblack::ecs::components::MapCellResident;
using openblack::ecs::components::MapPlace;
using openblack::ecs::reflection::RowMarks;

namespace
{

template <typename Component>
bool Marked(const RowMarks& marks, entt::entity entity)
{
	const auto* found = marks.Of(entt::type_hash<Component>::value());
	return found != nullptr && found->Has(entity);
}

/// Gives the marks to the memo, as a hash that keeps each row does
void HashOnce(Registry& registry, RowMarks& memo)
{
	registry.GiveRowMarks(memo);
	memo.Used();
}

} // namespace

TEST(RegistryRowMarks, AnAddAReplaceAndAWriteMarkTheRow)
{
	Registry registry;
	RowMarks memo;
	const auto first = registry.Create();
	const auto second = registry.Create();
	registry.Assign<MapPlace>(first);
	registry.Assign<MapPlace>(second);
	registry.Assign<MapCellResident>(first);
	HashOnce(registry, memo);
	EXPECT_TRUE(Marked<MapPlace>(memo, first));
	EXPECT_TRUE(Marked<MapPlace>(memo, second));
	HashOnce(registry, memo);
	EXPECT_FALSE(Marked<MapPlace>(memo, first));
	EXPECT_FALSE(Marked<MapCellResident>(memo, first));
	registry.AssignOrReplace<MapPlace>(second);
	registry.Write<MapCellResident>(first, [](MapCellResident& resident) { resident.cells.push_back(3); });
	registry.Assign<FixedOutline>(second);
	registry.GiveRowMarks(memo);
	EXPECT_TRUE(memo.Usable(&registry.Underlying()));
	EXPECT_FALSE(Marked<MapPlace>(memo, first));
	EXPECT_TRUE(Marked<MapPlace>(memo, second));
	EXPECT_TRUE(Marked<MapCellResident>(memo, first));
	EXPECT_TRUE(Marked<FixedOutline>(memo, second));
}

TEST(RegistryRowMarks, AResetAndADirectWriteMakeTheNextMarksIncomplete)
{
	Registry registry;
	RowMarks memo;
	HashOnce(registry, memo);
	HashOnce(registry, memo);
	registry.GiveRowMarks(memo);
	EXPECT_TRUE(memo.Usable(&registry.Underlying()));
	memo.Used();
	registry.StoresWritten();
	registry.GiveRowMarks(memo);
	EXPECT_FALSE(memo.Usable(&registry.Underlying()));
	memo.Used();
	registry.Reset();
	registry.GiveRowMarks(memo);
	EXPECT_FALSE(memo.Usable(&registry.Underlying()));
}

TEST(RegistryRowMarks, TheMarksOfOneRegistryAreNotUsableForAnother)
{
	Registry first;
	Registry second;
	RowMarks memo;
	HashOnce(first, memo);
	HashOnce(second, memo);
	first.GiveRowMarks(memo);
	EXPECT_FALSE(memo.Usable(&first.Underlying()));
}
