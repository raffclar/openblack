/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/MapProduction.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

class MapProductionFiling: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::entitiesMap::emplace<MapProduction>();
	}
	void TearDown() override
	{
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}
};

TEST_F(MapProductionFiling, AMadeFootballIsPutOnTheMapAtOnce)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto ball = registry.Create();
	registry.Assign<Transform>(ball, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mobile>(ball);
	registry.Assign<Ball>(ball);
	Locator::entitiesMap::value().Sync();
	EXPECT_TRUE(registry.AllOf<MapCellResident>(ball));
}

TEST_F(MapProductionFiling, AHandfulRefiledAsItIsMadeIsOnTheMapAtOnceAndOnlyOnce)
{
	// A creature picks up a handful from a pile in the same turn that it makes the handful. Thus the handful must be in
	// the map before a query of the map.
	auto& registry = Locator::entitiesRegistry::value();
	auto& map = Locator::entitiesMap::value();
	const auto handful = registry.Create();
	registry.Assign<Transform>(handful, glm::vec3(500.0f, 0.0f, 500.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Pot>(handful, Pot {.amount = 300, .maxAmount = 600, .type = PotInfo::WheatInHand});
	EXPECT_FALSE(registry.AllOf<MapCellResident>(handful));
	map.Refile(handful);
	ASSERT_TRUE(registry.AllOf<MapCellResident>(handful));
	const auto cell = map_coords::CellOf(glm::vec3(500.0f, 0.0f, 500.0f));
	const auto inCell = map.GetAllInCell(cell);
	EXPECT_EQ(std::ranges::count(inCell, handful), 1);
}
