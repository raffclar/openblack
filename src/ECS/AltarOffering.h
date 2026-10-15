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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// A held thing given to a worship site's altar by a player's hand (see altar_sacrifice for the rules)
namespace openblack::ecs::altar_offering
{

/// What the altar does with the thing it is given
enum class Answer : uint8_t
{
	/// The target is not an altar, or the thing is not one an altar takes
	NotAnAltar,
	/// The altar refuses it: it stays in the hand
	Refused,
	/// The altar took it, and it is gone from the world
	Sacrificed,
};

/// Who gives the thing, and where
struct Giver
{
	PlayerNames player {PlayerNames::NEUTRAL};
	/// Where the hand is, where the sacrifice is heard
	glm::vec3 hand {0.0f};
	/// The point the pointer is on, where the number floats up from; none shows no number
	std::optional<glm::vec3> pointed;
	/// The giver is the player at this machine, who alone sees the number
	bool local {true};
};

/// Whether an object is a worship site's altar
[[nodiscard]] bool IsAltar(entt::entity object);
/// Whether an altar takes a held thing of its kind: a villager, an animal, a tree or a dead tree. The altar can still
/// refuse it when it is given.
[[nodiscard]] bool Takes(entt::entity altar, entt::entity held);

/// The held thing is given to the altar. Taken, its prayer power goes into the altar's site; it is out of the hand and
/// gone from the world.
Answer Offer(entt::entity altar, entt::entity held, const Giver& giver);

} // namespace openblack::ecs::altar_offering
