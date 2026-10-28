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
#include <vector>

#include "Commands/PlayerCommands.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/Components/CreatureSkin.h"

/// A change of a creature's tattoos from the tattoo editor, and the tattoos that this computer shows on the creature
namespace openblack::ecs::creature_tattoo_change
{

/// Sends one command to the turn
using Submit = std::function<void(player_commands::CreatureTattoo)>;

/// Puts a change from the tattoo editor on the creature. There are three cases:
/// - In a game alone, the change goes to the turn as a command. Until the turn, the skin shows the change.
/// - In a network game, the change stays on this computer. The skin shows it, and the creature keeps the tattoos that it
///   brought into the game. The other computers do not see the change during the game.
/// - With no submit, the creature gets the change at once.
void Apply(components::CreatureTattoos& tattoos, player_commands::CreatureTattoo change, bool networkGame,
           const Submit& submit);

/// The slots that this computer shows on the creature: a change from the tattoo editor first, else the creature's slots
[[nodiscard]] const creature_tattoo::Slots& ShownSlots(const components::CreatureTattoos& tattoos);

/// The block of pictures that this computer shows on the creature: the block of a change first, else the creature's block
[[nodiscard]] const std::vector<uint8_t>& ShownPictures(const components::CreatureTattoos& tattoos);

} // namespace openblack::ecs::creature_tattoo_change
