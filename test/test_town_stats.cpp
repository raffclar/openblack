/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// town_stats: the mesh id of an info record (MeshIdHash) is the resource id resources::HashIdentifier gives, and
// AbodesOf keeps the order of a sort on each abode's creation index, ties included.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownStats.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace town_stats = openblack::ecs::town_stats;

namespace
{
class TownStatsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		_town = Reg().Create();
		Reg().Assign<Town>(_town).id = k_TownId;
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static constexpr uint32_t k_TownId = 3;
	entt::entity _town {entt::null};
};
} // namespace

TEST(TownStatsMeshIdHash, IsTheResourceIdOfTheMesh)
{
	for (uint32_t id = 0; id < 5000; ++id)
	{
		const auto mesh = static_cast<MeshId>(id);
		ASSERT_EQ(town_stats::MeshIdHash(mesh), resources::HashIdentifier(mesh)) << id;
	}
	for (const uint32_t id : {9999u, 10000u, 65535u, 1000000u, 4294967295u})
	{
		const auto mesh = static_cast<MeshId>(id);
		EXPECT_EQ(town_stats::MeshIdHash(mesh), resources::HashIdentifier(mesh)) << id;
	}
}

TEST_F(TownStatsTest, AbodesOfIsTheSortOnTheCreationIndexTiesIncluded)
{
	// more abodes than the sort's small-range threshold, with repeated indices and abodes without one (-1), between
	// abodes of another town
	std::vector<entt::entity> abodes;
	for (uint32_t i = 0; i < 90; ++i)
	{
		const auto abode = Reg().Create();
		Reg().Assign<Abode>(abode).townId = i % 4 == 3 ? k_TownId + 1 : k_TownId;
		if (i % 5 != 0)
		{
			Reg().Assign<ObjectCreationIndex>(abode, (i * 7u) % 23u);
		}
	}
	// the order the abodes had before: the town's abodes in the storage's order, sorted on Of() looked up each time
	Reg().Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId == k_TownId)
		{
			abodes.push_back(entity);
		}
	});
	std::sort(abodes.begin(), abodes.end(),
	          [](entt::entity a, entt::entity b) { return ecs::object_index::Of(a) > ecs::object_index::Of(b); });
	EXPECT_EQ(town_stats::AbodesOf(_town), abodes);
	EXPECT_TRUE(town_stats::AbodesOf(Reg().Create()).empty()); // not a town
}
