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

#include <memory>
#include <optional>
#include <span>
#include <string>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Creature/ClanCreatureOrder.h"
#include "Creature/CreatureFileLoad.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
}

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
	/// Whether the owner had a creature, which the new creature of the script replaces as their creature. A god that
	/// brings a creature never replaces one.
	bool replaced {false};
};

/// Whether the player is in the game. The neutral player is always in the game.
[[nodiscard]] bool PlayerInGame(PlayerNames name);

/// Makes the creature on the ground at the place, and makes it the creature of the owner. The mind file gives its mind,
/// its body, its alignment and its tattoos. It knows every leash, and it grows up fully at once.
Result Make(const Order& order);

/// At the start of a network game, the god of a human brings the creature of the file that its player sent before the
/// start. The creature comes at the creature home of the god's citadel, as a creature that a script makes from a mind
/// file: the file gives its species too. A god with no citadel, no file or a creature already brings no creature.
Result Bring(PlayerNames owner, std::span<const uint8_t> file);

/// At the start of a network game, the god of a human brings a new clan creature, as the order in the start data of its
/// player asks. The creature comes at the creature home of the god's citadel, with the species and the name of the
/// order. It has no mind file. It takes each stage up to the last, it knows every leash, and it grows up fully at once.
/// A god with no citadel or a creature already, or an order for a row past the creature table, brings no creature.
Result BringClanOrder(PlayerNames owner, const clan_creature_order::Order& order);

/// At the start of a skirmish, the god of the player brings the creature that the player's profile keeps. The creature
/// comes at the creature home of the god's citadel, as Bring makes it. A god with no citadel, no creature kept or a
/// creature already brings no creature.
Result BringKept(PlayerNames owner, std::shared_ptr<const creaturemind::MindFileData> mind);

} // namespace openblack::ecs::creature_from_file
