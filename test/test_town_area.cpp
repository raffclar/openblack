/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The town rectangle (areaMin / areaMax) as the original keeps it: SetTownArea
// from AddStructureToTown (the newcomer, still without its 3D object, is a point), the field's own extension
// (ExtendTownArea, the field's 5.0) and RemoveStructureFromTown. Fields (Get2DRadius 5.0)
// give a radius without meshes.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownPlacement.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace town_placement = openblack::ecs::town_placement;
namespace map_coords = openblack::map_coords;

namespace
{
constexpr uint32_t k_TownId = 1;

class TownAreaTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::infoConstants::reset(new InfoConstants());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		auto& registry = Reg();
		_town = registry.Create();
		registry.Assign<Town>(_town).id = k_TownId;
		registry.Context().towns[k_TownId] = _town;
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	const Town& TownData() { return Reg().Get<const Town>(_town); }

	/// A field of the town at (x, 0, z) metres (Get2DRadius 5.0)
	entt::entity MakeField(float x, float z)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(x, 0.0f, z), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Abode>(e, AbodeNumber::Field, k_TownId, 0u, 0u);
		registry.Assign<Field>(e, static_cast<int>(k_TownId));
		return e;
	}

	entt::entity _town {entt::null};
};

// AddStructureToTown: the newcomer (no 3D object yet) widens the rectangle by its point; the others by their
// radius
TEST_F(TownAreaTest, NewcomerIsAPoint)
{
	const auto first = MakeField(100.0f, 100.0f);
	town_placement::SetTownArea(_town, first);
	EXPECT_EQ(TownData().areaMin.x, map_coords::ToFixedGUtils(100.0f));
	EXPECT_EQ(TownData().areaMax.x, map_coords::ToFixedGUtils(100.0f));
	const auto second = MakeField(200.0f, 150.0f);
	town_placement::SetTownArea(_town, second);
	EXPECT_EQ(TownData().areaMin.x, map_coords::ToFixedGUtils(95.0f));
	EXPECT_EQ(TownData().areaMin.y, map_coords::ToFixedGUtils(95.0f));
	EXPECT_EQ(TownData().areaMax.x, map_coords::ToFixedGUtils(200.0f));
	EXPECT_EQ(TownData().areaMax.y, map_coords::ToFixedGUtils(150.0f));
}

// A new field then adds its own 5.0 (ExtendTownArea)
TEST_F(TownAreaTest, FieldCtorExtendsByItsRadius)
{
	const auto field = MakeField(200.0f, 150.0f);
	town_placement::SetTownArea(_town, field);
	town_placement::ExtendTownArea(_town, field);
	EXPECT_EQ(TownData().areaMin.x, map_coords::ToFixedGUtils(195.0f));
	EXPECT_EQ(TownData().areaMax.x, map_coords::ToFixedGUtils(205.0f));
	EXPECT_EQ(TownData().areaMin.y, map_coords::ToFixedGUtils(145.0f));
	EXPECT_EQ(TownData().areaMax.y, map_coords::ToFixedGUtils(155.0f));
}

// The rectangle is not recomputed when read: it stays as the last SetTownArea left it
TEST_F(TownAreaTest, KeptUntilTheNextSetTownArea)
{
	const auto field = MakeField(100.0f, 100.0f);
	town_placement::SetTownArea(_town, field);
	const auto centre = town_placement::GetTownAreaCentre(_town);
	EXPECT_EQ(centre.x, map_coords::ToFixedGUtils(100.0f));
	// a later SetTownArea (another abode's add or remove) gives the field its radius
	town_placement::SetTownArea(_town);
	EXPECT_EQ(TownData().areaMin.x, map_coords::ToFixedGUtils(95.0f));
	EXPECT_EQ(TownData().areaMax.x, map_coords::ToFixedGUtils(105.0f));
}

// SetTownArea's start values for a town with no abode
TEST_F(TownAreaTest, EmptyTown)
{
	town_placement::SetTownArea(_town);
	EXPECT_EQ(TownData().areaMin.x, 0x7FFFFFFF);
	EXPECT_EQ(TownData().areaMax.x, 0);
}
} // namespace
