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

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

namespace openblack::ecs
{
namespace components
{
struct FishShoal;
}

/// For each of the 15 fish: size 0.8..1.2, within +-5 (x, z) and -1..0 (y) of the centre, a random heading, speed
/// 0.5..1.5 and turn rate speed x about pi / 5 x (1 +- 0.1); the target is the centre
void InitFishShoal(components::FishShoal& shoal, const glm::vec3& centre);

/// Something hit the water here this frame (the hand gripping the sea, an object falling in).
/// Every shoal within 300 units of the camera reacts in the next UpdateFishShoals.
void SplashWater(const glm::vec3& point);

/// Once per game turn: every 16th turn each farm gets 1 food back, up to 1400
void ProcessFishFarmsTurn(uint32_t turn);

/// The fish farm with a shown fish within 2 units (x, z) of the point
[[nodiscard]] std::optional<entt::entity> FindFishFarmAt(const glm::vec3& point);

/// Coastal (a land cell on the coast line) and no fish farm already in that map cell. Nothing else (town, depth,
/// distance). Only scripts create fish farms today; this is the rule for placing one by hand.
[[nodiscard]] bool IsOkToCreateFishFarmAt(const glm::vec3& point);

/// All of `amount` if the farm has it, else what is left; the stock drops accordingly
uint32_t RemoveFishFarmFood(entt::entity farm, uint32_t amount);

/// Moves the fish of every fish farm shoal by `seconds` of game time and works out each shoal's visibility and alpha
/// from the camera distance. For the shoals with a bait (the fish puzzle) it also runs the bait's rule: the fish
/// inside, the net, and when it is done a ring per fish.
void UpdateFishShoals(float seconds, const glm::vec3& camera);

} // namespace openblack::ecs
