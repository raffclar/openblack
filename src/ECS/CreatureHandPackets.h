/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"
#include "Input/GamePackets.h"

namespace openblack::ecs::systems
{
class CreatureMindSystemInterface;
class LeashSystemInterface;
} // namespace openblack::ecs::systems

/// The two packets the hand sends about a creature it held, applied at the start of the next turn as every packet of
/// the hand. Wiki: docs/bw1-notes/hand-and-interface.md, "The hand on a creature".
namespace openblack::ecs::creature_hand_packets
{

/// How the hand treated a creature (the packet's data[0], from -1 to 1) reaches its mind, if the creature is still there
void ApplyFeedback(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet);

/// The player clicked their own creature: the same as their leash key
void ApplyLeashClick(systems::LeashSystemInterface* leash, PlayerNames player);

} // namespace openblack::ecs::creature_hand_packets
