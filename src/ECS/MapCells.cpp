/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapCells.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <unordered_map>

#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/CarriedByParticleSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Footpaths.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCellsLists.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MapCellsSystemInterface.h"
#include "ECS/Systems/MapShapeProviderInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/Citadel.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace map_cells = openblack::ecs::map_cells;

static_assert(map_cells::CountsAsFixed(ObjectType::Abode) && !map_cells::CountsAsFixed(ObjectType::Villager) &&
              map_cells::CountsAsFixed(ObjectType::Pot) && map_cells::CountsAsFixed(ObjectType::MobileStatic) &&
              !map_cells::CountsAsFixed(ObjectType::MobileObject) && !map_cells::CountsAsFixed(ObjectType::Firefly) &&
              map_cells::CountsAsFixed(ObjectType::MapShield) && !map_cells::CountsAsFixed(ObjectType::Any) &&
              !map_cells::CountsAsFixed(ObjectType::Invalid));

namespace
{
/// The circle of a map cell that a shape must touch
constexpr float k_CellCircleRadius = 7.1f;
/// Metres to cells
constexpr float k_CellsPerMetre = 0.1f;
/// FindNearestInSpiral's stop, best x 1.5 + 10
constexpr float k_SpiralStopFactor = 1.5f;
constexpr float k_SpiralStopAdd = 10.0f;
/// GetNearestTownCells: the best starts at 10 000 000
constexpr uint32_t k_TownCellsStart = 10000000u;
/// A tree's collide data: a circle of this radius at its position
constexpr float k_TreeCollideRadius = 0.3f;
/// CollideWithFixed: the circle tested at the point
constexpr float k_FixedTestRadius = 0.5f;

using map_cells::lists::Cell;
using map_cells::lists::Link;
using map_cells::lists::MultiCellLink;
using map_cells::lists::ReadFilter;

bool CheckEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("OPENBLACK_MAPCELLS_CHECK");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return enabled;
}

Registry* RegistryOrNull()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

/// The game's map cells; stops with a message when there are none (before the game or after it has gone)
systems::MapCellsSystemInterface& Lists()
{
	if (!Locator::mapCellsSystem::has_value())
	{
		std::fputs("ecs::map_cells: no map cells in the locator (Locator::mapCellsSystem)\n", stderr);
		std::abort();
	}
	return Locator::mapCellsSystem::value();
}

/// A new registry (the tests make one per test) drops every link: an entity of another registry is not this one's
void CheckRegistry()
{
	Lists().UseRegistry(RegistryOrNull());
}

/// The cell, or null off the map (InBounds)
Cell* CellAt(glm::ivec2 cell)
{
	return Lists().CellAt(cell);
}

const Cell* CellIfAny(glm::ivec2 cell)
{
	return Lists().CellIfAny(cell);
}

Link* LinkOf(entt::entity object)
{
	return Lists().LinkOf(object);
}

bool IsOneCell(map_cells::InsertKind kind)
{
	return kind == map_cells::InsertKind::Object || kind == map_cells::InsertKind::SingleMapFixed ||
	       kind == map_cells::InsertKind::FishFarm;
}

bool IsFixedClass(map_cells::InsertKind kind)
{
	return kind == map_cells::InsertKind::SingleMapFixed || kind == map_cells::InsertKind::MultiMapFixed ||
	       kind == map_cells::InsertKind::FishFarm;
}

/// A MultiMapFixed's children are sorted by x, then z
bool ChildLess(const MultiCellLink& a, const MultiCellLink& b)
{
	return a.x != b.x ? a.x < b.x : a.z < b.z;
}

/// The next in the cell's list: the object's own next; a MultiMapFixed's by a binary search on x, z; a FishFarm's for
/// its one position
entt::entity ChildOf(entt::entity object, glm::ivec2 cell)
{
	const auto* link = LinkOf(object);
	if (link == nullptr)
	{
		return entt::null;
	}
	if (link->kind != map_cells::InsertKind::MultiMapFixed)
	{
		return link->next;
	}
	const MultiCellLink key {entt::null, static_cast<int16_t>(cell.x), static_cast<int16_t>(cell.y)};
	const auto found = std::lower_bound(link->children.begin(), link->children.end(), key, ChildLess);
	if (found == link->children.end() || found->x != key.x || found->z != key.z)
	{
		return entt::null;
	}
	return found->next;
}

/// Sets the next in the cell's list; a MultiMapFixed by a linear search (the children may not be sorted yet while
/// InsertMapObject runs)
void SetChildOf(entt::entity object, glm::ivec2 cell, entt::entity next)
{
	auto* link = LinkOf(object);
	if (link == nullptr)
	{
		return;
	}
	if (link->kind != map_cells::InsertKind::MultiMapFixed)
	{
		link->next = next;
		return;
	}
	for (auto& child : link->children)
	{
		if (child.x == static_cast<int16_t>(cell.x) && child.z == static_cast<int16_t>(cell.y))
		{
			child.next = next;
			return;
		}
	}
}

ObjectType LinkType(entt::entity object)
{
	const auto* link = LinkOf(object);
	return link != nullptr ? link->type : ObjectType::Invalid;
}

/// A Fixed class enters at the head of the fixed list: the old head becomes the child (with no old head the link is
/// not touched), then the object becomes the head
void InsertFixedHead(entt::entity object, glm::ivec2 cellXZ)
{
	auto* cell = CellAt(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	if (cell->fixed != entt::null)
	{
		SetChildOf(object, cellXZ, cell->fixed);
	}
	cell->fixed = object;
}

/// Any other class: in the fixed list at its TAIL (the head when empty, else the last's child); else at the HEAD of
/// the mobile list (the old head's previous is this, the old head is this one's child, this is the head)
void InsertObject(entt::entity object, glm::ivec2 cellXZ)
{
	auto* cell = CellAt(cellXZ);
	auto* link = LinkOf(object);
	if (cell == nullptr || link == nullptr)
	{
		return;
	}
	if (link->fixedList)
	{
		if (cell->fixed == entt::null)
		{
			cell->fixed = object;
			return;
		}
		auto last = cell->fixed;
		for (auto next = ChildOf(last, cellXZ); next != entt::null; next = ChildOf(last, cellXZ))
		{
			last = next;
		}
		SetChildOf(last, cellXZ, object);
		return;
	}
	if (cell->mobile != entt::null)
	{
		if (auto* head = LinkOf(cell->mobile))
		{
			head->prev = object;
		}
		SetChildOf(object, cellXZ, cell->mobile);
	}
	cell->mobile = object;
}

/// Out of the object's list. Fixed: the head takes the child, else the previous one found by walking does. Mobile: no
/// previous is the head (the head is the child, the child has no previous), else the previous's child is the child,
/// the child's previous is the previous, and this has no previous. Then this has no child.
/// (openblack guard) a Fixed class leaves the fixed list it entered at the head even if its type's bit says mobile
void RemoveFromCell(entt::entity object, glm::ivec2 cellXZ)
{
	auto* cell = CellAt(cellXZ);
	auto* link = LinkOf(object);
	if (cell == nullptr || link == nullptr)
	{
		return;
	}
	const auto child = ChildOf(object, cellXZ);
	if (link->fixedList || IsFixedClass(link->kind))
	{
		if (cell->fixed == object)
		{
			cell->fixed = child;
		}
		else
		{
			for (auto previous = cell->fixed; previous != entt::null;)
			{
				const auto next = ChildOf(previous, cellXZ);
				if (next == object)
				{
					SetChildOf(previous, cellXZ, child);
					break;
				}
				previous = next;
			}
		}
	}
	else
	{
		const auto previous = link->prev;
		if (previous == entt::null || LinkOf(previous) == nullptr)
		{
			if (cell->mobile == object)
			{
				cell->mobile = child;
			}
			if (auto* next = LinkOf(child))
			{
				next->prev = entt::null;
			}
		}
		else
		{
			SetChildOf(previous, cellXZ, child);
			if (auto* next = LinkOf(child))
			{
				next->prev = previous;
			}
			link->prev = entt::null;
		}
	}
	SetChildOf(object, cellXZ, entt::null);
}

/// The registry's own answers about an entity's components (each question looks its storages up)
class RegistryProbe
{
public:
	explicit RegistryProbe(const Registry& registry)
	    : _registry(registry)
	{
	}
	template <typename... Components>
	[[nodiscard]] bool AnyOf(entt::entity entity) const
	{
		return _registry.AnyOf<Components...>(entity);
	}
	[[nodiscard]] bool Valid(entt::entity entity) const { return _registry.Valid(entity); }

private:
	const Registry& _registry;
};

/// The components the classification and the read filter ask about
template <typename... Components>
struct ComponentList
{
	static constexpr size_t k_Count = sizeof...(Components);

	/// The position of a component in the list
	template <typename Component>
	static consteval size_t IndexOf()
	{
		static_assert((std::is_same_v<Component, Components> || ...), "not a probed component");
		constexpr std::array<bool, k_Count> k_Same {std::is_same_v<Component, Components>...};
		return static_cast<size_t>(std::ranges::find(k_Same, true) - k_Same.begin());
	}

	/// Each component's storage id, in the list's order
	static constexpr std::array<entt::id_type, k_Count> k_Ids {entt::type_hash<Components>::value()...};
};

using ProbedComponents =
    ComponentList<Transform, Unavailable, CarriedByParticleSystem, MagicFireBall, SpellSeed, FishFarm, Abode, Field, Feature,
                  AnimatedStatic, MobileStatic, DeadTree, BigForest, TotemStatue, WorshipSite, Temple, SpellIcon, MagicTeleport,
                  Fragment, Tree, MagicTree, MapShield, ScriptHighlight, Villager, Animal, Creature, StreetLantern, Pot,
                  OneOffSpellSeed, MobileObject, Shark>;

/// The same answers as RegistryProbe for a pass over many entities: every storage is looked up once, when the probe is
/// made. A storage missing then reads as empty, as the registry's does, so a probe must not outlive the creation of a
/// storage it asks about
class StorageProbe
{
public:
	explicit StorageProbe(const Registry& registry)
	    : _registry(registry)
	{
		registry.EachStorage([this](entt::id_type id, const entt::sparse_set& storage) {
			const auto found = std::ranges::find(ProbedComponents::k_Ids, id);
			if (found != ProbedComponents::k_Ids.end())
			{
				_storages.at(static_cast<size_t>(found - ProbedComponents::k_Ids.begin())) = &storage;
			}
		});
	}
	template <typename... Components>
	[[nodiscard]] bool AnyOf(entt::entity entity) const
	{
		return (Has<Components>(entity) || ...);
	}
	[[nodiscard]] bool Valid(entt::entity entity) const { return _registry.Valid(entity); }

private:
	template <typename Component>
	[[nodiscard]] bool Has(entt::entity entity) const
	{
		const auto* storage = std::get<ProbedComponents::IndexOf<Component>()>(_storages);
		return storage != nullptr && storage->contains(entity);
	}

	const Registry& _registry;
	std::array<const entt::sparse_set*, ProbedComponents::k_Count> _storages {};
};

/// ecs::IsAvailable through a probe: a valid entity that is not marked Unavailable
template <typename Probe>
bool AvailableWith(const Probe& probe, entt::entity object)
{
	return probe.Valid(object) && !probe.template AnyOf<Unavailable>(object);
}

/// ReadFilter::Out through a probe of the filter's registry
template <typename Probe>
bool OutWith(const ReadFilter& filter, const Probe& probe, entt::entity object)
{
	if (!AvailableWith(probe, object) || probe.template AnyOf<CarriedByParticleSystem>(object))
	{
		// (above) a particle system carries it (physics::particle_carried_objects): out of the map
		return true;
	}
	if (filter.held && *filter.held == object)
	{
		return true;
	}
	if (std::find(filter.thrown.begin(), filter.thrown.end(), object) != filter.thrown.end())
	{
		return true;
	}
	return std::find(filter.flying.begin(), filter.flying.end(), object) != filter.flying.end();
}

/// map_cells::KindOf through a probe of the game's registry
template <typename Probe>
map_cells::InsertKind KindWith(const Probe& probe, entt::entity object)
{
	using map_cells::InsertKind;
	if (!AvailableWith(probe, object) || !probe.template AnyOf<Transform>(object))
	{
		return InsertKind::None;
	}
	// spell seeds and fireballs never enter the map
	if (probe.template AnyOf<MagicFireBall, SpellSeed>(object))
	{
		return InsertKind::None;
	}
	// a FishFarm is a MultiMapFixed with its own insertion
	if (probe.template AnyOf<FishFarm>(object))
	{
		return InsertKind::FishFarm;
	}
	// MultiMapFixed: the abodes (fields, storage pits, town centres), features, animated statics, mobile statics (rocks,
	// bonfires), dead trees, big forests, totems, worship sites, the citadel heart (openblack's Temple), spell icons,
	// magic teleports (mobile statics) and fragments (rocks)
	if (probe.template AnyOf<Abode, Field, Feature, AnimatedStatic, MobileStatic, DeadTree, BigForest, TotemStatue, WorshipSite,
	                         Temple, SpellIcon, MagicTeleport, Fragment>(object))
	{
		return InsertKind::MultiMapFixed;
	}
	// SingleMapFixed: Tree, MagicTree, MapShield, ScriptHighlight
	if (probe.template AnyOf<Tree, MagicTree, MapShield, ScriptHighlight>(object))
	{
		return InsertKind::SingleMapFixed;
	}
	// Object: the living, the creature, the street lanterns, pots and piles, the one-off orbs, mobile objects, the whale
	// (openblack's shark)
	if (probe.template AnyOf<Villager, Animal, Creature, StreetLantern, Pot, OneOffSpellSeed, MobileObject, Shark>(object))
	{
		return InsertKind::Object;
	}
	return InsertKind::None;
}

} // namespace

map_cells::lists::ReadFilter::ReadFilter()
    : registry(RegistryOrNull())
{
	if (Locator::handSystem::has_value())
	{
		const auto& hand = Locator::handSystem::value();
		held = hand.GetHeldObject();
		thrown = hand.GetThrownObjects();
	}
	// physics::PhysicsObjects::IsFlying of each, once: a body that is not a resting proxy
	physics::PhysicsObjects::ForEach([this](const physics::PhysicsObject& po) {
		if (!po.body.resting)
		{
			flying.push_back(po.entity);
		}
	});
}

bool map_cells::lists::ReadFilter::Out(entt::entity object) const
{
	return registry == nullptr || OutWith(*this, RegistryProbe(*registry), object);
}

namespace
{
/// The filter of one read: the batch's when one lives, else its own
class FilterRef
{
public:
	FilterRef()
	    : _batch(Lists().BatchFilter())
	{
		if (_batch == nullptr)
		{
			_own.emplace();
		}
	}
	[[nodiscard]] bool Out(entt::entity object) const { return _own ? _own->Out(object) : _batch->Out(object); }

private:
	/// The live batch's snapshot: the batch outlives every read made inside it
	const ReadFilter* _batch;
	std::optional<ReadFilter> _own;
};

/// The square of FindNearType / FindNearForScript / FindNearInfluenced: x * 10 * (1 / 65536) -/+ r, then * 65536 / 10
/// truncated, the signed high words
int32_t Corner(int32_t fixed, float offset)
{
	return map_coords::SignedCellOf(map_coords::ToFixedGUtils(map_coords::ToMetres(fixed) + offset));
}

/// The walk of FindNearForScript and FindNearInfluenced: x from the low corner to the high one (signed words); the z
/// count is (high & 0xFFFF) - low + 1 (low signed, high unsigned) and z goes up as a 32-bit counter whose low word is
/// the cell (the cell is stored as words)
template <typename Fn>
void ScriptSquare(const map_coords::MapCoords& coords, float radius, Fn&& fn)
{
	const int32_t lowX = Corner(coords.x, -radius);
	const int32_t lowZ = Corner(coords.z, -radius);
	const int32_t highX = Corner(coords.x, radius);
	const int32_t highZ = Corner(coords.z, radius);
	if (lowX > highX || lowZ > highZ)
	{
		return; // a low corner above the high one: no cells
	}
	const int32_t countZ = static_cast<int32_t>(static_cast<uint16_t>(highZ)) - lowZ + 1;
	for (int32_t x = lowX; x <= highX; ++x)
	{
		for (int32_t i = 0, z = lowZ; i < countZ; ++i, ++z)
		{
			// the words (x, z) as a cell, then InBounds
			const glm::ivec2 cell(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			if (map_coords::InBounds(cell))
			{
				fn(cell);
			}
		}
	}
}

/// The totem's position for a worship site, else the object's MapCoords. (inferred) the totem is openblack's
/// WorshipSite::totem entity
map_coords::MapCoords DistancePointOf(entt::entity object)
{
	if (const auto* site = Locator::entitiesRegistry::value().TryGet<const WorshipSite>(object))
	{
		if (site->totem != entt::null && Locator::entitiesRegistry::value().Valid(site->totem))
		{
			return object::MapCoordsOf(site->totem);
		}
	}
	return object::MapCoordsOf(object);
}

/// The x, z MapCoords of a world point (the altitude is not needed for the cells)
map_coords::MapCoords XZ(const glm::vec3& position)
{
	return map_coords::FromMetres(glm::vec2(position.x, position.z));
}

void Store(Link& link, const Transform& transform, entt::entity object)
{
	const auto coords = XZ(transform.position);
	link.x = coords.x;
	link.z = coords.z;
	link.rotation = transform.rotation;
	link.scale = transform.scale;
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(object);
	link.mesh = mesh != nullptr ? mesh->id : 0;
}

/// Sync's move test. Object / SingleMapFixed / FishFarm: re-enters only when the cell changes. (approximate) A FishFarm
/// re-enters like a MultiMapFixed, which does so (at the head) on any MapCoords change even in the same cell; a fish
/// farm does not move, so the cell test is kept for it. MultiMapFixed: on any MapCoords change and on a turn or a scale;
/// (approximate) the altitude is not compared (the ground openblack settles while drawing must not send the object to
/// the head every turn) and a new mesh counts as a change of shape
bool Moved(const Link& link, const Transform& transform, entt::entity object)
{
	const auto coords = XZ(transform.position);
	if (IsOneCell(link.kind))
	{
		return map_coords::Cell(coords) != link.cell;
	}
	const auto* mesh = Locator::entitiesRegistry::value().TryGet<const Mesh>(object);
	return coords.x != link.x || coords.z != link.z || transform.rotation != link.rotation || transform.scale != link.scale ||
	       (mesh != nullptr ? mesh->id : 0) != link.mesh;
}
} // namespace

// ---- Classification -------------------------------------------------------------------------------------------------

map_cells::InsertKind map_cells::KindOf(entt::entity object)
{
	const auto* registry = RegistryOrNull();
	if (registry == nullptr)
	{
		return InsertKind::None;
	}
	return KindWith(RegistryProbe(*registry), object);
}

bool map_cells::IsMultiCellStaticClass(entt::entity object)
{
	const auto kind = KindOf(object);
	return kind == InsertKind::MultiMapFixed || kind == InsertKind::FishFarm;
}

ObjectType map_cells::TypeOf(entt::entity object)
{
	if (KindOf(object) == InsertKind::None)
	{
		return ObjectType::Invalid;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const InfoConstants* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	// the info row's type; without info.dat (the unit tests) the type every row of that table has
	const auto row = [info](auto table, size_t index, ObjectType fallback) {
		if (info == nullptr || index >= (info->*table).size())
		{
			return fallback;
		}
		return (info->*table)[index].type;
	};
	if (const auto* c = registry.TryGet<const AnimatedStatic>(object))
	{
		return row(&InfoConstants::animatedStatic, static_cast<size_t>(c->type), ObjectType::Feature);
	}
	if (registry.AllOf<FishFarm>(object))
	{
		return info != nullptr ? info->fishFarm.type : ObjectType::FishFarm;
	}
	if (const auto* c = registry.TryGet<const BigForest>(object))
	{
		return row(&InfoConstants::bigForest, static_cast<size_t>(c->type), ObjectType::BigForest);
	}
	if (registry.AllOf<TotemStatue>(object))
	{
		// (inferred) the first totem statue row: openblack's statue keeps no row
		return row(&InfoConstants::totemStatue, 0, ObjectType::TotemStatue);
	}
	if (const auto* c = registry.TryGet<const WorshipSite>(object))
	{
		return row(&InfoConstants::worshipSite, c->infoIndex, ObjectType::Citadel);
	}
	if (registry.AllOf<Temple>(object))
	{
		return info != nullptr ? info->citadelHeart.type : ObjectType::Citadel;
	}
	if (const auto* c = registry.TryGet<const SpellIcon>(object))
	{
		return row(&InfoConstants::spellIcon, c->infoIndex, ObjectType::Citadel);
	}
	// (inferred) a mobile static row: magic teleports, fragments, dead trees (rocks) and street lanterns are made with
	// one; openblack keeps no row for them, so the first row's type
	if (registry.AnyOf<MagicTeleport, Fragment, DeadTree, StreetLantern>(object))
	{
		return row(&InfoConstants::mobileStatic, 0, ObjectType::MobileStatic);
	}
	if (registry.AllOf<OneOffSpellSeed>(object))
	{
		// the orb is made as the mobile object of row 25; its type in info.dat is 20 MOBILE_OBJECT (test_map_cells), so
		// the orb is at the head of the mobile list
		return row(&InfoConstants::mobileObject, static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed),
		           ObjectType::MobileObject);
	}
	if (registry.AllOf<Shark>(object))
	{
		// the whale's mobile object row (MobileObjectInfo::Whale)
		return row(&InfoConstants::mobileObject, static_cast<size_t>(MobileObjectInfo::Whale), ObjectType::MobileObject);
	}
	if (const auto* c = registry.TryGet<const ScriptHighlight>(object))
	{
		// its script highlight row; 35 SCRIPT_HIGHLIGHT for the four of info.dat
		return row(&InfoConstants::scriptHighlight, c->infoIndex, ObjectType::ScriptHighlight);
	}
	if (const auto* c = registry.TryGet<const Creature>(object))
	{
		// its species' row (inferred)
		return row(&InfoConstants::creature, creature::InfoRow(c->species), ObjectType::Creature);
	}
	if (info != nullptr)
	{
		// the rows openblack keeps: physics::PhysicsObjects::ObjectInfo (mobile statics, pots, mobile objects, orbs,
		// trees, dead trees, shields, animals, villagers), fields (fieldType), abodes, features
		if (const auto* objectInfo = fire::traits::InfoOf(object))
		{
			return objectInfo->type;
		}
	}
	// no row: the type of the class's table (inferred)
	if (registry.AllOf<Field>(object))
	{
		return ObjectType::Field;
	}
	if (registry.AllOf<Abode>(object))
	{
		return ObjectType::Abode;
	}
	if (registry.AllOf<Feature>(object))
	{
		return ObjectType::Feature;
	}
	if (registry.AllOf<MobileStatic>(object))
	{
		return ObjectType::MobileStatic;
	}
	if (registry.AnyOf<Tree, MagicTree>(object))
	{
		return ObjectType::ForestTree;
	}
	if (registry.AllOf<MapShield>(object))
	{
		return ObjectType::MapShield;
	}
	if (registry.AllOf<Pot>(object))
	{
		return ObjectType::Pot;
	}
	if (registry.AllOf<Villager>(object))
	{
		return ObjectType::Villager;
	}
	if (registry.AllOf<Animal>(object))
	{
		return ObjectType::Animal;
	}
	return ObjectType::MobileObject;
}

// ---- The cells of an object ----------------------------------------------------------------------------------------

std::vector<glm::ivec2> map_cells::DescriptorCells(const map_collide::Shape& shape, float reach)
{
	// (centre - reach) x 0.1 truncated (+ reach for the high corner). (approximate) the original keeps reach and each
	// product at extended precision; here floats
	int32_t x0 = map_coords::FtoL((shape.centre.x - reach) * k_CellsPerMetre);
	int32_t x1 = map_coords::FtoL((shape.centre.x + reach) * k_CellsPerMetre);
	int32_t z0 = map_coords::FtoL((shape.centre.y - reach) * k_CellsPerMetre);
	int32_t z1 = map_coords::FtoL((shape.centre.y + reach) * k_CellsPerMetre);
	// a negative low corner is 0, and only then a negative high one is 0 too
	if (x0 < 0)
	{
		x0 = 0;
		if (x1 < 0)
		{
			x1 = 0;
		}
	}
	if (z0 < 0)
	{
		z0 = 0;
		if (z1 < 0)
		{
			z1 = 0;
		}
	}
	const int32_t width = x1 - x0 + 1;
	const int32_t depth = z1 - z0 + 1;
	if (width <= 0 || depth <= 0)
	{
		return {};
	}
	std::vector<uint8_t> marked(static_cast<size_t>(width) * static_cast<size_t>(depth), 0);
	int32_t count = 0;
	// x outer, z inner, the mask index always moves on; a cell is tested only when it is on the map (unsigned) against a
	// 7.1 m circle at (10 x + 5, 10 z + 5)
	size_t index = 0;
	for (int32_t x = x0; x <= x1; ++x)
	{
		for (int32_t z = z0; z <= z1; ++z, ++index)
		{
			if (static_cast<uint32_t>(x) >= map_coords::k_MapCells || static_cast<uint32_t>(z) >= map_coords::k_MapCells)
			{
				continue;
			}
			const glm::vec2 point(static_cast<float>(x * 10 + 5), static_cast<float>(z * 10 + 5));
			if (map_collide::Collide(point, k_CellCircleRadius, shape))
			{
				marked[index] = 0xFF;
				++count;
			}
		}
	}
	// none hit -> the middle one, ((w / 2) x d) + d / 2 (rounded towards 0), not checked against the map
	if (count == 0)
	{
		marked[static_cast<size_t>((width / 2) * depth + depth / 2)] = 0xFF;
	}
	// the marked cells in that order; a marked cell off the map ends the list, and the insertion stops there
	std::vector<glm::ivec2> cells;
	index = 0;
	for (int32_t x = x0; x <= x1; ++x)
	{
		for (int32_t z = z0; z <= z1; ++z, ++index)
		{
			if (marked[index] == 0)
			{
				continue;
			}
			if (static_cast<uint32_t>(x) >= map_coords::k_MapCells || static_cast<uint32_t>(z) >= map_coords::k_MapCells)
			{
				return cells;
			}
			cells.emplace_back(x, z);
		}
	}
	return cells;
}

namespace
{
/// The shape of the object's mesh and the reach of its cells (Locator::mapShapeProvider); false without a mesh
bool MeshShape(entt::entity object, map_collide::Shape& shape, float& reach)
{
	return Locator::mapShapeProvider::value().MeshShape(object, shape, reach);
}

/// The cells InsertMapObject puts the object in and, when asked, the collide data its insert builds:
/// - Object: none (villagers, animals, pots and piles, street lanterns, mobile objects).
/// - SingleMapFixed: Tree / MagicTree a 0.3 circle at the position; MapShield and the rest the mesh's shape.
/// - MultiMapFixed: the mesh's shape; a BigForest has none. (approximate) WorshipSite and the citadel heart build their
///   own shapes (not ported): the mesh's.
/// - FishFarm: none.
/// (approximate) a fixed object without a mesh in openblack has no shape (and only the cell of its position)
std::vector<glm::ivec2> CellsAndCollide(entt::entity object, map_cells::InsertKind kind,
                                        std::optional<map_collide::Shape>* collide)
{
	using map_cells::InsertKind;
	if (kind == InsertKind::None)
	{
		return {};
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(object);
	// Object, SingleMapFixed and FishFarm: the cell of the one position, nothing off the map
	const auto own = map_coords::CellOf(transform.position);
	const auto ownCell = [&own]() -> std::vector<glm::ivec2> {
		if (!map_coords::InBounds(own))
		{
			return {};
		}
		return {own};
	};
	if (kind == InsertKind::SingleMapFixed)
	{
		if (collide != nullptr)
		{
			if (registry.AnyOf<Tree, MagicTree>(object))
			{
				*collide = map_collide::Shape {
				    glm::vec2(transform.position.x, transform.position.z), k_TreeCollideRadius, {}, 0.0f, "tree"};
			}
			else
			{
				map_collide::Shape shape;
				float reach = 0.0f;
				if (MeshShape(object, shape, reach))
				{
					*collide = std::move(shape);
				}
			}
		}
		return ownCell();
	}
	if (kind != InsertKind::MultiMapFixed)
	{
		return ownCell();
	}
	// the cells come from the same mesh shape as the collide data
	map_collide::Shape shape;
	float reach = 0.0f;
	if (!MeshShape(object, shape, reach))
	{
		return ownCell(); // (approximate) no mesh: the cell of its position
	}
	auto cells = map_cells::DescriptorCells(shape, reach);
	if (collide != nullptr && !registry.AllOf<BigForest>(object))
	{
		*collide = std::move(shape);
	}
	return cells;
}
} // namespace

std::vector<glm::ivec2> map_cells::CellsOf(entt::entity object)
{
	return CellsAndCollide(object, KindOf(object), nullptr);
}

// ---- The hooks ------------------------------------------------------------------------------------------------------

namespace
{
/// A Fixed / MultiMapFixed insert or remove, after the Object part on every path (also off the map): when the creature
/// must avoid it, the footpaths go round it (or back), with its 2D radius and its MapCoords. The position is the
/// Transform's x and z as they are; the altitude is not read by either. (A FishFarm does not do this.) (pending,
/// creature) the insert then walks the creatures: each one that must avoid it, in the right state, adds it to its
/// route plan
void TellFootpaths(entt::entity object, map_cells::InsertKind kind, bool inserted)
{
	if (kind != map_cells::InsertKind::SingleMapFixed && kind != map_cells::InsertKind::MultiMapFixed)
	{
		return;
	}
	auto* registry = RegistryOrNull();
	if (registry == nullptr || !route_plan_world::CreatureMustAvoid(object))
	{
		return;
	}
	const auto* transform = registry->TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	const map_coords::MapCoords pos {map_coords::ToFixed(transform->position.x), map_coords::ToFixed(transform->position.z),
	                                 0.0f};
	const float r = object::Get2DRadius(object);
	if (inserted)
	{
		footpaths::RerouteFootpathsAroundObstacle(r, pos);
	}
	else
	{
		footpaths::StopReroutingAroundObstacle(r, pos);
	}
}
} // namespace

void map_cells::InsertMapObject(entt::entity object)
{
	CheckRegistry();
	auto* registry = RegistryOrNull();
	if (registry == nullptr)
	{
		return;
	}
	const auto kind = KindOf(object);
	if (LinkOf(object) != nullptr)
	{
		TellFootpaths(object, kind, true); // (openblack, guard) already in: no second link; the wrapper's call stays
		return;
	}
	if (kind == InsertKind::None)
	{
		return;
	}
	Link link;
	const auto cells = CellsAndCollide(object, kind, &link.collide);
	if (cells.empty() && kind != InsertKind::MultiMapFixed)
	{
		TellFootpaths(object, kind, true); // off the map: not in the map, but the footpaths still hear of it
		return;
	}
	link.kind = kind;
	link.type = TypeOf(object);
	link.fixedList = CountsAsFixed(link.type);
	Store(link, registry->Get<const Transform>(object), object);
	if (kind == InsertKind::MultiMapFixed)
	{
		// one child per cell, then the head of each cell's fixed list
		link.children.reserve(cells.size());
		auto& stored = Lists().AddLink(object, std::move(link));
		for (const auto cell : cells)
		{
			stored.children.push_back({entt::null, static_cast<int16_t>(cell.x), static_cast<int16_t>(cell.y)});
			InsertFixedHead(object, cell);
		}
		// the children sorted by x, then z
		auto* sorted = LinkOf(object);
		std::sort(sorted->children.begin(), sorted->children.end(), ChildLess);
		TellFootpaths(object, kind, true);
		return;
	}
	link.cell = cells.front();
	Lists().AddLink(object, std::move(link));
	if (kind == InsertKind::Object)
	{
		InsertObject(object, cells.front());
	}
	else
	{
		InsertFixedHead(object, cells.front()); // SingleMapFixed and FishFarm: at the head
	}
	TellFootpaths(object, kind, true);
}

void map_cells::RemoveMapObject(entt::entity object)
{
	auto* link = LinkOf(object);
	if (link == nullptr)
	{
		TellFootpaths(object, KindOf(object), false); // not in the map: no cells to leave, the footpaths still hear of it
		return;
	}
	// (approximate) the cells it was put in: the original works the cells out again from the current state, which is the
	// same because it removes before it moves or turns the object
	if (link->kind == InsertKind::MultiMapFixed)
	{
		const auto children = link->children;
		for (const auto& child : children)
		{
			RemoveFromCell(object, glm::ivec2(child.x, child.z));
		}
	}
	else
	{
		RemoveFromCell(object, link->cell);
	}
	const auto kind = link->kind;
	Lists().RemoveLink(object); // no longer in the map
	// The MultiMapFixed one runs after the removal and before the collide data is released; openblack's collide data
	// goes with the link, which the planner never reads (it finds its obstacles through the cells, and the object is out
	// of them in both)
	TellFootpaths(object, kind, false);
}

void map_cells::MoveMapObject(entt::entity object, const glm::vec3& position)
{
	auto* registry = RegistryOrNull();
	if (registry == nullptr || !registry->Valid(object))
	{
		return;
	}
	auto* transform = registry->TryGet<Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	const auto* link = LinkOf(object);
	bool reinsert = false;
	if (link != nullptr)
	{
		if (IsOneCell(link->kind))
		{
			// the new cell against the old one. (approximate) a FishFarm re-enters like a MultiMapFixed (any MapCoords
			// change); it does not move
			reinsert = map_coords::CellOf(position) != link->cell;
		}
		else
		{
			// any MapCoords change (x, z and the altitude)
			reinsert = !(map_coords::FromWorld(position) == map_coords::FromWorld(transform->position));
		}
	}
	if (!reinsert)
	{
		transform->position = position;
		return;
	}
	// Remove, set the position, Insert
	RemoveMapObject(object);
	transform->position = position;
	InsertMapObject(object);
}

void map_cells::OnAnglesOrScaleChanged(entt::entity object)
{
	if (LinkOf(object) == nullptr)
	{
		return;
	}
	RemoveMapObject(object);
	InsertMapObject(object);
}

bool map_cells::IsObjectInMap(entt::entity object)
{
	return LinkOf(object) != nullptr;
}

void map_cells::Clear()
{
	Lists().Clear();
}

size_t map_cells::ObjectCount()
{
	return Lists().Links().size();
}

void map_cells::Sync()
{
	CheckRegistry();
	auto* registry = RegistryOrNull();
	if (registry == nullptr)
	{
		return;
	}
	const ReadFilter filter;
	// 1. out of the map: deleted, now of another class (a felled tree), in the hand or in
	// physics, or a one-cell object off the map. Taking an object out of a list does not change the others' order
	// Each pass asks the same questions of every entity, so its storages are looked up once (StorageProbe); the second
	// pass takes its own, after the removals
	std::vector<entt::entity> out;
	{
		const StorageProbe probe(*registry);
		for (const auto& [entity, link] : Lists().Links())
		{
			if (!registry->Valid(entity) || OutWith(filter, probe, entity) || KindWith(probe, entity) != link.kind)
			{
				out.push_back(entity);
			}
		}
	}
	for (const auto entity : out)
	{
		RemoveMapObject(entity);
	}
	// 2. the moves and the new ones, by creation index (inferred: the turn's processing order; the original inserts each
	// object as it is made)
	struct Action
	{
		int64_t index;
		entt::entity entity;
		bool move;
	};
	std::vector<Action> actions;
	{
		const StorageProbe probe(*registry);
		registry->Each<const Transform>([&](entt::entity entity, const Transform& transform) {
			const auto kind = KindWith(probe, entity);
			if (kind == InsertKind::None || OutWith(filter, probe, entity))
			{
				return;
			}
			const auto* link = LinkOf(entity);
			if (link == nullptr)
			{
				actions.push_back({object_index::Of(entity), entity, false});
			}
			else if (Moved(*link, transform, entity))
			{
				actions.push_back({object_index::Of(entity), entity, true});
			}
		});
	}
	std::sort(actions.begin(), actions.end(), [](const Action& a, const Action& b) {
		const auto ia = a.index < 0 ? std::numeric_limits<int64_t>::max() : a.index;
		const auto ib = b.index < 0 ? std::numeric_limits<int64_t>::max() : b.index;
		return ia != ib ? ia < ib : a.entity < b.entity;
	});
	for (const auto& action : actions)
	{
		if (action.move)
		{
			RemoveMapObject(action.entity); // a move or a turn goes to the head
		}
		InsertMapObject(action.entity);
	}
	if (CheckEnabled())
	{
		const auto errors = CheckConsistency();
		const auto used = static_cast<size_t>(std::ranges::count_if(
		    Lists().Cells(), [](const auto& cell) { return cell.mobile != entt::null || cell.fixed != entt::null; }));
		if (auto logger = spdlog::get("game")) // (openblack) the tests have no "game" logger
		{
			SPDLOG_LOGGER_INFO(logger, "map_cells: {} objects, {} cells, {} errors", Lists().Links().size(), used, errors);
		}
	}
}

size_t map_cells::CheckConsistency()
{
	size_t errors = 0;
	std::unordered_map<entt::entity, size_t> seen;
	auto& lists = Lists();
	const auto cells = lists.Cells();
	const size_t limit = lists.Links().size() + 1;
	for (size_t i = 0; i < cells.size(); ++i)
	{
		const glm::ivec2 cellXZ(static_cast<int32_t>(i / map_coords::k_MapCells),
		                        static_cast<int32_t>(i % map_coords::k_MapCells));
		const auto& cell = cells[i];
		for (int list = 0; list < 2; ++list)
		{
			size_t steps = 0;
			entt::entity previous = entt::null;
			for (auto e = list == 0 ? cell.fixed : cell.mobile; e != entt::null; e = ChildOf(e, cellXZ))
			{
				const auto* link = LinkOf(e);
				if (link == nullptr || ++steps > limit)
				{
					++errors; // an unlinked entity in a list, or a cycle
					break;
				}
				const bool inFixed = link->fixedList || IsFixedClass(link->kind);
				if (inFixed != (list == 0) || (list == 1 && link->prev != previous))
				{
					++errors;
				}
				++seen[e];
				previous = e;
			}
		}
	}
	for (const auto& [entity, link] : lists.Links())
	{
		const size_t expected = link.kind == InsertKind::MultiMapFixed ? link.children.size() : 1;
		const auto found = seen.find(entity);
		if ((found == seen.end() ? 0 : found->second) != expected)
		{
			++errors;
		}
	}
	return errors;
}

// ---- One cell -------------------------------------------------------------------------------------------------------

entt::entity map_cells::FirstMobile(glm::ivec2 cell)
{
	const auto* c = CellIfAny(cell);
	return c != nullptr ? c->mobile : entt::null;
}

entt::entity map_cells::FirstFixed(glm::ivec2 cell)
{
	const auto* c = CellIfAny(cell);
	return c != nullptr ? c->fixed : entt::null;
}

entt::entity map_cells::GetMapChild(entt::entity object, glm::ivec2 cell)
{
	return ChildOf(object, cell);
}

bool map_cells::IsReadable(entt::entity object)
{
	return !FilterRef().Out(object);
}

map_cells::ReadBatch::ReadBatch()
{
	if (Locator::mapCellsSystem::has_value())
	{
		_lists = &Locator::mapCellsSystem::value();
		_lists->BeginReadBatch();
	}
}

map_cells::ReadBatch::~ReadBatch()
{
	if (_lists != nullptr)
	{
		_lists->EndReadBatch();
	}
}

void map_cells::ForEachInCell(glm::ivec2 cellXZ, const std::function<bool(entt::entity)>& fn)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	const FilterRef filter;
	// the fixed head, or the mobile head when the fixed list is empty
	auto object = cell->fixed;
	bool fixed = true;
	if (object == entt::null)
	{
		object = cell->mobile;
		fixed = false;
	}
	while (object != entt::null)
	{
		// the next first, moving on to the mobile list at the end of the fixed list
		auto next = ChildOf(object, cellXZ);
		if (next == entt::null && fixed)
		{
			next = cell->mobile;
			fixed = false;
		}
		if (!filter.Out(object) && !fn(object))
		{
			return;
		}
		object = next;
	}
}

std::vector<entt::entity> map_cells::ObjectsInCell(glm::ivec2 cell)
{
	std::vector<entt::entity> objects;
	ForEachInCell(cell, [&objects](entt::entity object) {
		objects.push_back(object);
		return true;
	});
	return objects;
}

void map_cells::ForEachMobile(glm::ivec2 cellXZ, const std::function<bool(entt::entity)>& fn)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	const FilterRef filter;
	for (auto object = cell->mobile; object != entt::null;)
	{
		const auto next = ChildOf(object, cellXZ);
		if (!filter.Out(object) && !fn(object))
		{
			return;
		}
		object = next;
	}
}

std::vector<entt::entity> map_cells::MobileInCell(glm::ivec2 cell)
{
	std::vector<entt::entity> objects;
	ForEachMobile(cell, [&objects](entt::entity object) {
		objects.push_back(object);
		return true;
	});
	return objects;
}

entt::entity map_cells::FindType(glm::ivec2 cellXZ, ObjectType type, entt::entity after)
{
	const auto* cell = CellIfAny(cellXZ); // null off the map
	if (cell == nullptr)
	{
		return entt::null;
	}
	const FilterRef filter;
	if (type == ObjectType::Any)
	{
		// after's child; at the end of the fixed list (after's type counts as fixed) the mobile head
		const auto next = [cell, cellXZ](entt::entity object) {
			const auto child = ChildOf(object, cellXZ);
			if (child == entt::null && CountsAsFixed(LinkType(object)))
			{
				return cell->mobile;
			}
			return child;
		};
		auto object = after == entt::null ? (cell->fixed != entt::null ? cell->fixed : cell->mobile) : next(after);
		while (object != entt::null && filter.Out(object))
		{
			object = next(object);
		}
		return object;
	}
	// only the list of the type, from after's child or the head; the first whose info has that type
	auto object = after != entt::null ? ChildOf(after, cellXZ) : (CountsAsFixed(type) ? cell->fixed : cell->mobile);
	while (object != entt::null && (LinkType(object) != type || filter.Out(object)))
	{
		object = ChildOf(object, cellXZ);
	}
	return object;
}

entt::entity map_cells::FindFixedOnMap(glm::ivec2 cellXZ, entt::entity after)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return entt::null;
	}
	const FilterRef filter;
	auto object = after != entt::null ? ChildOf(after, cellXZ) : cell->fixed;
	while (object != entt::null)
	{
		const auto* link = LinkOf(object);
		// a MultiMapFixed (a FishFarm is one)
		if (link != nullptr && (link->kind == InsertKind::MultiMapFixed || link->kind == InsertKind::FishFarm) &&
		    !filter.Out(object))
		{
			return object;
		}
		object = ChildOf(object, cellXZ);
	}
	return entt::null;
}

bool map_cells::IsFixed(glm::ivec2 cellXZ)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return false;
	}
	// a fixed head that is a MultiMapFixed. The head the original has (an object out of the map is not in it)
	const FilterRef filter;
	auto head = cell->fixed;
	while (head != entt::null && filter.Out(head))
	{
		head = ChildOf(head, cellXZ);
	}
	const auto* link = head != entt::null ? LinkOf(head) : nullptr;
	return link != nullptr && (link->kind == InsertKind::MultiMapFixed || link->kind == InsertKind::FishFarm);
}

void map_cells::ForEachFixed(glm::ivec2 cellXZ, const std::function<bool(entt::entity)>& fn)
{
	const auto* cell = CellIfAny(cellXZ);
	if (cell == nullptr)
	{
		return;
	}
	const FilterRef filter;
	for (auto object = cell->fixed; object != entt::null;)
	{
		const auto next = ChildOf(object, cellXZ);
		if (!filter.Out(object) && !fn(object))
		{
			return;
		}
		object = next;
	}
}

const map_collide::Shape* map_cells::CollideDataOf(entt::entity object)
{
	const auto* link = LinkOf(object);
	return link != nullptr && link->collide ? &*link->collide : nullptr;
}

namespace
{
/// The collide bits of a cell on the map
uint32_t CellCollide(glm::ivec2 cellXZ)
{
	// the landscape cell's water bit (1 when no block), 0x10 off the game map, else 1 water, 2 land
	// (ecs::sea_cells::CollideLandscape). No island (the unit tests): no block, so water
	uint32_t bits = Locator::terrainSystem::has_value() ? sea_cells::CollideLandscape(Locator::terrainSystem::value(), cellXZ)
	                                                    : static_cast<uint32_t>(sea_cells::k_CollideWater);
	if (bits == sea_cells::k_CollideEdge)
	{
		return bits; // no object bits
	}
	// the fixed list from the head, the type of each: 6 FOREST_TREE |= 0x20, 0x12 FIELD |= 4; the mobile list is not
	// read. openblack's filter stands in for the objects the original has already taken out of the list
	map_cells::ForEachFixed(cellXZ, [&bits](entt::entity object) {
		const auto type = LinkType(object);
		if (type == ObjectType::ForestTree)
		{
			bits |= sea_cells::k_CollideTree;
		}
		else if (type == ObjectType::Field)
		{
			bits |= sea_cells::k_CollideField;
		}
		return true;
	});
	return bits;
}
} // namespace

uint32_t map_cells::Collide(const map_coords::MapCoords& coords)
{
	// off the map: all bits set
	if (!map_coords::InBounds(coords))
	{
		return 0xFFFFFFFFu;
	}
	// the cell's bits, then the collide-with-fixed test only when they have bit 8. The cell only ever gives 0x10, or
	// 1 / 2 with 4 and 0x20: that branch is dead
	return CellCollide(map_coords::Cell(coords));
}

uint32_t map_cells::CollideWithFixed(const map_coords::MapCoords& coords)
{
	// off the map: all bits set
	if (!map_coords::InBounds(coords))
	{
		return 0xFFFFFFFFu;
	}
	// the cell's bits, then a 0.5 circle at (x, z) in metres (x 10 / 65536; the altitude is the point's y, the collision
	// is in x, z) against the collide data of every object of the fixed list (none skipped but those without data); the
	// first hit gives | 8
	const auto cellXZ = map_coords::Cell(coords);
	uint32_t bits = CellCollide(cellXZ);
	const glm::vec2 point = map_coords::ToMetres(coords);
	bool hit = false;
	ForEachFixed(cellXZ, [&](entt::entity object) {
		const auto* shape = CollideDataOf(object);
		hit = shape != nullptr && map_collide::Collide(point, k_FixedTestRadius, *shape);
		return !hit;
	});
	if (hit)
	{
		bits |= sea_cells::k_CollideFixed;
	}
	return bits;
}

// ---- Searches -------------------------------------------------------------------------------------------------------

entt::entity map_cells::FindNearType(const map_coords::MapCoords& coords, ObjectType type, float radius)
{
	// the corners, x outer and z inner as signed words, InBounds per cell
	const int32_t lowX = Corner(coords.x, -radius);
	const int32_t lowZ = Corner(coords.z, -radius);
	const int32_t highX = Corner(coords.x, radius);
	const int32_t highZ = Corner(coords.z, radius);
	const FilterRef filter;
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max();
	const bool fixedList = CountsAsFixed(type); // ANY is the mobile list
	for (int32_t x = lowX; x <= highX; ++x)
	{
		for (int32_t z = lowZ; z <= highZ; ++z)
		{
			const glm::ivec2 cellXZ(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			const auto* cell = CellIfAny(cellXZ);
			if (cell == nullptr)
			{
				continue;
			}
			for (auto object = fixedList ? cell->fixed : cell->mobile; object != entt::null; object = ChildOf(object, cellXZ))
			{
				if ((type != ObjectType::Any && LinkType(object) != type) || filter.Out(object))
				{
					continue;
				}
				const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(object));
				if (distance < bestDistance) // strictly nearer
				{
					bestDistance = distance;
					best = object;
				}
			}
		}
	}
	return best;
}

entt::entity map_cells::FindNearForScript(const map_coords::MapCoords& coords, const std::function<bool(entt::entity)>& pred,
                                          float radius)
{
	const ReadBatch batch; // one snapshot for every cell of the search (openblack's cost only)
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max();
	ScriptSquare(coords, radius, [&](glm::ivec2 cell) {
		ForEachInCell(cell, [&](entt::entity object) {
			if (!pred(object))
			{
				return true;
			}
			// from this to the totem of a worship site or the object's MapCoords
			const float distance = gutils::GetDistanceInMetres(coords, DistancePointOf(object));
			if (distance < bestDistance) // strictly nearer
			{
				bestDistance = distance;
				best = object;
			}
			return true;
		});
	});
	return best;
}

map_coords::MapCoords map_cells::ScriptDistancePoint(entt::entity object)
{
	return DistancePointOf(object);
}

entt::entity map_cells::FindNearestInSpiral(const map_coords::MapCoords& coords, const std::function<bool(entt::entity)>& pred,
                                            float radius, entt::entity excluded)
{
	const ReadBatch batch; // one snapshot for every cell of the search (openblack's cost only)
	// ceil(2r / 10) as a double, truncated, at least 3. (approximate) 2r / 10 in float here, at extended precision in
	// the original
	int32_t side = map_coords::FtoL(static_cast<float>(std::ceil(static_cast<double>((radius + radius) / 10.0f))));
	if (side <= 3)
	{
		side = 3;
	}
	int32_t cells = side * side;
	auto walk = coords;
	map_coords::Spiral spiral;
	entt::entity best = entt::null;
	float bestDistance = 0.0f;
	while (cells != 0)
	{
		// with a best, stop when best x 1.5 + 10 < the distance to this cell's walk point
		if (best != entt::null &&
		    bestDistance * k_SpiralStopFactor + k_SpiralStopAdd < gutils::GetDistanceInMetres(coords, walk))
		{
			break;
		}
		if (map_coords::InBounds(walk))
		{
			const auto cell = map_coords::Cell(walk);
			for (auto object = FindType(cell, ObjectType::Any); object != entt::null;
			     object = FindType(cell, ObjectType::Any, object))
			{
				if (!pred(object) || object == excluded)
				{
					continue;
				}
				const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(object));
				// d < r, then d < best or no best yet
				if (distance < radius && (distance < bestDistance || best == entt::null))
				{
					bestDistance = distance;
					best = object;
				}
			}
		}
		--cells;
		map_coords::AddCells(walk, spiral.Next());
	}
	return best;
}

entt::entity map_cells::FindNearInfluenced(const map_coords::MapCoords& coords, PlayerNames player,
                                           const std::function<float(entt::entity)>& score, float radius)
{
	const ReadBatch batch; // one snapshot for every cell of the search (openblack's cost only)
	entt::entity best = entt::null;
	float bestScore = 0.0f;
	ScriptSquare(coords, radius, [&](glm::ivec2 cell) {
		// the player's influence at the cell > 0. (inferred) the cell's point is taken as its middle
		const glm::vec3 middle(static_cast<float>(cell.x) * map_coords::k_CellSize + map_coords::k_CellSize * 0.5f, 0.0f,
		                       static_cast<float>(cell.y) * map_coords::k_CellSize + map_coords::k_CellSize * 0.5f);
		if (!(influence::CalculatePlayerInfluence(player, middle) > 0.0f))
		{
			return;
		}
		ForEachInCell(cell, [&](entt::entity object) {
			const float s = score(object);
			if (s < bestScore)
			{
				return true;
			}
			const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(object));
			const float value = gutils::GetDistanceModifier(distance, radius) * s;
			if (value > bestScore) // strictly above
			{
				bestScore = value;
				best = object;
			}
			return true;
		});
	});
	return best;
}

float map_cells::TallestOverlapping(const map_coords::MapCoords& coords, entt::entity self,
                                    const map_coords::MapCoords& selfCoords, float selfRadius, bool skipLiving)
{
	if (!map_coords::InBounds(coords))
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// the in-cell offset in metres: the low words of the MapCoords x 1/65536 x 10
	const auto offset = [](const map_coords::MapCoords& c) {
		return glm::vec2(static_cast<float>(static_cast<uint32_t>(c.x) & 0xFFFFu) * (1.0f / 65536.0f) * 10.0f,
		                 static_cast<float>(static_cast<uint32_t>(c.z) & 0xFFFFu) * (1.0f / 65536.0f) * 10.0f);
	};
	const glm::vec2 selfOffset = offset(selfCoords);
	float best = 0.0f;
	ForEachInCell(map_coords::Cell(coords), [&](entt::entity object) {
		if (object == self)
		{
			return true;
		}
		// living or moving. (approximate) openblack keeps no last position for a fixed object: one with a physics body is
		// the moving one
		if (skipLiving &&
		    (registry.AnyOf<Villager, Animal, Creature>(object) || physics::PhysicsObjects::Find(object) != nullptr))
		{
			return true;
		}
		// only a top above the best
		const float top = object::GetTopPos(object);
		if (!(top > best))
		{
			return true;
		}
		const glm::vec2 d = selfOffset - offset(object::MapCoordsOf(object));
		const float radius = object::Get2DRadius(object);
		// (dz dz + dx dx) strictly below (r_obj r_obj + r_self r_self)
		if (d.y * d.y + d.x * d.x < radius * radius + selfRadius * selfRadius)
		{
			best = top;
		}
		return true;
	});
	return best;
}

// ---- Towns ----------------------------------------------------------------------------------------------------------

std::vector<entt::entity> map_cells::TownsOf(PlayerNames player)
{
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	if (auto* registry = RegistryOrNull())
	{
		registry->Each<const Town>([&](entt::entity entity, const Town& town) {
			if (town.owner == player)
			{
				towns.emplace_back(town.ownerListStamp, entity);
			}
		});
	}
	// the player's town list from its head: a town is added at the tail (on creation and on a take-over), so the order
	// the towns joined this player (Town::ownerListStamp)
	std::sort(towns.begin(), towns.end());
	std::vector<entt::entity> result;
	result.reserve(towns.size());
	for (const auto& [id, entity] : towns)
	{
		result.push_back(entity);
	}
	return result;
}

void map_cells::ForEachTown(const std::function<bool(entt::entity)>& fn)
{
	// the slots 0..7 in order, the neutral one (7) last
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		for (const auto town : TownsOf(static_cast<PlayerNames>(p)))
		{
			if (!fn(town))
			{
				return;
			}
		}
	}
}

namespace
{
template <typename Accept>
entt::entity NearestTown(const map_coords::MapCoords& coords, float radius, Accept&& accept)
{
	entt::entity best = entt::null;
	float bestDistance = radius;
	map_cells::ForEachTown([&](entt::entity town) {
		if (!accept(town))
		{
			return true;
		}
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(town));
		if (distance < bestDistance) // strictly nearer
		{
			bestDistance = distance;
			best = town;
		}
		return true;
	});
	return best;
}
} // namespace

entt::entity map_cells::GetNearestTown(const map_coords::MapCoords& coords, float radius)
{
	return NearestTown(coords, radius, [](entt::entity) { return true; });
}

bool map_cells::TownHasCentre(entt::entity town)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& data = registry.Get<const Town>(town);
	// the town's abodes, one that is a town centre. A town centre is the abode of an info of type 0x404
	// (CREATE_TOWN_CENTRE / CREATE_ABODE -> AbodeArchetype), and in info.dat those are exactly the infos of number
	// ABODE_NUMBER_TOWN_CENTRE (12; test_map_cells TownCentreInfosAreNumber12), the only thing the Abode keeps
	bool found = false;
	registry.Each<const Abode>(
	    [&](const Abode& abode) { found = found || (abode.townId == data.id && abode.type == AbodeNumber::TownCentre); });
	if (found)
	{
		return true;
	}
	// the planned list, one whose info has abode number 12 (TOWN_CENTRE); the other planned things' infos, features,
	// give -1; openblack's plannedAbodes are CREATE_PLANNED_ABODE's only
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	return std::any_of(data.plannedAbodes.begin(), data.plannedAbodes.end(), [&infos](const PlannedAbode& planned) {
		const auto i = static_cast<size_t>(planned.info);
		return i < infos.size() && infos[i].abodeNumber == AbodeNumber::TownCentre;
	});
}

entt::entity map_cells::GetNearestTownWithCentre(const map_coords::MapCoords& coords, float radius)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return NearestTown(coords, radius, [&registry](entt::entity town) {
		// the town's centre is set, or else TownHasCentre
		return registry.Get<const Town>(town).centre != entt::null || TownHasCentre(town);
	});
}

map_cells::TownInCells map_cells::GetNearestTownCells(const map_coords::MapCoords& coords, entt::entity excluded,
                                                      std::optional<Tribe> tribe)
{
	const auto& registry = Locator::entitiesRegistry::value();
	TownInCells result;
	uint32_t best = k_TownCellsStart;
	ForEachTown([&](entt::entity town) {
		if (town == excluded)
		{
			return true;
		}
		if (const auto* t = registry.TryGet<const Tribe>(town); tribe && t != nullptr && *t == *tribe)
		{
			return true; // of the excluded tribe
		}
		const auto at = object::MapCoordsOf(town);
		// the high words' differences, |dx| and |dz|, then max + (min >> 1)
		const int32_t dx = std::abs(static_cast<int32_t>(map_coords::SignedCellOf(at.x)) - map_coords::SignedCellOf(coords.x));
		const int32_t dz = std::abs(static_cast<int32_t>(map_coords::SignedCellOf(at.z)) - map_coords::SignedCellOf(coords.z));
		const auto distance = static_cast<uint32_t>(std::max(dx, dz) + (std::min(dx, dz) >> 1));
		if (distance < best)
		{
			best = distance;
			result.town = town;
			result.distance = distance;
		}
		return true;
	});
	if (result.town != entt::null)
	{
		// the cells are the high words of the rectangle, compared unsigned (a cell below 4 wraps to "outside"); 4 cells
		// of margin. An empty rectangle (min 0x7FFF, max 0) is never "within"
		result.code = 2;
		if (const auto* t = registry.TryGet<const openblack::ecs::components::Town>(result.town); t != nullptr)
		{
			constexpr uint32_t k_Margin = 4;
			const uint32_t cx = map_coords::CellX(coords);
			const uint32_t cz = map_coords::CellZ(coords);
			const bool outside =
			    cx + k_Margin < map_coords::CellOf(t->areaMin.x) || cx - k_Margin > map_coords::CellOf(t->areaMax.x) ||
			    cz + k_Margin < map_coords::CellOf(t->areaMin.y) || cz - k_Margin > map_coords::CellOf(t->areaMax.y);
			result.code = outside ? 2 : 1;
		}
	}
	return result;
}

entt::entity map_cells::GetNearestCitadel(const map_coords::MapCoords& coords, float radius)
{
	entt::entity best = entt::null;
	float bestDistance = radius;
	for (uint8_t p = 0; p < static_cast<uint8_t>(PlayerNames::_COUNT); ++p)
	{
		const auto citadel = worship::citadel::Of(static_cast<PlayerNames>(p));
		if (citadel == entt::null)
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(citadel));
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = citadel;
		}
	}
	return best;
}

entt::entity map_cells::GetNearestTownToPos(const map_coords::MapCoords& coords, std::optional<Tribe> tribe, int32_t abodeType,
                                            float radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	return NearestTown(coords, radius, [&](entt::entity town) {
		if (tribe)
		{
			const auto* t = registry.TryGet<const Tribe>(town);
			if (t == nullptr || *t != *tribe)
			{
				return false;
			}
		}
		if (abodeType == k_AnyAbodeType)
		{
			return true;
		}
		// the town's abodes whose info has that abode type; a town that has one is skipped. The fields are in that list:
		// a field is an abode of its town, and its info is the abode info it was made with
		const auto id = registry.Get<const Town>(town).id;
		bool found = false;
		registry.Each<const Abode>([&](entt::entity abode, const Abode& data) {
			if (found || data.townId != id)
			{
				return;
			}
			const auto* info = fire::traits::AbodeInfo(abode);
			found = info != nullptr && static_cast<int32_t>(info->abodeType) == abodeType;
		});
		return !found;
	});
}

entt::entity map_cells::FindPlayerTownAtPos(const map_coords::MapCoords& coords, float radius, PlayerNames player)
{
	// only that player's list, best = r; the distance in metres to the town's MapCoords keeps it unless it is farther:
	// <=, so a tie goes to the later town (and a NaN distance is taken)
	entt::entity best = entt::null;
	float bestDistance = radius;
	for (const auto town : TownsOf(player))
	{
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(town));
		if (!(distance > bestDistance))
		{
			bestDistance = distance;
			best = town;
		}
	}
	return best;
}

entt::entity map_cells::FindNearestTownInList(const map_coords::MapCoords& coords)
{
	// the global list: the newest town first (a new town goes to the head)
	entt::entity best = entt::null;
	float bestDistance = 0.0f;
	for (const auto town : town_queries::TownsNewestFirst())
	{
		const float distance = gutils::GetDistanceInMetres(coords, object::MapCoordsOf(town));
		if (best == entt::null || distance < bestDistance) // the first always, then strictly nearer
		{
			bestDistance = distance;
			best = town;
		}
	}
	return best;
}

float map_cells::detail::YAngleOf(const glm::mat3& rotation)
{
	// the y of the YXZ decomposition, ArcTanOctant(m8, -m6), stored as a float; glm's column 2 is the matrix's row 2
	return static_cast<float>(affine::ArcTanOctant(rotation[2][2], -rotation[2][0]));
}
