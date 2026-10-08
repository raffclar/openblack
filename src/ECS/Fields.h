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

#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

// The field: its farmers, its food and growth, and what the villager side calls.

namespace openblack
{
struct GFieldTypeInfo;
}

namespace openblack::ecs
{

/// Once per game turn (the global lists' first loop): every 10th turn (plus the field's turn offset) a fully sown
/// field that is not on fire grows until it is ripe, by 2 (0.5 alignment + 1) x the sun / rain multiplier of its
/// GFieldTypeInfo, and its food with it
void ProcessFieldsTurn(uint32_t turn);
/// One field's turn: the abode's process first (the building site's Process and the empty-abode counters), then the
/// growth above. Called by ProcessFieldsTurn (every turn) and by the town's abode pass (every processAbodeEvery
/// turns: a town's field is a structure of it), so on those turns it runs twice, as in the original
void ProcessField(entt::entity field, uint32_t turn);

/// fields::RemoveFood: the wrapper the hand and the fire call (they drop the result)
int32_t RemoveFieldFood(entt::entity field, float amount);

/// True when ripe: growth >= ageRecolt
[[nodiscard]] bool IsFieldRipe(entt::entity field);

/// The water spell's own part on a field (the water miracle's drop, Magic/Spells/SpellWater.cpp, calls it after the
/// object part, only when the field is not on fire): crops <= timesToSow -> crops = timesToSow + 1 truncated toward zero = 31,
/// sown at once; else, while growth <= ageRecolt, growth += effectOfWaterSpell (info.dat 2.0) and food += effectOfWaterSpell x
/// totalFoodInField / ageRecolt. Returns false when it is not a field.
bool ApplyWaterSpellToField(entt::entity field);

/// Shown only with growth >= 0.25 x ageGrowth and food >= 25; sinks with its food over 1 s and fades out below 20 %
/// (PileSink / Alpha)
void UpdateFields(float seconds);

namespace components
{
struct Field;
}

/// The object colour the field's land light is multiplied by: growing, olive (full food) to light green; ripening,
/// olive to white; ripe, white
[[nodiscard]] glm::u8vec3 FieldDrawColour(const components::Field& field);

/// The wind lean of the 16 sway slots (as the trees': T0 = -0.03 cos(phase), the wind angle being always 0, so the
/// lean is along world z only); ripe fields shear their up axis by 1.75 x scale x this, trees by 1 x
[[nodiscard]] float WindSway(uint32_t slot);

} // namespace openblack::ecs

namespace openblack::ecs::fields
{
/// A field's or fish farm's worker on deletion: SetTopState(163 DECIDE_WHAT_TO_DO) (villager::SetTopState); its
/// exit function ExitFarming / ExitFishing unlinks it (RemoveFarmer / RemoveFisherman). Nothing for a villager no
/// longer valid
void ReleaseWorker(entt::entity villager);
/// (openblack) Game::LoadMap calls it right before Registry::Reset: entt's clear publishes on_destroy pool by pool
/// ((not verified) whether the original's map clear sends any worker to 163); without it the deletion listeners
/// (on_destroy<Field> / on_destroy<FishFarm>, connected again by the next AddFarmer / AddFisherman) would run
/// villager::SetTopState and its random draws on a half-cleared registry after the new map's RNG reset
void DisconnectDeletionListeners();

/// The field's GFieldTypeInfo: InfoConstants::fieldType[type]
[[nodiscard]] const GFieldTypeInfo& InfoOf(const components::Field& field);

/// The growth of one turn's step: a = 2 (alignment x 0.5 + 1); x the rain multipliers (growing / ripening) when it
/// rains, else the sun ones; growing while growth < ageGrowth, read before the step
[[nodiscard]] float GrowthStep(float growth, const GFieldTypeInfo& info, float alignment, bool raining);
/// ApplyWaterSpellToField's rule on the field itself; effectOfWaterSpell is read only when the field grows
void ApplyWaterSpell(components::Field& field, float (*effectOfWaterSpell)());

/// (float)crops / timesToSow
[[nodiscard]] float GetPercentFull(entt::entity field);
/// GetPercentFull < 1 -> 1 (to sow); growth >= ageGrowth -> 2 (to harvest); else 0
[[nodiscard]] int GetFieldActivity(entt::entity field);
/// A fire effect or not functional -> 0; a = 1 - min(1, farmers / maxFarmerInFarm); activity 2: growth >=
/// ageRecolt ? a : 0; activity 1: (1 - min(GetPercentFull, 1)) a^3; else 0
[[nodiscard]] float GetDesireToBeFarmed(entt::entity field);
/// (float)crops < timesToSow -> crops++ and true; else false
bool PlantCrop(entt::entity field);
/// Still sowing: (float)crops < timesToSow
[[nodiscard]] bool IsStillSowing(entt::entity field);
/// r1 = 5 - GameFloatRand(10) for x, r2 = 5 - GameFloatRand(10) for z; each axis (pos x 10 / 65536 + r) x
/// 65536 / 10, truncated toward zero; the altitude copied
[[nodiscard]] map_coords::MapCoords RandomFarmPoint(entt::entity field);
/// growth < ageRecolt -> false (no draw); else out = RandomFarmPoint (2 draws), true
bool RipeFarmPoint(entt::entity field, map_coords::MapCoords& out);
/// The field's position
[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity field);
/// fx - 5 <= px < fx + 5 and the same in z, in metres
[[nodiscard]] bool IsTouching(entt::entity field, const map_coords::MapCoords& pos);
/// 0 when it has no food or is not fully sown; values truncated toward zero: k = amount; cost = unripe ? amount x
/// ratioBeforeRipe + k : k; (u32)cost < food -> food -= cost, k. Otherwise it runs out: SetTemperature(0, null), the
/// town's pulse, and unripe: food = 0, amount x ratioBeforeRipe; ripe: food, and the field cleared (food, crops,
/// growth 0)
int32_t RemoveFood(entt::entity field, float amount);
/// growth < ageRecolt ? 0 : food
[[nodiscard]] float GetFoodValue(entt::entity field);
/// Already in the list or null -> nothing; else at the head. No maximum (maxFarmerInFarm is only in
/// GetDesireToBeFarmed); TargetThing is not written
void AddFarmer(entt::entity field, entt::entity villager);
/// Every node of the villager out; the villager's TargetThing = null ALWAYS, also when it was not in the list
/// (TODO: villager::SetTargetThing)
void RemoveFarmer(entt::entity field, entt::entity villager);
/// Is the villager in the list (AddFarmer's test)
[[nodiscard]] bool HasFarmer(entt::entity field, entt::entity villager);
[[nodiscard]] uint32_t FarmerCount(entt::entity field);
/// The field's town (kept as a Town::id), entt::null without one
[[nodiscard]] entt::entity TownOf(entt::entity field);
/// The town's fields, newest first (head insertion at creation): the Field entities with that town, by creation
/// index from high to low. (inferred) the same order: fields never change town
[[nodiscard]] std::vector<entt::entity> TownFields(entt::entity town);
/// A valid entity with a Field (ExitFarming's test)
[[nodiscard]] bool IsField(entt::entity thing);
/// The field's deletion (run by an on_destroy<Field> listener that AddFarmer connects, so ecs::ToBeDeleted /
/// Registry::Destroy reach it): every farmer, head first, the next taken before the call: ReleaseWorker
/// (SetTopState(163)) and TargetThing = null (TODO: villager::SetTargetThing). The town's and the global field lists and
/// SetTownArea need nothing in openblack (the components are the lists; town_placement recomputes the rectangle
/// when it is read); RemoveMapObject is ecs::ToBeDeleted's
void DeleteDependants(entt::entity field);
} // namespace openblack::ecs::fields
