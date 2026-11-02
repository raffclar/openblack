/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#include "MapProduction.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/FixedOutline.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FixedOutlineSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::ecs::map_cells::Placement;

namespace
{
/// Calls a function with each kind of component that puts a thing on the map
template <typename Func>
void ForEachMapComponent(Func&& func)
{
	// Buildings and features, which cover their outline
	func.template operator()<Abode>();
	func.template operator()<Feature>();
	func.template operator()<AnimatedStatic>();
	func.template operator()<Flowers>();
	func.template operator()<BigForest>();
	func.template operator()<MobileStatic>();
	func.template operator()<DeadTree>();
	func.template operator()<SpellDispenser>();
	func.template operator()<TeleportStone>();
	func.template operator()<WorshipSite>();
	// A worship site's altar, which stands in the land as a building does from when the game makes its site
	func.template operator()<WorshipAltar>();
	// A vortex, standing in the land as a rock does, over its unseen cylinder
	func.template operator()<Vortex>();
	// A temple's heart, which stands in the land as a building does
	func.template operator()<Temple>();
	// A town centre's totem, which covers the outline of its icon
	func.template operator()<VillageTotem>();
	// A worship site's icons, which stand in the land as buildings do. A village centre's stone hands are not on the map.
	func.template operator()<WorshipIcon>();
	// Trees, shields and the scripts' scrolls and signs, each in one cell
	func.template operator()<Tree>();
	func.template operator()<MagicShield>();
	func.template operator()<ShieldDome>();
	func.template operator()<ScriptHighlight>();
	// Fish farms, each in the cell it stands in, before the cell's other fixed things
	func.template operator()<FishFarm>();
	// Street lanterns and the flags of the towns' wants, each in the cell it stands in, after the cell's other fixed things
	func.template operator()<StreetLantern>();
	func.template operator()<TownDesireFlag>();
	// Things that count as staying put but can be carried
	func.template operator()<Pot>();
	func.template operator()<OneOffSpellSeed>();
	// The living and the other things that move
	func.template operator()<Villager>();
	func.template operator()<Creature>();
	func.template operator()<Animal>();
	func.template operator()<MobileObject>();
	func.template operator()<Scaffold>();
	func.template operator()<Ball>();
	func.template operator()<RewardOnLand>();
	// Fireflies, each in the cell of its place, before the cell's other things that move
	func.template operator()<Firefly>();
}

/// A building's outline on the ground, from its model's box as it is placed
std::optional<map_cells::Outline> OutlineOf(const Transform& transform, std::optional<entt::id_type> meshId)
{
	if (!meshId.has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(*meshId))
	{
		return std::nullopt;
	}
	const auto box = meshes.Handle(*meshId)->GetBoundingBox();
	return map_cells::OutlineOfBox(transform.position, transform.rotation, transform.scale.x, box.Center(), box.Size() * 0.5f);
}

/// The outline of a thing from its model in the resources
std::optional<map_cells::Outline> ModelOutlineOf(const Registry& registry, entt::entity entity)
{
	return OutlineOf(registry.Get<const Transform>(entity), thing_model::OutlineModelOf(registry, entity));
}
} // namespace

MapProduction::MapProduction()
    : MapProduction(&ModelOutlineOf)
{
}

MapProduction::MapProduction(OutlineSource outlines)
    : _outlines(std::move(outlines))
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	_registry = &Locator::entitiesRegistry::value();
	ForEachMapComponent([this]<typename Component>() {
		_connections.push_back(_registry->OnConstruct<Component>().template connect<&MapProduction::OnMade>(*this));
		_connections.push_back(_registry->OnDestroy<Component>().template connect<&MapProduction::OnGone>(*this));
	});
	_connections.push_back(_registry->OnDestroy<Transform>().connect<&MapProduction::OnGone>(*this));
	_connections.push_back(_registry->OnDestroy<MapCellResident>().connect<&MapProduction::OnResidentGone>(*this));
	_connections.push_back(_registry->OnDestroying().connect<&MapProduction::OnDestroying>(*this));
	// The lists that filing writes exist before any entity goes, so that the filing done as an entity goes adds no list
	static_cast<void>(_registry->Size<MapCellMover>());
	static_cast<void>(_registry->Size<FixedOutline>());
	// Anything already made is filed too, each thing once, in the order the components first name it
	std::unordered_set<entt::entity> listed(_made.begin(), _made.end());
	ForEachMapComponent([this, &listed]<typename Component>() {
		_registry->Each<const Component>([this, &listed](entt::entity entity, const auto&... /*unused*/) {
			if (listed.insert(entity).second)
			{
				_made.push_back(entity);
			}
		});
	});
}

MapProduction::~MapProduction()
{
	// The registry may have gone first, taking its signals with it
	if (Locator::entitiesRegistry::has_value() && &Locator::entitiesRegistry::value() == _registry)
	{
		for (auto& connection : _connections)
		{
			connection.release();
		}
	}
}

std::optional<MapProduction::Kind> MapProduction::KindOf(const Registry& registry, entt::entity entity)
{
	// A thing that shows only as a print on the land never goes into the map's cells
	if (registry.AnyOf<GroundPrint>(entity))
	{
		return std::nullopt;
	}
	if (registry.AnyOf<Abode, Feature, AnimatedStatic, Flowers, BigForest, SpellDispenser, TeleportStone, WorshipSite,
	                   WorshipAltar, Temple, VillageTotem, WorshipIcon>(entity))
	{
		return Kind {.placement = Placement::FixedFront, .coversOutline = true, .moves = false};
	}
	// Dead trees are rocks of a kind, so they cover their outline as other rocks do; so does a vortex
	if (registry.AnyOf<Vortex>(entity))
	{
		return Kind {.placement = Placement::FixedFront, .coversOutline = true, .moves = false};
	}
	// A piece that broke off a building is a rock. It goes into the cells only when it comes to rest, because nothing
	// files it when the game makes it.
	if (registry.AnyOf<MobileStatic, DeadTree, BuildingPiece>(entity))
	{
		return Kind {.placement = Placement::FixedFront, .coversOutline = true, .moves = true};
	}
	if (registry.AnyOf<Tree, MagicShield, ShieldDome, ScriptHighlight>(entity))
	{
		return Kind {.placement = Placement::FixedFront, .coversOutline = false, .moves = false};
	}
	if (registry.AnyOf<FishFarm>(entity))
	{
		return Kind {.placement = Placement::FixedFront, .coversOutline = false, .moves = false};
	}
	if (registry.AnyOf<StreetLantern>(entity))
	{
		return Kind {.placement = Placement::FixedBack, .coversOutline = false, .moves = false};
	}
	// A town's flag goes to the store that its town builds
	if (registry.AnyOf<TownDesireFlag>(entity))
	{
		return Kind {.placement = Placement::FixedBack, .coversOutline = false, .moves = true};
	}
	if (registry.AnyOf<Pot, OneOffSpellSeed>(entity))
	{
		return Kind {.placement = Placement::FixedBack, .coversOutline = false, .moves = true};
	}
	if (registry.AnyOf<Villager, Creature, Animal, MobileObject, Scaffold, Ball, RewardOnLand, Firefly>(entity))
	{
		return Kind {.placement = Placement::MobileFront, .coversOutline = false, .moves = true};
	}
	return std::nullopt;
}

void MapProduction::OnMade(entt::registry& /*registry*/, entt::entity entity)
{
	if (std::ranges::find(_made, entity) == _made.end())
	{
		_made.push_back(entity);
	}
}

void MapProduction::OnGone(entt::registry& /*registry*/, entt::entity entity)
{
	std::erase(_made, entity);
	TakeOut(entity);
}

void MapProduction::OnResidentGone(entt::registry& /*registry*/, entt::entity entity)
{
	TakeOut(entity);
}

void MapProduction::OnDestroying(entt::entity /*entity*/)
{
	// The things made before it went, itself too, are in their cells before it comes out of its own
	FileMadeBeforeLook();
}

void MapProduction::FileMadeBeforeLook() const
{
	if (_made.empty() || _registry == nullptr)
	{
		return;
	}
	auto made = std::move(_made);
	_made.clear();
	for (const auto entity : made)
	{
		// A thing stays out of the map's cells while a hand or a creature holds it, or while it moves in the physics.
		// A load makes again a thing that a creature holds. That thing stays out of the cells, as before the save.
		if (!_registry->Valid(entity) || _registry->AllOf<MapCellResident>(entity) ||
		    _registry->AnyOf<InHand, InPhysics, HeldByCreature>(entity))
		{
			continue;
		}
		if (!_registry->AllOf<Transform>(entity))
		{
			// Not placed yet: filed once it is
			_made.push_back(entity);
			continue;
		}
		if (const auto kind = KindOf(*_registry, entity))
		{
			File(entity, *kind);
		}
	}
}

std::vector<uint32_t> MapProduction::CellsFor(entt::entity entity, const Kind& kind) const
{
	std::vector<uint32_t> cells;
	if (kind.coversOutline)
	{
		const auto outline = _outlines(*_registry, entity);
		if (!outline.has_value())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Map: a building without a model can't be put in the map's cells");
			return cells;
		}
		for (const auto cell : map_cells::CellsCovered(*outline, _lists.CellsPerSide()))
		{
			if (const auto index = _lists.IndexOf(cell))
			{
				cells.push_back(*index);
			}
		}
		return cells;
	}
	if (const auto index = CellOfPlace(entity))
	{
		cells.push_back(*index);
	}
	return cells;
}

std::optional<uint32_t> MapProduction::CellOfPlace(entt::entity entity) const
{
	return _lists.IndexOf(map_coords::Cell(thing_map_place::Of(*_registry, entity)));
}

bool MapProduction::MovedSinceFiled(entt::entity entity, bool anyThing) const
{
	const auto* resident = _registry->TryGet<const MapCellResident>(entity);
	const auto* transform = _registry->TryGet<const Transform>(entity);
	if (resident == nullptr || transform == nullptr || (!anyThing && !_registry->AllOf<MapCellMover>(entity)))
	{
		return false;
	}
	// A building as soon as it is anywhere else
	if (resident->coversOutline)
	{
		return transform->position != resident->filedAt;
	}
	// Anything else once it is in another cell
	const auto now = CellOfPlace(entity);
	return resident->cells.empty() ? now.has_value() : (!now.has_value() || *now != resident->cells.front());
}

void MapProduction::KnowLandBlocks() const
{
	if (!_lists.Blocks().LandBlocksKnown() && Locator::terrainSystem::has_value())
	{
		std::vector<glm::ivec2> blocks;
		for (const auto& block : Locator::terrainSystem::value().GetBlocks())
		{
			if (block.GetLndBlock() != nullptr)
			{
				blocks.push_back(block.GetBlockPosition());
			}
		}
		_lists.Blocks().SetLandBlocks(blocks);
	}
}

void MapProduction::File(entt::entity entity, const Kind& kind) const
{
	// The land's blocks are known before anything goes on them
	KnowLandBlocks();
	auto cells = CellsFor(entity, kind);
	for (const auto cell : cells)
	{
		_lists.Insert(cell, entity, kind.placement);
	}
	const auto at = _registry->Get<const Transform>(entity).position;
	_registry->AssignOrReplace<MapCellResident>(entity, MapCellResident {.placement = kind.placement,
	                                                                     .coversOutline = kind.coversOutline,
	                                                                     .moves = kind.moves,
	                                                                     .cells = std::move(cells),
	                                                                     .filedAt = at});
	if (kind.moves && !_registry->AllOf<MapCellMover>(entity))
	{
		_registry->Assign<MapCellMover>(entity);
	}
	// A thing that stays put, coming onto the map, may send the footpaths round it
	if (kind.placement != Placement::MobileFront && Locator::footpathSystem::has_value())
	{
		Locator::footpathSystem::value().ThingPut(entity);
	}
	// A thing that stays put gets its outline on the ground each time it goes into the cells
	if (kind.placement != Placement::MobileFront && Locator::fixedOutlineSystem::has_value())
	{
		Locator::fixedOutlineSystem::value().Insert(entity);
	}
}

void MapProduction::TakeOut(entt::entity entity) const
{
	if (_registry == nullptr)
	{
		return;
	}
	auto* resident = _registry->TryGet<MapCellResident>(entity);
	if (resident == nullptr)
	{
		return;
	}
	const bool filed = !resident->cells.empty();
	for (const auto cell : resident->cells)
	{
		_lists.Remove(cell, entity);
	}
	resident->cells.clear();
	// A thing that stays put, going off the map, may bring the footpaths back from going round it
	if (resident->placement != Placement::MobileFront && Locator::footpathSystem::has_value())
	{
		Locator::footpathSystem::value().ThingRemoved(entity, resident->filedAt, filed);
	}
}

std::span<const entt::entity> MapProduction::GetFixedInGridCell(const CellId& cellId) const
{
	FileMadeBeforeLook();
	const auto index = _lists.IndexOf({cellId.x, cellId.y});
	return index.has_value() ? _lists.Fixed(*index) : std::span<const entt::entity> {};
}

std::span<const entt::entity> MapProduction::GetFixedInGridCell(const glm::vec3& pos) const
{
	return GetFixedInGridCell(GetGridCell(pos));
}

std::span<const entt::entity> MapProduction::GetFiledFixedInGridCell(const CellId& cellId) const
{
	const auto index = _lists.IndexOf({cellId.x, cellId.y});
	return index.has_value() ? _lists.Fixed(*index) : std::span<const entt::entity> {};
}

std::span<const entt::entity> MapProduction::GetFiledMobileInGridCell(const CellId& cellId) const
{
	const auto index = _lists.IndexOf({cellId.x, cellId.y});
	return index.has_value() ? _lists.Mobile(*index) : std::span<const entt::entity> {};
}

std::span<const entt::entity> MapProduction::GetMobileInGridCell(const CellId& cellId) const
{
	FileMadeBeforeLook();
	const auto index = _lists.IndexOf({cellId.x, cellId.y});
	return index.has_value() ? _lists.Mobile(*index) : std::span<const entt::entity> {};
}

std::span<const entt::entity> MapProduction::GetMobileInGridCell(const glm::vec3& pos) const
{
	return GetMobileInGridCell(GetGridCell(pos));
}

std::vector<entt::entity> MapProduction::GetAllInCell(glm::ivec2 cell) const
{
	FileMadeBeforeLook();
	const auto index = _lists.IndexOf(cell);
	return index.has_value() ? _lists.All(*index) : std::vector<entt::entity> {};
}

std::span<const entt::entity> MapProduction::GetOnBlock(glm::ivec2 block) const
{
	FileMadeBeforeLook();
	return _lists.Blocks().OnBlock(block);
}

std::span<const entt::entity> MapProduction::GetOffBlocks() const
{
	FileMadeBeforeLook();
	return _lists.Blocks().OffBlocks();
}

void MapProduction::Sync()
{
	if (_registry == nullptr)
	{
		return;
	}
	FileMadeBeforeLook();
	// What can move goes into its new cell: a building as soon as it is anywhere else, anything else once it is in
	// another cell
	std::vector<entt::entity> moved;
	_registry->Each<const MapCellMover, const MapCellResident>(
	    [this, &moved](entt::entity entity, const MapCellResident& /*resident*/) {
		    if (MovedSinceFiled(entity))
		    {
			    moved.push_back(entity);
		    }
	    });
	for (const auto entity : moved)
	{
		Refile(entity);
	}
}

void MapProduction::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _lists.SavedCells());
	reflection::Save(out, _lists.Blocks());
	reflection::Save(out, _made);
}

bool MapProduction::LoadState(reflection::BinaryReader& in)
{
	// The load filed the things in the cells in the order of the registry. The order that the game had comes back.
	std::vector<map_cells::CellLists::SavedCell> cells;
	map_cells::BlockLists blocks;
	std::vector<entt::entity> made;
	reflection::Load(in, cells);
	reflection::Load(in, blocks);
	reflection::Load(in, made);
	if (in.Failed() || !_lists.RestoreCells(cells))
	{
		return false;
	}
	_lists.Blocks() = std::move(blocks);
	_made = std::move(made);
	return true;
}

void MapProduction::FileResidents()
{
	if (_registry == nullptr)
	{
		return;
	}
	_lists.Clear();
	// The load made every entity again, and so put each one in the list of the things to file. But the saved world
	// keeps which things are in the cells, and its map state gives back the things that waited to be filed. A thing out
	// of the cells, such as a person in a creature's hand, must stay out.
	_made.clear();
	KnowLandBlocks();
	_registry->Each<const MapCellResident>([this](entt::entity entity, const MapCellResident& resident) {
		for (const auto cell : resident.cells)
		{
			_lists.Insert(cell, entity, resident.placement);
		}
	});
}

bool MapProduction::InMap(entt::entity entity) const
{
	FileMadeBeforeLook();
	if (_registry == nullptr || !_registry->Valid(entity))
	{
		return false;
	}
	return !KindOf(*_registry, entity).has_value() || _registry->AllOf<MapCellResident>(entity);
}

void MapProduction::Moved(entt::entity entity)
{
	if (_registry == nullptr)
	{
		return;
	}
	// The things made before it moved are in their cells first
	FileMadeBeforeLook();
	if (_registry->Valid(entity) && MovedSinceFiled(entity))
	{
		TakeOut(entity);
		if (const auto kind = KindOf(*_registry, entity))
		{
			File(entity, *kind);
		}
	}
}

void MapProduction::Placed(entt::entity entity)
{
	if (_registry == nullptr)
	{
		return;
	}
	// The things made before it moved are in their cells first
	FileMadeBeforeLook();
	if (!_registry->Valid(entity) || _registry->AnyOf<InHand, InPhysics>(entity))
	{
		return;
	}
	const auto* resident = _registry->TryGet<const MapCellResident>(entity);
	if (resident == nullptr || resident->cells.empty() || !MovedSinceFiled(entity, true))
	{
		return;
	}
	TakeOut(entity);
	if (const auto kind = KindOf(*_registry, entity))
	{
		File(entity, *kind);
	}
}

void MapProduction::Reinsert(entt::entity entity)
{
	if (_registry == nullptr)
	{
		return;
	}
	// The things made before it changed are in their cells first
	FileMadeBeforeLook();
	if (!_registry->Valid(entity) || !_registry->AllOf<Transform>(entity))
	{
		return;
	}
	// What a hand holds or the physics moves is not in the cells
	const auto* resident = _registry->TryGet<const MapCellResident>(entity);
	if (resident == nullptr || resident->cells.empty() || _registry->AnyOf<InHand, InPhysics>(entity))
	{
		return;
	}
	if (const auto kind = KindOf(*_registry, entity))
	{
		TakeOut(entity);
		File(entity, *kind);
	}
}

void MapProduction::FileNew()
{
	FileMadeBeforeLook();
}

void MapProduction::Refile(entt::entity entity)
{
	if (_registry == nullptr)
	{
		return;
	}
	// The things made before it moved are in their cells first
	FileMadeBeforeLook();
	if (!_registry->Valid(entity) || !_registry->AllOf<Transform>(entity))
	{
		return;
	}
	const auto kind = KindOf(*_registry, entity);
	if (!kind.has_value())
	{
		return;
	}
	TakeOut(entity);
	File(entity, *kind);
}
