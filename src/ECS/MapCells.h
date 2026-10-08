/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::map_collide
{
struct Shape;
}

namespace openblack::ecs::systems
{
class MapCellsSystemInterface;
}

/// The object lists of the map cells: every one of the 512 x 512 cells has two ORDERED lists, the mobile one and the
/// fixed one, kept up to date on every insert, remove and move, and the queries built on them.
///
/// - Which list: the object's type (CountsAsFixed), not its class.
/// - Which end: the Fixed classes (SingleMapFixed, MultiMapFixed, FishFarm) enter at the HEAD of the fixed list; every
///   other class at the TAIL of the fixed list when its type counts as fixed (pots and piles, street lanterns), else at
///   the HEAD of the mobile list (a doubly linked one).
/// - Which cells: a MultiMapFixed every cell whose 7.1 m circle touches its shape, every other class the cell of its
///   position.
/// - A walk of a cell is the fixed list, then the mobile one, each from its head.
///
/// The lists are kept by hooks the owners call (InsertMapObject, RemoveMapObject, MoveMapObject) and, for what has no
/// hook yet, by Sync() once per turn (MapProduction::Rebuild calls it): the creations, deletions, hand, physics and moves
/// of the other owners. Every read also skips what the original has taken out of the map at once (invalid, in the hand,
/// flying, carried by a storm).
///
/// The old ecs::MapInterface (MapProduction's unordered sets) stays for the callers that are not migrated yet.
namespace openblack::ecs::map_cells
{

/// Whether an object type goes in the fixed list (45 types); a type past the table, including ANY (-1) and -2 read as
/// unsigned, does not
constexpr std::array<bool, 45> k_CountsAsFixed {true, false, false, false, false, false, true,  true, true,  true, false, true,
                                                true, false, true,  false, false, false, true,  true, false, true, true,  true,
                                                true, true,  true,  false, true,  true,  false, true, true,  true, true,  true,
                                                true, true,  true,  true,  true,  true,  false, true, true};

[[nodiscard]] constexpr bool CountsAsFixed(ObjectType type)
{
	const auto index = static_cast<uint32_t>(type);
	return index < k_CountsAsFixed.size() && k_CountsAsFixed[index];
}

/// How the object's class enters the cells (InsertMapObject, GetMapChild)
enum class InsertKind : uint8_t
{
	None,           ///< not in the map: spell seeds, fireballs, towns, forests...
	Object,         ///< its own cell (mobile objects too)
	SingleMapFixed, ///< its cell, at the head
	MultiMapFixed,  ///< every cell its shape touches, the head of each
	FishFarm,       ///< only its own cell, at the head
};

/// The class of an openblack entity (its components stand in for the vtable)
[[nodiscard]] InsertKind KindOf(entt::entity object);
/// A MultiMapFixed or a FishFarm (a FishFarm is a MultiMapFixed)
[[nodiscard]] bool IsMultiCellStaticClass(entt::entity object);
/// The OBJECT_TYPE of the object's info, read from info.dat; a fixed (inferred) type for
/// the classes openblack keeps no info row for. ObjectType::Invalid for a class that is not in the map
[[nodiscard]] ObjectType TypeOf(entt::entity object);

// ---- The hooks ------------------------------------------------------------------------------------------------------

/// Puts the object in its cells by its class. Nothing if it is already in the map (openblack: an idempotent hook) or it
/// is not a map class. (inferred) A creation hook assumes the flags that make the original skip the insertion on
/// creation are clear
void InsertMapObject(entt::entity object);
/// Takes the object out of every cell it was put in
void RemoveMapObject(entt::entity object);
/// An Object only re-enters when the cell changes (else the position changes and the lists don't), a MultiMapFixed
/// when the MapCoords changes at all; both by Remove, set position, Insert (at the head). The Transform takes the new
/// position
void MoveMapObject(entt::entity object, const glm::vec3& position);
/// Remove and Insert when in the map, so it goes to the head even in the same cell. For the code that turns or scales
/// an object
void OnAnglesOrScaleChanged(entt::entity object);
/// Whether the object is in the map
[[nodiscard]] bool IsObjectInMap(entt::entity object);

/// openblack: once per turn, the other owners' creations, deletions, hand, physics and moves (until they call the hooks
/// themselves), in creation index order (inferred). MapProduction::Rebuild calls it
void Sync();
/// Empties every cell: no objects, no links
void Clear();
/// With OPENBLACK_MAPCELLS_CHECK=1 Sync checks the lists after each run (every link in the lists of its cells, no
/// cycles, every listed entity linked) and logs "map_cells: N objects, M cells, E errors". The number of errors
[[nodiscard]] size_t CheckConsistency();
/// The number of objects in the map (the links)
[[nodiscard]] size_t ObjectCount();

// ---- The cells of an object ----------------------------------------------------------------------------------------

/// The cells a shape covers: the box (centre -/+ reach) x 0.1 truncated and clipped at 0, each cell (x outer, z inner)
/// marked when its 7.1 m circle at (10 i + 5, 10 j + 5) touches the shape, the box's middle cell when none does; in the
/// order a MultiMapFixed is inserted, cut at the first marked cell off the map
[[nodiscard]] std::vector<glm::ivec2> DescriptorCells(const map_collide::Shape& shape, float reach);
/// The cells InsertMapObject would put the object in now, in its order
[[nodiscard]] std::vector<glm::ivec2> CellsOf(entt::entity object);

// ---- One cell -------------------------------------------------------------------------------------------------------

/// The head of the mobile / fixed list (unfiltered); null off the map
[[nodiscard]] entt::entity FirstMobile(glm::ivec2 cell);
[[nodiscard]] entt::entity FirstFixed(glm::ivec2 cell);
/// The next in that cell's list (a MultiMapFixed looks up its link for the cell by the cell's x and z). Unfiltered
[[nodiscard]] entt::entity GetMapChild(entt::entity object, glm::ivec2 cell);
/// The readers' filter: what the original has out of the map (invalid, in the hand, flying, held out)
[[nodiscard]] bool IsReadable(entt::entity object);
/// While one lives, the readers share one snapshot of the hand (held, thrown) and of the flying physics bodies instead
/// of taking it on each call (held out / invalid are still read live). openblack's cost only: the original takes the
/// objects out of its lists at once. Take it only around reads whose callbacks do not pick up, throw or launch
/// objects. Nests. Without the game's map cells it does nothing (a search that reads no cell still runs; the first read
/// stops as before)
class ReadBatch
{
public:
	ReadBatch();
	~ReadBatch();
	ReadBatch(const ReadBatch&) = delete;
	ReadBatch& operator=(const ReadBatch&) = delete;

private:
	systems::MapCellsSystemInterface* _lists {nullptr};
};

/// The cell walk: the fixed list, then the mobile one, each from the head; the next is taken before fn runs. fn returns
/// false to stop. Off the map: nothing (the callers check InBounds first)
void ForEachInCell(glm::ivec2 cell, const std::function<bool(entt::entity)>& fn);
/// The same walk as a snapshot (an object fn deletes or moves cannot break it)
[[nodiscard]] std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell);
/// The mobile list only (FirstMobile + GetMapChild), from the head
[[nodiscard]] std::vector<entt::entity> MobileInCell(glm::ivec2 cell);
void ForEachMobile(glm::ivec2 cell, const std::function<bool(entt::entity)>& fn);
/// type ANY (-1): the fixed list, then the mobile one (after the last of the fixed list, when its type counts as fixed,
/// the mobile head). Any other type: only its own list (the fixed one when CountsAsFixed(type)), the first after
/// `after` of that type. Null off the map
[[nodiscard]] entt::entity FindType(glm::ivec2 cell, ObjectType type, entt::entity after = entt::null);
/// The fixed list from after's child (or the head), the first MultiMapFixed
[[nodiscard]] entt::entity FindFixedOnMap(glm::ivec2 cell, entt::entity after = entt::null);
/// Only the head of the fixed list, a MultiMapFixed
[[nodiscard]] bool IsFixed(glm::ivec2 cell);
/// The fixed list only (FirstFixed + GetMapChild) from the head, filtered like the other readers; the next is taken
/// before fn runs. The circle iterator, a tree's end of physics and CollideWithFixed walk it. fn returns false to
/// stop. Off the map: nothing
void ForEachFixed(glm::ivec2 cell, const std::function<bool(entt::entity)>& fn);
/// The collide data of an object in the map: the shape built on insert (a tree's 0.3 circle, the mesh's shape); null
/// for the Object class, a BigForest, a FishFarm, an object not in the map or (approximate) one without a mesh
[[nodiscard]] const map_collide::Shape* CollideDataOf(entt::entity object);
/// 0xFFFFFFFF off the map; else the landscape's 0x10 (off the game map) or 1 water / 2 land
/// (sea_cells::CollideLandscape), with 0x20 for a type 6 (FOREST_TREE) and 4 for a type 0x12 (FIELD) in the fixed list.
/// Never 8: the original's collide-with-fixed branch here tests a bit the cell collide never sets
[[nodiscard]] uint32_t Collide(const map_coords::MapCoords& coords);
/// 0xFFFFFFFF off the map; else Collide's bits, | 8 when a 0.5 m circle at the MapCoords' x, z touches the collide data
/// of an object of the fixed list (CollideDataOf)
[[nodiscard]] uint32_t CollideWithFixed(const map_coords::MapCoords& coords);
/// The MapCoords' cell is this one (the effects walk a multi-cell object only in its own cell)
[[nodiscard]] constexpr bool IsOwnCell(const map_coords::MapCoords& coords, glm::ivec2 cell)
{
	return map_coords::Cell(coords) == cell;
}

// ---- Searches over several cells -----------------------------------------------------------------------------------

/// The cells of the square +-r (metres to 16.16 fixed point, the signed high words), x outer, z inner, InBounds; ONE
/// list (the fixed one when CountsAsFixed(type), else the mobile one, so ANY is the mobile list only); the nearest by
/// distance in metres, from FLT_MAX: not cut at r
[[nodiscard]] entt::entity FindNearType(const map_coords::MapCoords& coords, ObjectType type, float radius);
/// The same square (x outer; z counted as (high & 0xFFFF) - low + 1), InBounds on each cell, the full walk; among those
/// pred accepts, the nearest (from the totem for a worship site), strictly, from FLT_MAX: not cut at r
[[nodiscard]] entt::entity FindNearForScript(const map_coords::MapCoords& coords, const std::function<bool(entt::entity)>& pred,
                                             float radius);
/// The point FindNearForScript measures to, and the scripts' filters (CALL_NEAR, IS_FIRE_NEAR) compare with r: the
/// totem for a worship site, else the object's MapCoords
[[nodiscard]] map_coords::MapCoords ScriptDistancePoint(entt::entity object);
/// The spiral of max(3, ceil(2r / 10))^2 cells, FindType(ANY) in each; pred, not excluded, d < r and (d < best or none
/// yet); stops when best x 1.5 + 10 < the distance to the cell
[[nodiscard]] entt::entity FindNearestInSpiral(const map_coords::MapCoords& coords,
                                               const std::function<bool(entt::entity)>& pred, float radius,
                                               entt::entity excluded = entt::null);
/// The square +-r of the cells in the player's influence (influence > 0), the full walk; score(obj) not below the best,
/// then score x the distance modifier (distance, r) above the best (from 0)
[[nodiscard]] entt::entity FindNearInfluenced(const map_coords::MapCoords& coords, PlayerNames player,
                                              const std::function<float(entt::entity)>& score, float radius);
/// The highest top of the objects of this cell (fixed, then mobile) other than self (and, with skipLiving, not living
/// nor moving) whose in-cell offset is within dx^2 + dz^2 < r_obj^2 + r_self^2 of self's; 0 if none or off the map
[[nodiscard]] float TallestOverlapping(const map_coords::MapCoords& coords, entt::entity self,
                                       const map_coords::MapCoords& selfCoords, float selfRadius, bool skipLiving);

// ---- Towns (no cells) -----------------------------------------------------------------------------------------------

/// The 8 player slots (the neutral one last) x each player's town list, filled at the tail: the oldest first.
/// openblack: the towns whose owner is the player, by Town::id (inferred: a town that changes hands would go to the end
/// of its new owner's list)
void ForEachTown(const std::function<bool(entt::entity)>& fn);
[[nodiscard]] std::vector<entt::entity> TownsOf(PlayerNames player);
/// The distance in metres < best, best = r
[[nodiscard]] entt::entity GetNearestTown(const map_coords::MapCoords& coords, float radius);
/// True when one of the town's abodes is a town centre or one of its planned buildings has an info of abode number
/// TOWN_CENTRE
[[nodiscard]] bool TownHasCentre(entt::entity town);
/// The same, only the towns with a centre: the town's centre is set, or else TownHasCentre
[[nodiscard]] entt::entity GetNearestTownWithCentre(const map_coords::MapCoords& coords, float radius);
struct TownInCells
{
	entt::entity town {entt::null};
	uint32_t distance {0};
	uint32_t code {0}; ///< 0 none, 1 within 4 cells of the town's rectangle, 2 not
};
/// The whole-cell distance max(|dx|, |dz|) + (min >> 1) (an arithmetic shift), best = 10 000 000, not `excluded` and
/// not of `tribe`. The code: 1 when the position's cell is within 4 cells of the found town's rectangle
/// (Town::areaMin / areaMax, town_placement::SetTownArea keeps it), else 2
[[nodiscard]] TownInCells GetNearestTownCells(const map_coords::MapCoords& coords, entt::entity excluded,
                                              std::optional<Tribe> tribe);
/// The citadel of each player, < best, best = r
[[nodiscard]] entt::entity GetNearestCitadel(const map_coords::MapCoords& coords, float radius);
/// "any abode type" for GetNearestTownToPos
constexpr int32_t k_AnyAbodeType = 0x7FFF;
/// The town walk, the distance in metres < best, best = r; the tribe when given; abode != k_AnyAbodeType skips the
/// towns that have that abode type (it accepts the towns WITHOUT it)
[[nodiscard]] entt::entity GetNearestTownToPos(const map_coords::MapCoords& coords, std::optional<Tribe> tribe,
                                               int32_t abodeType, float radius);
/// GET_NEAREST_TOWN_OF_PLAYER: only TownsOf(player), the distance in metres <= best, best = r: a tie goes to the later
/// town. Not GetNearestTown
[[nodiscard]] entt::entity FindPlayerTownAtPos(const map_coords::MapCoords& coords, float radius, PlayerNames player);
/// The global town list (town_queries::TownsNewestFirst), the first always taken, then the distance in metres < best:
/// an exact tie goes to the newer town. It has no id branch
[[nodiscard]] entt::entity FindNearestTownInList(const map_coords::MapCoords& coords);

namespace detail
{
/// The angle the collide shape is turned by: the object's Y angle, stored when it is placed, turned or scaled. The x and
/// z angles do not enter. From a Transform: the y of the YXZ decomposition (atan2(m8, -m6), from row 2, where
/// m6 = -(cb sa) and m8 = cb ca), so what the Y angle was set to, and what the physics stores of a landed body
[[nodiscard]] float YAngleOf(const glm::mat3& rotation);
} // namespace detail

} // namespace openblack::ecs::map_cells
