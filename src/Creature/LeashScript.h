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
#include <optional>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::systems
{
class LeashSystemInterface;
} // namespace openblack::ecs::systems

/// What the scripts' leash commands do, given the things they name. A thing the script names that does not exist is
/// none; the commands that need a creature are given none for anything else, and then do nothing. The scripts'
/// messages for a thing that is missing or not a creature are silent, as they are in the shipped game.
/// Docs: docs/bw1-notes/creature.md, Leash natives.
namespace openblack::creature_leash::script
{
using ecs::systems::LeashSystemInterface;

/// Whether a thing is a creature
using IsCreature = std::function<bool(entt::entity)>;

/// The creature and the other thing of a command that names two things
struct CreatureAndThing
{
	entt::entity creature;
	entt::entity thing;
};

/// The two things of a command that names a creature and something else, the creature named first. Either order
/// works: when only the other is a creature, they swap. None when either is missing or neither is a creature.
[[nodiscard]] std::optional<CreatureAndThing> Order(std::optional<entt::entity> creature, std::optional<entt::entity> thing,
                                                    const IsCreature& isCreature);

/// The creature's leash is tied to the thing, put on first if it is not worn
void AttachToThing(LeashSystemInterface& leash, std::optional<entt::entity> creature, std::optional<entt::entity> thing,
                   const IsCreature& isCreature);
/// The creature's leash goes back to the hand: a tied leash is untied, and one not worn is put on if the leash service
/// lets it on (a refusal is the service's, as for the hand)
void AttachToHand(LeashSystemInterface& leash, std::optional<entt::entity> creature);
/// The creature's leash comes off
void Detach(LeashSystemInterface& leash, std::optional<entt::entity> creature);
/// Whether the creature wears a leash; false for no creature
[[nodiscard]] bool IsLeashed(const LeashSystemInterface& leash, std::optional<entt::entity> creature);
/// Whether the leash makes the creature do anything; the script's value is taken as set when it is not zero
void SetWorks(LeashSystemInterface& leash, std::optional<entt::entity> creature, int32_t value);
/// Whether the creature's leash is tied to the thing; false when either is missing or neither is a creature
[[nodiscard]] bool IsLeashedToThing(const LeashSystemInterface& leash, std::optional<entt::entity> creature,
                                    std::optional<entt::entity> thing, const IsCreature& isCreature);
/// The leash picked for the creature, worn or not, as the scripts number it: -1 for none, 1 aggression, 2 learning,
/// 3 compassion. A thing that is missing or not a creature gives 0, which is no leash type.
[[nodiscard]] int32_t TypeOf(const LeashSystemInterface& leash, std::optional<entt::entity> creature);
/// The script's player presses the leash key, for their creature. A script player that is no game player does nothing.
/// Returns whether the leash changed.
bool Toggle(LeashSystemInterface& leash, int32_t scriptPlayer);

} // namespace openblack::creature_leash::script
