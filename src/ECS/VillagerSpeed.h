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

#include <entt/entity/fwd.hpp>

namespace openblack
{
struct GVillagerInfo;
}

namespace openblack::ecs::components
{
struct WallHug;
}

namespace openblack::ecs
{

/// A villager's walking speed and size like the original (docs/bw1-notes/animation.md).

/// The info.dat entry of the villager (its tribe and villager number)
const GVillagerInfo* VillagerInfoOf(entt::entity villager);

/// On every top state change, from the speed group entry of the final state (info.dat villagerStateTable speed index),
/// the wounded and town terms, then the per-villager factor (creation index), age, life and sex. Sets WallHug::speed
/// (metres per turn). Nothing changes for a villager controlled by a script or dancing ((approximate) as TOP ==
/// IN_DANCE).
void SetVillagerStateSpeed(entt::entity villager);

/// The speed `whole` (map units per turn) times the per-villager factor (creation index, age, food, life and sex) when
/// applyFactor, else times 1, truncated, then clamped to 0..0xFFFF into WallHug::speed
void SetVillagerSpeed(entt::entity villager, int32_t whole, bool applyFactor);
/// The script's SET_PROPERTY Speed: SetVillagerSpeed(ConvertMetersToWholeDistance(metres), false): no factor
void SetVillagerSpeedInMetres(entt::entity villager, float metres);
/// ConvertWholeDistanceToMeters of the stored speed: WallHug::speed as it is
[[nodiscard]] float VillagerSpeedInMetres(entt::entity villager);
/// The u16 speed (map units a turn) the original keeps: WallHug::speed is ToMetres of it, and
/// ConvertMetersToWholeDistance gives it back exactly (whole x 10 / 65536 and its inverse are exact below 2^16). The one
/// copy: the step setup and the walk paths read it
[[nodiscard]] uint16_t WholeSpeed(const components::WallHug& wallHug);

/// The scale part of setting a villager's age: the initial scale, then the scale for the age, with the game's synced
/// GameFloatRand (villager::InitialScaleForAge / ScaleForAge). Adults end in (0.95, 1.05]; children take
/// ageToScale[age - 1] plus a random part of the way to ageToScale[age + 1].
float VillagerScaleForAge(const GVillagerInfo& info, uint32_t age);

} // namespace openblack::ecs
