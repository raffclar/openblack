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
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Creature/CreatureFileLoad.h"
#include "Enums.h"

/// A creature that a script makes for a player from a mind file in the creature mind folder of the game
namespace openblack::ecs::creature_from_file
{

/// What a script gives to make the creature
struct Order
{
	PlayerNames owner {PlayerNames::NEUTRAL};
	/// The row of the species in the creature table, as the scripts give it
	uint32_t speciesRow {0};
	/// The name of the mind file in the creature mind folder
	std::string fileName;
	/// The place on the land, in metres
	glm::vec2 place {0.0f};
};

/// The result: the new creature, or why there is none
struct Result
{
	std::optional<entt::entity> creature;
	creature_file_load::Refusal refusal {creature_file_load::Refusal::None};
	/// Whether the owner had a creature, which the new creature replaces as their creature
	bool replaced {false};
};

/// Whether the player is in the game. The neutral player is always in the game.
[[nodiscard]] bool PlayerInGame(PlayerNames name);

/// Makes the creature on the ground at the place, and makes it the creature of the owner. The mind file gives its mind,
/// its body, its alignment and its tattoos. It knows every leash, and it grows up fully at once.
Result Make(const Order& order);

/// At the start of a network game, the god of a human brings the creature of the file that its player sent before the
/// start. The creature comes at the creature home of the god's citadel, as a creature that a script makes from a mind
/// file: the file gives its species too. A god with no citadel or no file brings no creature.
Result Bring(PlayerNames owner, std::span<const uint8_t> file);

} // namespace openblack::ecs::creature_from_file
