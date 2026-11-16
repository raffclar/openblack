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
#include <string_view>

#include <entt/entity/entity.hpp>

#include "Commands/PlayerCommands.h"
#include "Common/CreatureEditValues.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct Creature;
struct CreatureMindState;
} // namespace openblack::ecs::components

namespace openblack::creature_physiology
{
struct Needs;
}

/// The edits of the editor to the values of a creature in a network game. The editor of this computer does not change
/// the creature itself: it sends each change as a command of the turns, and each computer does it at the same turn.
namespace openblack::editor
{

/// The lowest and the highest amount of a value
struct ValueLimits
{
	float low {0.0f};
	float high {1.0f};
};

/// The limits of a value of a creature. The body shows the size shownSize. The info table has the count of the stages of
/// growing up in stages.
[[nodiscard]] ValueLimits LimitsOf(creature_edit_values::Value value, float shownSize, uint32_t stages) noexcept;

/// The amount in the limits of the value. A number that is not finite gives none.
[[nodiscard]] std::optional<float> AmountInLimits(creature_edit_values::Value value, float amount, float shownSize,
                                                  uint32_t stages) noexcept;

/// The amount of a value of a creature now
[[nodiscard]] float CreatureValueOf(creature_edit_values::Value value, const ecs::components::Creature& creature,
                                    const creature_physiology::Needs& needs,
                                    const ecs::components::CreatureMindState& mind) noexcept;

/// Sets a value of a creature to an amount that is in its limits. It changes only the value itself: ApplyCreatureEdit
/// also changes the body that the value shows.
void SetCreatureValue(creature_edit_values::Value value, float amount, ecs::components::Creature& creature,
                      creature_physiology::Needs& needs, ecs::components::CreatureMindState& mind) noexcept;

/// Does an edit of a creature at the turn, as each computer of a network game does it. False when the world refused the
/// edit: the thing is not a creature, the value is not known or the amount is not a finite number.
bool ApplyCreatureEdit(ecs::Registry& registry, const player_commands::EditorCreatureEdit& edit);

/// Sends a change of a value of a creature as a command of the turns. The change comes at the next turn.
void SendCreatureEdit(entt::entity creature, creature_edit_values::Value value, float amount);

/// Why the world does not make a new creature of the editor, or none when it makes it. The rules keep no state.
[[nodiscard]] std::optional<std::string_view> CreatureSpawnRefusal(const player_commands::EditorCreatureSpawn& spawn) noexcept;

/// Makes a new creature of the editor at the turn, as each computer of a network game does it. Gives the creature, or
/// entt::null when the world refused it.
entt::entity ApplyCreatureSpawn(const player_commands::EditorCreatureSpawn& spawn);

/// Sends a new creature of the creature tools as a command of the turns. The creature comes at the next turn.
void SendCreatureSpawn(const player_commands::EditorCreatureSpawn& spawn);

} // namespace openblack::editor
