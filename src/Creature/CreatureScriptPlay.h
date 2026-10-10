/*******************************************************************************
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
#include <vector>

#include "Creature/CreatureIdleMind.h"

/// A script's "play" for a creature ("CreatureCow play 63 loop 1"): the script first gives the animation and how many
/// times, then tells the creature to start. The creature gives up what it was doing, plays it, waits for its body to be
/// free, and goes back to its own plans. Scripts also ask whether a creature has played what it was doing.
namespace openblack::creature_script_play
{

/// Numbers up to this one are the creature's static actions (a start, a loop and an end, as sitting); those after it
/// are its individual actions, played once each time
constexpr int32_t k_LastStaticAction = 51;
/// A static action's loop lasts this many seconds
constexpr float k_StaticActionSeconds = 10.0f;

/// What a script last gave a creature to play
struct Request
{
	int32_t animation {0};
	uint32_t plays {0};
};

/// The agenda for what the script gave: the individual action that many times, then waiting until its body is free.
/// None for a static action, whose start, loop and end openblack doesn't have the table of yet.
[[nodiscard]] std::optional<std::vector<creature_mind::Step>> Agenda(const Request& request);

/// Whether a creature has played what it was doing: its agenda is over, or it is only being idle. A creature busy with
/// any agenda, its own or a script's, hasn't.
[[nodiscard]] bool Played(const creature_mind::IdleMind& mind);

} // namespace openblack::creature_script_play
