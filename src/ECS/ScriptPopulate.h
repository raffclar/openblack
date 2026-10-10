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

#include <glm/vec3.hpp>

/// How a challenge script fills a flock, town or other container with new things of a kind
namespace openblack::ecs::script_populate
{

/// The kinds of thing a script may fill a container with: the scripts' object types from the first to the forty-first
[[nodiscard]] bool IsValidType(int32_t type);

/// How many things: the script's quantity with its fraction dropped, taken as an unsigned count, so a negative quantity
/// asks for a great many; none when the quantity is beyond any whole number
[[nodiscard]] uint32_t CountOf(float quantity);

/// How far either way of the container's place the things are spread: a tenth of a metre for each and 4 m more
[[nodiscard]] float SpreadOf(uint32_t count);

/// Where the next thing goes: away from the container's place across the land's x and up or down from its height, each
/// by the game's random draw below twice the spread, less the spread (across first); in line with the container along
/// z. `floatRand(x)` is the game's random number below x.
[[nodiscard]] glm::vec3 PlaceOf(glm::vec3 centre, float spread, const std::function<float(float)>& floatRand);

} // namespace openblack::ecs::script_populate
