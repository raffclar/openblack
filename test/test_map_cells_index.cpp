/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The map cells system's flat link index (ECS/Systems/Implementations/MapCellsSystem): every answer and every order is
// the one the plain map of links gave. The map-only system is kept here as the reference; the same operations run on
// both, first on the system alone (insertions, removals and clears in a mixed order, with recycled indices that are
// linked twice at once), then through the map cells' queries on a synthetic grid.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByParticleSystem.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Locator.h"
#include "support/MapFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using map_cells::lists::Cell;
using map_cells::lists::Link;
using map_cells::lists::ReadFilter;

namespace
{
/// The map cells system as it was before the index: every link lookup is a find in the map
class MapOnlyCellsSystem final: public systems::MapCellsSystemInterface
{
public:
	[[nodiscard]] Cell* CellAt(glm::ivec2 cell) override
	{
		if (!map_coords::InBounds(cell))
		{
			return nullptr;
		}
		if (_cells.empty())
		{
			_cells.resize(static_cast<std::size_t>(map_coords::k_MapCells) * map_coords::k_MapCells);
		}
		return &_cells[IndexOf(cell)];
	}
	[[nodiscard]] const Cell* CellIfAny(glm::ivec2 cell) const override
	{
		if (!map_coords::InBounds(cell) || _cells.empty())
		{
			return nullptr;
		}
		return &_cells[IndexOf(cell)];
	}
	[[nodiscard]] std::span<const Cell> Cells() const override { return _cells; }

	[[nodiscard]] Link* LinkOf(entt::entity object) override
	{
		const auto found = _links.find(object);
		return found != _links.end() ? &found->second : nullptr;
	}
	Link& AddLink(entt::entity object, Link link) override { return _links.emplace(object, std::move(link)).first->second; }
	void RemoveLink(entt::entity object) override { _links.erase(object); }
	[[nodiscard]] const std::unordered_map<entt::entity, Link>& Links() const override { return _links; }

	void Clear() override
	{
		_cells.clear();
		_links.clear();
	}
	void UseRegistry(const Registry* registry) override
	{
		if (registry != _registry)
		{
			Clear();
			_registry = registry;
		}
	}

	void BeginReadBatch() override
	{
		if (_batchDepth++ == 0)
		{
			_batchFilter.emplace();
		}
	}
	void EndReadBatch() override
	{
		if (--_batchDepth == 0)
		{
			_batchFilter.reset();
		}
	}
	[[nodiscard]] const ReadFilter* BatchFilter() const override { return _batchFilter ? &*_batchFilter : nullptr; }

private:
	static std::size_t IndexOf(glm::ivec2 cell)
	{
		return static_cast<std::size_t>(cell.x) * map_coords::k_MapCells + static_cast<std::size_t>(cell.y);
	}

	std::vector<Cell> _cells;
	std::unordered_map<entt::entity, Link> _links;
	const Registry* _registry {nullptr};
	std::optional<ReadFilter> _batchFilter;
	int _batchDepth {0};
};

entt::entity EntityOf(uint32_t index, uint32_t version)
{
	using Traits = entt::entt_traits<entt::entity>;
	return Traits::construct(static_cast<Traits::entity_type>(index), static_cast<Traits::version_type>(version));
}

/// The links' walk (entity and tag, in the map's order)
std::vector<std::pair<entt::entity, int32_t>> WalkOf(const systems::MapCellsSystemInterface& lists)
{
	std::vector<std::pair<entt::entity, int32_t>> walk;
	for (const auto& [entity, link] : lists.Links())
	{
		walk.emplace_back(entity, link.x);
	}
	return walk;
}

TEST(MapCellsIndex, EveryLookupAndTheMapsOrderAsTheMapAlone)
{
	// 16 indices x 4 versions, so recycled indices are linked twice at once; a far index; null and the tombstone
	std::vector<entt::entity> probes;
	for (uint32_t index = 0; index < 16; ++index)
	{
		for (uint32_t version = 0; version < 4; ++version)
		{
			probes.push_back(EntityOf(index, version));
		}
	}
	probes.push_back(EntityOf(5000, 0));
	probes.push_back(entt::null);
	probes.push_back(entt::tombstone);

	systems::MapCellsSystem indexed;
	MapOnlyCellsSystem reference;
	std::mt19937 random(20261008u); // the test's own sequence, not the game's
	int32_t tag = 0;
	for (int step = 0; step < 20000; ++step)
	{
		const auto object = probes[random() % probes.size()];
		const auto op = random() % 100;
		if (op < 55)
		{
			Link link;
			link.x = ++tag;
			auto& a = indexed.AddLink(object, link);
			auto& b = reference.AddLink(object, link);
			ASSERT_EQ(a.x, b.x) << "step " << step; // an object already linked keeps its own
			ASSERT_EQ(&a, &indexed.Links().at(object));
		}
		else if (op < 99)
		{
			indexed.RemoveLink(object);
			reference.RemoveLink(object);
		}
		else
		{
			indexed.Clear();
			reference.Clear();
		}
		ASSERT_EQ(WalkOf(indexed), WalkOf(reference)) << "step " << step;
		for (const auto probe : probes)
		{
			const auto* expected = reference.LinkOf(probe);
			const auto* found = indexed.LinkOf(probe);
			ASSERT_EQ(found == nullptr, expected == nullptr) << "step " << step;
			if (found != nullptr)
			{
				ASSERT_EQ(found, &indexed.Links().at(probe)) << "step " << step; // the map's own node
				ASSERT_EQ(found->x, expected->x) << "step " << step;
			}
		}
	}
}

// ---- Through the map cells' queries ---------------------------------------------------------------------------------

/// Everything the readers see, as one sequence of numbers
class Recorder
{
public:
	void Entity(entt::entity e) { _values.push_back(static_cast<uint64_t>(entt::to_integral(e))); }
	void Value(uint64_t value) { _values.push_back(value); }
	void List(const std::vector<entt::entity>& list)
	{
		Value(list.size());
		for (const auto e : list)
		{
			Entity(e);
		}
	}
	[[nodiscard]] const std::vector<uint64_t>& Values() const { return _values; }

private:
	std::vector<uint64_t> _values;
};

/// The cells the scenario uses: the rocks' 2 x 2 at the corner, and around (5, 5)
const std::vector<glm::ivec2> k_Cells {{0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 1}, {5, 5}, {5, 6}, {6, 5}, {6, 6}};

void RecordQueries(Recorder& out, const std::vector<entt::entity>& made)
{
	for (const auto cell : k_Cells)
	{
		out.List(map_cells::ObjectsInCell(cell));
		out.List(map_cells::MobileInCell(cell));
		std::vector<entt::entity> walk;
		map_cells::ForEachInCell(cell, [&walk](entt::entity e) {
			walk.push_back(e);
			return true;
		});
		out.List(walk);
		walk.clear();
		map_cells::ForEachFixed(cell, [&walk](entt::entity e) {
			walk.push_back(e);
			return true;
		});
		out.List(walk);
		walk.clear();
		map_cells::ForEachMobile(cell, [&walk](entt::entity e) {
			walk.push_back(e);
			return true;
		});
		out.List(walk);
		for (const auto type :
		     {ObjectType::Any, ObjectType::ForestTree, ObjectType::Pot, ObjectType::Animal, ObjectType::MobileStatic})
		{
			walk.clear();
			for (auto e = map_cells::FindType(cell, type); e != entt::null; e = map_cells::FindType(cell, type, e))
			{
				walk.push_back(e);
			}
			out.List(walk);
		}
		walk.clear();
		for (auto e = map_cells::FindFixedOnMap(cell); e != entt::null; e = map_cells::FindFixedOnMap(cell, e))
		{
			walk.push_back(e);
		}
		out.List(walk);
		out.Value(map_cells::IsFixed(cell) ? 1 : 0);
		out.Entity(map_cells::FirstFixed(cell));
		out.Entity(map_cells::FirstMobile(cell));
		const glm::vec2 centre(static_cast<float>(cell.x) * 10.0f + 5.0f, static_cast<float>(cell.y) * 10.0f + 5.0f);
		for (const auto offset : {glm::vec2(0.0f), glm::vec2(0.4f, 0.0f), glm::vec2(-3.0f, 2.0f), glm::vec2(4.6f, -4.6f)})
		{
			const auto coords = map_coords::FromMetres(centre + offset);
			out.Value(map_cells::Collide(coords));
			out.Value(map_cells::CollideWithFixed(coords));
		}
	}
	const auto here = map_coords::FromMetres(glm::vec2(52.0f, 57.0f));
	for (const auto type : {ObjectType::Any, ObjectType::ForestTree, ObjectType::Pot, ObjectType::Animal})
	{
		out.Entity(map_cells::FindNearType(here, type, 20.0f));
	}
	out.Entity(map_cells::FindNearForScript(
	    here, [](entt::entity) { return true; }, 20.0f));
	out.Entity(map_cells::FindNearestInSpiral(
	    here, [](entt::entity) { return true; }, 30.0f));
	for (const auto e : made)
	{
		out.Value(map_cells::IsObjectInMap(e) ? 1 : 0);
		out.Value(map_cells::CollideDataOf(e) != nullptr ? 1 : 0);
	}
	out.Value(map_cells::ObjectCount());
	out.Value(map_cells::CheckConsistency());
}

Registry& Reg()
{
	return Locator::entitiesRegistry::value();
}

entt::entity Make(const glm::vec3& position)
{
	const auto e = Reg().Create();
	object_index::Assign(e);
	Reg().Assign<Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return e;
}

template <typename Class>
entt::entity MakeOf(std::vector<entt::entity>& made, const glm::vec3& position)
{
	const auto e = Make(position);
	Reg().Assign<Class>(e);
	made.push_back(e);
	return e;
}

/// The same scenario with the given map cells system: inserts, removals, moves, turns, a storm, a deletion and a
/// recycled index linked while the deleted one still is, in a mixed order; every query recorded after each step
template <typename System>
std::vector<uint64_t> RunScenario()
{
	test::EmplaceMapAndVillagerDefaults();
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	test::EmplaceWorldSystems();
	Locator::mapCellsSystem::emplace<System>();
	map_cells::Clear();
	object_index::OnLoadMap();
	// a rock's shape: a 3 m circle round its position, reach 4 (its 2 x 2 cells)
	test::FakeMapShapeProvider shapeProvider;
	shapeProvider.meshShape = [](entt::entity object, map_collide::Shape& shape, float& reach) {
		const auto& position = Reg().Get<const Transform>(object).position;
		shape = {{position.x, position.z}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	};
	Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>(shapeProvider);

	Recorder out;
	std::vector<entt::entity> made;
	const glm::vec3 p(55.0f, 0.0f, 55.0f);
	const auto step = [&] { RecordQueries(out, made); };

	const auto treeA = MakeOf<Tree>(made, p);
	const auto animalA = MakeOf<Animal>(made, p);
	const auto rockA = MakeOf<MobileStatic>(made, glm::vec3(10.0f, 0.0f, 10.0f));
	const auto potA = MakeOf<Pot>(made, glm::vec3(52.0f, 0.0f, 52.0f));
	const auto animalB = MakeOf<Animal>(made, p);
	const auto treeB = MakeOf<Tree>(made, glm::vec3(5.0f, 0.0f, 5.0f));
	for (const auto e : {animalA, rockA, potA, treeB, animalB, treeA})
	{
		map_cells::InsertMapObject(e);
	}
	step();
	const auto rockB = MakeOf<MobileStatic>(made, glm::vec3(12.0f, 0.0f, 8.0f));
	const auto animalC = MakeOf<Animal>(made, p);
	const auto potB = MakeOf<Pot>(made, p);
	map_cells::Sync(); // the new ones by creation index
	step();
	map_cells::RemoveMapObject(animalB); // the middle of the mobile list
	map_cells::RemoveMapObject(rockA);   // a multi-cell one, from every cell
	step();
	map_cells::MoveMapObject(animalA, glm::vec3(65.0f, 0.0f, 55.0f)); // another cell
	map_cells::MoveMapObject(animalC, glm::vec3(56.0f, 0.0f, 56.0f)); // the same cell: no change
	map_cells::MoveMapObject(rockB, glm::vec3(13.0f, 0.0f, 9.0f));    // any MapCoords change
	map_cells::InsertMapObject(rockA);
	map_cells::InsertMapObject(animalB);
	step();
	map_cells::OnAnglesOrScaleChanged(treeA); // to the head of its list
	Reg().Get<Transform>(potB).position = glm::vec3(65.0f, 0.0f, 65.0f);
	Reg().Get<Transform>(animalB).position = glm::vec3(54.0f, 0.0f, 65.0f);
	map_cells::Sync(); // the moves found by Sync
	step();
	// a storm carries the tree off; the first animal is deleted without a removal, and its index is used again at once
	map_cells::RemoveMapObject(treeA);
	Reg().Assign<CarriedByParticleSystem>(treeA);
	Reg().Destroy(animalA);
	const auto again = MakeOf<Animal>(made, glm::vec3(66.0f, 0.0f, 54.0f));
	EXPECT_EQ(entt::to_entity(again), entt::to_entity(animalA)); // the recycled index, linked with the old one
	map_cells::InsertMapObject(again);
	step();
	map_cells::RemoveMapObject(animalA); // the deleted one is still linked until Sync
	step();
	map_cells::InsertMapObject(animalA); // not valid: KindOf says none
	map_cells::Sync();                   // drops the deleted one, the carried one stays out
	step();
	Reg().Remove<CarriedByParticleSystem>(treeA);
	Reg().Destroy(potA);
	const auto potC = MakeOf<Pot>(made, glm::vec3(5.0f, 0.0f, 5.0f));
	map_cells::Sync();
	step();
	for (const auto e : {rockB, again, treeB, potC, animalC})
	{
		map_cells::RemoveMapObject(e);
	}
	step();
	map_cells::Sync();
	step();

	map_cells::Clear();
	Locator::entitiesRegistry::reset();
	test::ResetWorldSystems();
	test::ResetMapAndVillagerDefaults();
	return out.Values();
}

TEST(MapCellsIndex, EveryQueryAndOrderAsTheMapAlone)
{
	const auto reference = RunScenario<MapOnlyCellsSystem>();
	const auto indexed = RunScenario<systems::MapCellsSystem>();
	ASSERT_FALSE(reference.empty());
	EXPECT_EQ(indexed, reference);
}
} // namespace
