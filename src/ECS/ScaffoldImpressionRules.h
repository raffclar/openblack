/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <bit>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

/// The rules of the belief that a scaffold gives when it starts a building near towns. The scaffold makes a reaction,
/// and the first people of the near towns are impressed by it at once. The value of the scaffold, the land's balance and
/// the want of the town of the building set how impressive the scaffold is. Free of the game's state, so that they can
/// be tested on their own.
namespace openblack::ecs::scaffold_impression
{

/// The change of a town's boredom with the scaffold reaction for each impression, before the share of the villager in
/// its town multiplies it. A scaffold does not use the value of the belief table.
inline constexpr float k_BoredomStep = std::bit_cast<float>(0xBF800000U); // -1

/// How impressive a scaffold is to one watcher: its value in scaffolds, times the impressiveness that it has as a thing
/// in the world, times the want of the town of its building. A scaffold that is on no site has a want of 1.
[[nodiscard]] float ImpressiveValue(uint32_t value, float objectValue, std::optional<float> siteWant);

/// The want of the town that multiplies how impressive a scaffold on a site is: the want for abodes when the building is
/// an abode, else the want for civic buildings
[[nodiscard]] TownDesireInfo SiteWant(bool abode);

/// How many people of a town a new building site impresses: the town's adults and children times the part of the town
/// to impress, truncated, and at least 1
[[nodiscard]] uint32_t PeopleToImpress(uint32_t people, float proportion);

/// A town, for the choice of the towns that a new building site impresses
struct TownEntry
{
	/// When the town was made, against the other towns: earlier made, smaller
	uint32_t made {0};
	PlayerNames owner {PlayerNames::NEUTRAL};
	/// When its player gained it, against the player's other towns: earlier gained, smaller
	uint32_t gained {0};
};

/// The towns that a new building site can impress, in their order, by their places in the list. The first is the town
/// that was made last. The others are the towns that its player gained after it, earlier gained first. The towns of
/// other players are not in the list (a vanilla bug, kept).
[[nodiscard]] std::vector<size_t> TownsToImpress(std::span<const TownEntry> towns);

/// The first people of a town, abode by abode, at most a count. Each abode gives its people in their order.
[[nodiscard]] std::vector<entt::entity> FirstPeople(std::span<const std::vector<entt::entity>> abodes, uint32_t count);

} // namespace openblack::ecs::scaffold_impression
