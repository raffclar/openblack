/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
struct GObjectInfo;
} // namespace openblack

// What the fire asks of the burning object, and the info.dat values it reads (defenceMultiplierBurn, heatCapacity,
// combustionTemperature, burningPriority). One function per question; the classes that
// answer it their own way are handled inside. Wiki: docs/bw1-notes/magic.md, "Fire".

namespace openblack::ecs::fire::traits
{
/// The object's GObjectInfo: trees, dead trees, abodes, fields, features, villagers, animals, mobile
/// statics and objects, pots, magic fireballs. nullptr: no info, the object never burns.
[[nodiscard]] const GObjectInfo* InfoOf(entt::entity object);
/// An abode's GAbodeInfo, a field's too: a field hands its GAbodeInfo to its abode part, which keeps it as the
/// object's info (InfoOf gives a field's GFieldTypeInfo instead).
/// nullptr when the object is not an abode
[[nodiscard]] const GAbodeInfo* AbodeInfo(entt::entity object);

/// the info's combustion temperature; 0 without an info (the fire refuses to start)
[[nodiscard]] float CombustionTemperature(entt::entity object);
/// the info's heat capacity; a fireball's grows with its radius and strength
[[nodiscard]] float HeatCapacity(entt::entity object);
/// the info's defence multiplier against burns
[[nodiscard]] float DefenceMultiplierBurn(entt::entity object);
/// the info's burning priority (for the fire group)
[[nodiscard]] float BurningPriority(entt::entity object);

/// the object's position; a dead tree's mesh centre in x, z
/// (with its own height above the land). World x, z and the height above the land in y.
[[nodiscard]] glm::vec3 FireCentre(entt::entity object);
/// the 2D radius; a dead tree 0.35 x its height; a worship site
/// 14 (ecs::object::GetDefaultFireRadius)
[[nodiscard]] float DefaultFireRadius(entt::entity object);
/// the object's height (ecs::object::GetHeight)
[[nodiscard]] float Height(entt::entity object);
/// the object's 2D radius (ecs::object::GetRadius)
[[nodiscard]] float Radius(entt::entity object);
/// 0.01; a fireball 0 when the script cast it
[[nodiscard]] float RainCoolingMultiplier(entt::entity object);

/// still in the world
[[nodiscard]] bool IsAvailable(entt::entity object);
/// the object's "in the map" flag (not held, not flying; a fireball never is)
[[nodiscard]] bool IsObjectInMap(entt::entity object);
/// a fixed object spread over several map cells. The classes: Abode (Field, StoragePit...), BigForest, the citadel
/// parts (CitadelHeart, WorshipSite, the creature pen, WorshipTotem), Feature (AnimatedStatic), FishFarm,
/// MobileStatic (MagicTeleport; rocks: DeadTree, Fragment, the bonfire), the football pitch, the prayer site, SpellIcon,
/// TotemStatue. The
/// one-cell fixed objects (Tree, MapShield...) are not, nor are the footpaths and building sites.
[[nodiscard]] bool IsMultiCellStatic(entt::entity object);
[[nodiscard]] bool IsVillager(entt::entity object);
[[nodiscard]] bool IsCreature(entt::entity object);
/// in the hand
[[nodiscard]] bool InHand(entt::entity object);
/// whether a burn reaches it: true for most objects; a villager, a pot (something in it) and a field have their own
/// rules
[[nodiscard]] bool IsBurnReceiver(entt::entity object, float burn);

/// it never catches fire (SET_SET_ON_FIRE false)
[[nodiscard]] bool CannotBeSetOnFire(entt::entity object);
void SetCannotBeSetOnFire(entt::entity object, bool value);
/// burning does it no harm (SET_HURT_BY_FIRE false)
[[nodiscard]] bool NotHurtByFire(entt::entity object);
void SetNotHurtByFire(entt::entity object, bool value);

/// the life lost to burning: ReduceLife unless it is not hurt by fire (the town's aggressor is not
/// ported); a field loses damage x totalFoodInField of its food and returns 1. Returns the life after it.
float ReduceLifeDueToBurning(entt::entity object, float damage, std::optional<PlayerNames> player);
/// the default deletes the object (features too); a villager dies; a field
/// empties and deletes its fire;
/// the abode's own (building site, ghost) is not ported yet (the abode stays at life 0). `player` / `amount`: the
/// original's two arguments (the fire passes its player and 0); only the villager
/// uses them so far
void DestroyedByEffect(entt::entity object, std::optional<PlayerNames> player = std::nullopt, float amount = 0.0f);
/// the multi-cell fixed objects and dead trees drop their reactions; a pot its own
void StartOnFire(entt::entity object);
/// a dead tree creates REACT_TO_WOOD; a pot re-creates its reaction (not ported)
void EndOnFire(entt::entity object);

/// A land is loaded
void Clear();
} // namespace openblack::ecs::fire::traits
