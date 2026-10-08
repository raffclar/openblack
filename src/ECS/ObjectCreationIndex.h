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

#include <string>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{
namespace components
{
/// The value of the creation counter when the object was made
struct ObjectCreationIndex
{
	uint32_t value;
};
} // namespace components

/// The original's object creation counter:
/// every Object-derived thing takes the next number when it is made (abodes, trees, features, rocks, villagers,
/// animals, pots, the creature, lanterns, and side objects openblack doesn't make: town desire flags, script highlights,
/// spell icons, citadel parts...). Towns, forests, mists, footpaths, streams, planned buildings and the hand don't
/// count. It only goes up; clearing the map sets it to 0 when a land is loaded, and on a fresh boot the first land
/// starts at 2 (the help system made two help spirits before it). Villager::SetSpeed reads it.
namespace object_index
{
/// A land is loaded: back to 0, or to 2 on the first land of the session
void OnLoadMap();
/// Gives the entity the next index
void Assign(entt::entity entity);
/// Takes numbers for objects openblack doesn't create
void Skip(uint32_t count);
/// The entity's index, or none (-1)
[[nodiscard]] int64_t Of(entt::entity entity);

/// CREATE_NEW_TOWN_SPELL / the town centre's spell icons (TownCentreSpellIcons, at most 6): a town's distinct spell
/// seeds, and whether its centre exists; each new seed of a town with a centre makes an icon
void AddTownSpell(uint32_t town, const std::string& spell);
void OnTownCentre(uint32_t town);
} // namespace object_index

} // namespace openblack::ecs
