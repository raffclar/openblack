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

#include <entt/entity/entity.hpp>

#include "Enums.h"

/// How the scripts read and write a thing's properties, where the answer is a rule of its own
namespace openblack::script::property_rules
{

/// A building of a town, as the scripts weigh whether the town is wholly destroyed: its life (0 to 1), whether it is a
/// field, and how much of it is built (0 to 1)
struct TownBuilding
{
	float life {1.0f};
	bool field {false};
	float built {1.0f};
};

/// A building still stands while more than this much of it is built (or it is finished)
constexpr float k_StandingBuilt = 0.1f;

/// A town is wholly destroyed when none of its buildings stands: one stands while it has life, isn't a field and is
/// more than a tenth built. A town without buildings is destroyed.
[[nodiscard]] bool TownCompletelyDestroyed(std::span<const TownBuilding> buildings);

/// A thing's player as the scripts number them: 1 for a wholly destroyed town, whoever it belonged to; nothing for a
/// thing of no player or of the neutral player; otherwise the player's number counted from 1
[[nodiscard]] float PlayerProperty(std::optional<PlayerNames> player, bool destroyedTown);

/// Whether a thing is in a creature's hand, as the scripts ask: it is what the creature carries, or what it is eating
[[nodiscard]] bool InCreatureHand(entt::entity thing, entt::entity carried, std::optional<entt::entity> eating);

/// A creature's need that the scripts can set
enum class CreatureNeed : uint8_t
{
	Warmth,
	Energy,
	Itchiness,
	Poo,
	Exhaustion,
	Dehydration,
};

/// The value a creature's need takes when a script sets it: warmth is kept between -1 and 1, energy between 0 and 1,
/// the others are taken as they are
[[nodiscard]] float SetNeed(CreatureNeed need, float value);

} // namespace openblack::script::property_rules
