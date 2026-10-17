/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingModel.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/MapProduction.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr entt::id_type k_AltarModel = 0xA17A2;
constexpr entt::id_type k_DrawnModel = 0xD2A3;
/// A place in the middle of one map cell
constexpr glm::vec3 k_InCell {1005.0f, 0.0f, 2005.0f};

/// Each thing that covers its outline covers a box two units on a side at its place, so it stays in one cell
std::optional<map_cells::Outline> SmallOutline(const Registry& registry, entt::entity entity)
{
	const auto& transform = registry.Get<const Transform>(entity);
	return map_cells::OutlineOfBox(transform.position, transform.rotation, transform.scale.x, glm::vec3(0.0f), glm::vec3(1.0f));
}

class AltarInTheMap: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		Locator::entitiesMap::emplace<MapProduction>(&SmallOutline);
	}
	void TearDown() override
	{
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}

	static Registry& Entities() { return Locator::entitiesRegistry::value(); }
	static MapInterface& Map() { return Locator::entitiesMap::value(); }

	static entt::entity Placed(glm::vec3 at)
	{
		const auto entity = Entities().Create();
		Entities().Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}
	static entt::entity MakeSite(glm::vec3 at)
	{
		const auto site = Placed(at);
		Entities().Assign<WorshipSite>(site);
		return site;
	}
	static entt::entity MakeAltar(entt::entity site, glm::vec3 at)
	{
		const auto altar = Placed(at);
		Entities().Assign<WorshipAltar>(altar, WorshipAltar {.site = site, .model = k_AltarModel});
		return altar;
	}
	static entt::entity MakeFeature(glm::vec3 at)
	{
		const auto feature = Placed(at);
		Entities().Assign<Feature>(feature, FeatureInfo::AztcOlmechead);
		return feature;
	}
};
} // namespace

// The altar goes into the cells of its outline as a building does, at the front of the things that stay put
TEST_F(AltarInTheMap, TheAltarGoesIntoTheMapAsABuildingThatStaysPut)
{
	const auto altar = MakeAltar(entt::null, k_InCell);
	const auto kind = MapProduction::KindOf(Entities(), altar);
	ASSERT_TRUE(kind.has_value());
	EXPECT_EQ(kind->placement, map_cells::Placement::FixedFront);
	EXPECT_TRUE(kind->coversOutline);
	EXPECT_FALSE(kind->moves);
}

// The altar comes after its site, thus it is in front of the site. A thing that stays put and comes after the altar is
// in front of the altar. The things that move come after all the things that stay put.
TEST_F(AltarInTheMap, TheNewestThingThatStaysPutComesFirstInTheCell)
{
	const auto site = MakeSite(k_InCell);
	const auto altar = MakeAltar(site, k_InCell);
	const auto ball = Placed(k_InCell);
	Entities().Assign<Mobile>(ball);
	Entities().Assign<Ball>(ball);
	const auto feature = MakeFeature(k_InCell);

	const auto all = Map().GetAllInCell(map_coords::CellOf(k_InCell));
	const std::vector expected {feature, altar, site, ball};
	EXPECT_EQ(all, expected);
	EXPECT_TRUE(Entities().AllOf<MapCellResident>(altar));
}

// The altar goes from its cells when the game removes it, and the other things keep their order
TEST_F(AltarInTheMap, TheAltarLeavesItsCellsWhenItGoes)
{
	const auto site = MakeSite(k_InCell);
	const auto altar = MakeAltar(site, k_InCell);
	const auto feature = MakeFeature(k_InCell);
	const auto cell = map_coords::CellOf(k_InCell);
	ASSERT_EQ(std::ranges::count(Map().GetAllInCell(cell), altar), 1);

	Entities().Destroy(altar);
	const std::vector expected {feature, site};
	EXPECT_EQ(Map().GetAllInCell(cell), expected);
}

// The altar has its model for its size and shape from when the game makes its site, before the game shows the model
TEST_F(AltarInTheMap, TheAltarHasItsModelBeforeTheModelShows)
{
	const auto altar = MakeAltar(entt::null, k_InCell);
	EXPECT_EQ(thing_model::ModelOf(Entities(), altar), std::optional(k_AltarModel));
	Entities().Assign<Mesh>(altar, k_DrawnModel, int8_t {0}, int8_t {0});
	EXPECT_EQ(thing_model::ModelOf(Entities(), altar), std::optional(k_DrawnModel));
	EXPECT_FALSE(thing_model::ModelOf(Entities(), Placed(k_InCell)).has_value());
}

// Walkers and the search for an arena go round the box of the altar's model, as they do for a building
TEST_F(AltarInTheMap, WalkersAndArenasMeetTheAltarAsAModelBox)
{
	const auto altar = MakeAltar(entt::null, k_InCell);
	EXPECT_EQ(thing_circles::ShapeOf(Entities(), altar), wall_hug::ThingShape::ModelBox);
}

// Things that fly go through the altar: the game never builds the altar, so it is not in their way
TEST_F(AltarInTheMap, ThingsThatFlyGoThroughTheAltar)
{
	const auto altar = MakeAltar(entt::null, k_InCell);
	const auto info = std::make_unique<InfoConstants>();
	const auto facts = physics_classes::Classify(Entities(), altar, *info, physics_classes::ClassInputs {});
	EXPECT_FALSE(facts.interacts);
	EXPECT_FALSE(facts.canBecomePhysicsObject);
}
