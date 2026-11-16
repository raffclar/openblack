/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string_view>

#include "Commands/PlayerCommands.h"
#include "Common/CreatureEditActs.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// The acts of the creature tools of the editor. Outside a network game the tools do each act at once. In a network game
/// the tools send each act as a command of the turns, and each computer does it at the same turn.
namespace openblack::editor
{

/// The things that the hands tab puts near a creature, for it to pick up (the index of the act PutThing)
inline constexpr std::array<MobileObjectInfo, 6> k_PutThings {
    MobileObjectInfo::Ball,      MobileObjectInfo::EgyptBarrel, MobileObjectInfo::EgyptPotA,
    MobileObjectInfo::MagicFood, MobileObjectInfo::WaterJug,    MobileObjectInfo::MagicWood,
};

/// The kind of thing that each trainer rewards (the index of the act Trainer). The first trainer is none.
[[nodiscard]] std::optional<uint32_t> TrainerKind(size_t trainer) noexcept;

/// The trainer that rewards the kind of thing, or none when no trainer of the list rewards it
[[nodiscard]] std::optional<size_t> TrainerOf(std::optional<uint32_t> kind) noexcept;

/// Why the numbers of the act are out of their limits, or none when the act can go. These limits need no world.
[[nodiscard]] std::optional<std::string_view> ActRefusal(const player_commands::EditorCreatureAct& act) noexcept;

/// What an act did
struct ActResult
{
	/// False when the world refused the act, or when the creature could not do it
	bool done {false};
};

/// Does an act of the creature tools, as each computer of a network game does it at the turn
ActResult ApplyCreatureAct(ecs::Registry& registry, const player_commands::EditorCreatureAct& act);

/// Does the act at once outside a network game. In a network game it sends the act as a command of the turns, and the act
/// comes at the next turn: the result is then none.
std::optional<ActResult> DoCreatureAct(const player_commands::EditorCreatureAct& act);

/// An act with only a creature and its kind
[[nodiscard]] constexpr player_commands::EditorCreatureAct ActOf(entt::entity creature, creature_edit_acts::Act act) noexcept
{
	return {.creature = creature, .act = static_cast<uint8_t>(act)};
}

/// An act with a switch: on or off
[[nodiscard]] constexpr player_commands::EditorCreatureAct SwitchOf(entt::entity creature, creature_edit_acts::Act act,
                                                                    bool on) noexcept
{
	return {.creature = creature, .act = static_cast<uint8_t>(act), .args = {static_cast<uint8_t>(on ? 1 : 0)}};
}

/// An act with an index in its list
[[nodiscard]] constexpr player_commands::EditorCreatureAct IndexOf(entt::entity creature, creature_edit_acts::Act act,
                                                                   size_t index) noexcept
{
	return {.creature = creature, .act = static_cast<uint8_t>(act), .index = static_cast<uint16_t>(index)};
}

} // namespace openblack::editor
