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

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

// The town's queries the idle villagers use: the emergency, the congregation point and the two clear-area searches,
// plus the gutils helpers they are made of. Positions are MapCoords x / z (6553.6 per metre, as the original keeps
// them), converted from openblack's metres by truncation.

namespace openblack::ecs::components
{
struct Town;
}

namespace openblack::audio::guidance
{
struct HelpTown;
}

namespace openblack::ecs::town_queries
{
/// metres -> MapCoords (map_coords::ToFixed, truncated towards 0). (approximate) openblack keeps the positions in
/// float metres
[[nodiscard]] glm::ivec2 ToMapCoords(glm::vec2 metres);
[[nodiscard]] glm::vec2 ToMetres(glm::ivec2 mapCoords);
/// The x / z of an object's Transform as MapCoords; (0, 0) without one
[[nodiscard]] glm::ivec2 PosOf(entt::entity object);

/// The distance in metres: the hypotenuse of dx, dz x 10 / 65536, through Common/GUtilsDistance
/// (gutils::GetDistanceInMetres)
[[nodiscard]] float GetDistanceInMetres(glm::ivec2 a, glm::ivec2 b);
/// The arc tangent of b - a in 2048ths of a turn, from a towards b (gutils::GetAngleFromXZ)
[[nodiscard]] uint16_t GetAngleFromXZ(glm::ivec2 a, glm::ivec2 b);
/// The game angle of b - a in radians: (angle & 0x7FF) x 0.0030679617 (2 pi / 2048)
[[nodiscard]] float Get3DAngleFromXZ(glm::ivec2 a, glm::ivec2 b);
/// (cos(angle) x d x 65536 / 10, sin(angle) x d x 65536 / 10), truncated toward zero
/// (gutils::GetPosFromAngle, the x and z)
[[nodiscard]] glm::ivec2 GetPosFromAngle(float angle, float metres);

/// max(0.2 r truncated toward zero, 1)^2 map cells
[[nodiscard]] uint32_t GetMapCellSpiralSizeFromRadius(float radius);
/// (1 - (-2 a / b truncated toward zero))^2 points
[[nodiscard]] uint32_t GetIncrementSpiralSizeFromRadius(float a, float b);
/// --count == 0 -> ++dir, count = dir / 2; then pos = (pos x 10 / 65536 + step x table[dir & 3]) x 65536 / 10,
/// truncated toward zero, on x and on z (the spiral's table: (1, 0) (0, 1) (-1, 0) (0, -1))
void SpiralIncrement(glm::ivec2& pos, int32_t& dir, int32_t& count, float step);

/// Town::storagePit if it is available (abode_queries::IsAvailable), else entt::null
[[nodiscard]] entt::entity GetStoragePit(entt::entity town);
/// Town::creche (set when the creche becomes functional) if it is a valid entity, else entt::null
[[nodiscard]] entt::entity GetCreche(entt::entity town);
/// The town entity's Tribe component (TownArchetype), Tribe::NONE without one
[[nodiscard]] Tribe TribeOf(entt::entity town);
/// The town's "something changed" pulse: buildPulse = 1, buildPulsePrevious = 0, written by its writers (the storage
/// pit's and workshops' resources, a new building site, the fields, a death, a workshop made functional or losing its
/// scaffold, a scaffold's building); TownProcess reads and clears it. Nothing for anything that is not a town
void Pulse(entt::entity town);
/// The global town list: a new town goes to the head, so the newest town first: by Town::creationStamp from high to low
/// (the order TownArchetype::Create ran in = the script order). Empty without a registry
[[nodiscard]] std::vector<entt::entity> TownsNewestFirst();
/// (openblack) the next Town::ownerListStamp: a town joins the tail of its owner's list
[[nodiscard]] uint32_t NextOwnerListStamp();
/// (openblack) the "town_owner_list" part of the turn state hash (Debug/StateHash.h): every town's ownerListStamp,
/// newest town first. Registered on every land load (Game::LoadMap)
void RegisterStateHash();
/// What the help sprites' remarks read of a town: adults + children (Town::stats), GetStoragePit and its IsFunctional,
/// the position (the town's Transform)
[[nodiscard]] audio::guidance::HelpTown HelpTownOf(entt::entity town);

/// The emergency's start != 0 && turn - start < gameTurnsAfterEmergencyVillagersReact (unsigned)
[[nodiscard]] bool IsInStateOfEmergency(const components::Town& town);

/// The filters of the clear-area searches
using ClearAreaFilter = std::function<bool(entt::entity)>;
/// Fixed objects 1 (houses, features, fields, stores, town centre...), mobiles 0 (villagers, animals, piles, pots),
/// mobile statics 0 (fallen tree, rock, bonfire...), trees 0
[[nodiscard]] bool BlocksTownClearArea(entt::entity object);
/// 1 for every object (also villagers and trees). (approximate) every entity of openblack's map cells is taken as an
/// object
[[nodiscard]] bool IsObject(entt::entity object);

/// In the max(0.2 r truncated toward zero, 1)^2 map cells of a spiral from pos's cell (those inside the map), no object with
/// GetDistanceInMetres(object, pos) - Get2DRadius < r that is not `excluded` and passes `filter`. Its two lists:
/// the cell's fixed list, then its mobile one (TownCellObjectsInterface::ObjectsInCell, the map cells)
/// `blocker` (openblack, for the trace): the object that made it not clear
[[nodiscard]] bool CheckForClearArea(glm::ivec2 pos, float radius, const ClearAreaFilter& filter, entt::entity excluded,
                                     entt::entity* blocker = nullptr);
/// The object's 2D radius (Locator::townCellObjects)
[[nodiscard]] float Get2DRadius(entt::entity object);
/// The objects of one map cell as the clear area check walks them, the fixed list then the mobile one
/// (Locator::townCellObjects)
[[nodiscard]] std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell);
/// GetIncrementSpiralSizeFromRadius(a, b) points from `start` (SpiralIncrement, steps of b metres, dir = count = 1), the
/// first one with CheckForClearArea(p, r); none when no point is clear
[[nodiscard]] std::optional<glm::ivec2> FindClearArea(glm::ivec2 start, float a, float b, float radius,
                                                      const ClearAreaFilter& filter, entt::entity excluded);

/// The town's congregation point (cached in the town): the average of the town's abodes that are not fields
/// (and of its planned ones when there are fewer than 3) cleared with FindClearArea(130, 3, 10, BlocksTownClearArea),
/// else a point 10..20 m from a base (the single abode, or the town). `town` is the town entity
[[nodiscard]] glm::ivec2 GetCongregationPos(entt::entity town);

/// The first town whose ScriptIdOf is `id`, the players in order then the neutral one, each one's towns:
/// map_cells::ForEachTown's order; entt::null if none. For a number the scripts give (CREATE_*, the map scripts' magic)
[[nodiscard]] entt::entity FindTownWithID(uint32_t id);
/// Town::scriptId, else Town::id
[[nodiscard]] uint32_t ScriptIdOf(entt::entity town);
/// (openblack) the town of openblack's unique key Town::id (Abode::townId, Field::town, the forests): the id map. The
/// original holds a pointer to the town there
[[nodiscard]] entt::entity TownByKey(uint32_t key);
/// The town of any object, each kind's own:
/// - Villager;
/// - Field;
/// - FishFarm;
/// - Abode (the storage pits, town centres, workshops, creches, wonders, dispensers and graveyards answer the same);
/// - Scaffold;
/// - TotemStatue (its town centre's);
/// - Pot (the structure it is part of, when available: its own).
/// Everything else answers entt::null: animals, trees, mobile objects, the creature, worship sites, citadel, mobile
/// statics, features, rocks, big forests. (pending) town artifacts have a town too: none in openblack
[[nodiscard]] entt::entity GetTown(entt::entity object);
} // namespace openblack::ecs::town_queries
