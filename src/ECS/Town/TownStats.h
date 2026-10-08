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

#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include "ECS/Components/Town.h"
#include "Enums.h"

// The town's TownStats: the original keeps them incrementally (added and removed when a villager or an abode joins or
// leaves the town); openblack recomputes them from the entities at the start of the town's turn (ecs::town_process).
// (approximate) the same counts; the float sums are added in another order.

namespace openblack
{
struct GAbodeInfo;
enum class MeshId : uint32_t;
} // namespace openblack

namespace openblack::ecs::town_stats
{
/// Counts each villager of the town (Villager::town) and each of its abodes (Abode::townId; the fields too, they are
/// Abodes) that MakeFunctional has counted (Abode::addedToTownStats); the civic plans and the wood at the town's
/// building sites
[[nodiscard]] components::TownStats Compute(entt::entity town);
/// The abodes of the town: Abode::townId == Town::id, newest first (the creation index from high to low, as
/// town_queries' congregation point: an abode joining the town goes to the head)
[[nodiscard]] std::vector<entt::entity> AbodesOf(entt::entity town);
/// The abode's GAbodeInfo: the record of its abode number and mesh, else its tribe's (as influence::AbodeInfoOf;
/// (inferred) openblack keeps no info pointer); nullptr when none
[[nodiscard]] const GAbodeInfo* AbodeInfoOf(entt::entity abode, Tribe tribe);
/// The resource id of an info record's mesh, as resources::HashIdentifier gives it (the hashed text of the mesh's
/// number), without making the text a string: the abode info lookups compare it with each record they read
[[nodiscard]] entt::id_type MeshIdHash(MeshId mesh);
/// (TRIBE_TYPE, ABODE_NUMBER): the FIRST record whose tribe is the given one or Tribe::NONE and whose number matches;
/// null when none. (GAbodeInfo::Find of InfoConstants.cpp keeps the last match and throws without one: this one is the
/// original's)
[[nodiscard]] const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number);
/// The civic ABODE_TYPEs: Totem 0x14, StoragePit 0x24, Creche 0x44, Workshop 0x84, Wonder 0x100, Graveyard 0x204,
/// TownCentre 0x404, FootballPitch 0x1004, SpellDispenser 0x2004; not Field 0x4004 nor Citadel 0x804
[[nodiscard]] bool IsCivic(AbodeType type);
} // namespace openblack::ecs::town_stats
