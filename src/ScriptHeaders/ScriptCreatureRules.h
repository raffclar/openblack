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
#include <span>

#include <glm/vec3.hpp>

#include "Enums.h"

/// How the scripts' creature and leash functions read and set a creature, where the answer is a rule of its own
namespace openblack::script::creature_rules
{

/// A leash on a player's temple, as the scripts see it: its type, and whether the player has picked it
struct LeashPostState
{
	LeashType type {LeashType::Rope};
	bool selected {false};
};

/// The leash type the scripts get for a creature: the leash its player has picked on the temple, not the leash the
/// creature wears. -1 when the player has picked none, and 0 when the player has no leashes on a temple.
[[nodiscard]] int32_t LeashTypeForScripts(std::span<const LeashPostState> posts);

/// Which of the two things a script asks about is the creature with the leash: the first when it is a creature, else
/// the second when it is one, else neither
enum class LeashedPair : uint8_t
{
	FirstIsCreature,
	SecondIsCreature,
	Neither,
};
[[nodiscard]] LeashedPair WhichIsLeashed(bool firstIsCreature, bool secondIsCreature);

/// The area a script keeps a creature in. The centre is kept as precisely as a map position, at height 0; the height
/// the script gives is not used.
struct Confinement
{
	glm::vec3 centre {0.0f};
	float radius {0.0f};
};
[[nodiscard]] Confinement ConfinementFor(const glm::vec3& point, float radius);

} // namespace openblack::script::creature_rules
