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

#include "ECS/Components/Villager.h"
#include "Enums.h"

/// What a villager carries: picking food or wood up, putting it down, and the room left for more. Free of the game's
/// state so they can be tested on their own.
namespace openblack::ecs::villager_carry
{

/// Adds to what it carries, food or else wood, which shows as one of four logs. What was picked up, which the town
/// counts as carried.
int16_t PickUp(components::Villager& villager, ResourceType type, int16_t amount, uint8_t woodGraphic);

/// Puts down some of what it carries of food or else wood: all of it when asked for none or more than it has, the load
/// read unsigned. What was put down. Its wood keeps the log it showed.
uint16_t PutDown(components::Villager& villager, ResourceType type, uint16_t amount);

/// The room left for more of food or else wood, under the most it can carry
[[nodiscard]] int32_t Room(const components::Villager& villager, ResourceType type, int32_t maxFood, int32_t maxWood);

/// Which of its loads it carries most of: food when it has more food than wood, else wood when it has any, else none.
/// The loads are compared as the game compares them, unsigned.
[[nodiscard]] ResourceType HeldResource(const components::Villager& villager);

} // namespace openblack::ecs::villager_carry
