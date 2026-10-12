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

#include <vector>

#include "Creature/CreatureIdleMind.h"

/// A script's "play" for a creature ("CreatureCow play 63 loop 1"): the script first gives the animation and how many
/// times, then tells the creature to start. The creature gives up what it was doing, plays it, waits for its body to be
/// free, and goes back to its own plans. Scripts also ask whether a creature has played what it was doing.
namespace openblack::creature_script_play
{

/// Numbers up to this one are the creature's static actions: number N plays the animations N - 1, N and N + 1 as a
/// start, a loop and an end, as sitting down, sitting and getting up. Those after it are its individual actions, played
/// once each time.
constexpr int32_t k_LastStaticAction = 51;
/// A static action's loop ends this long after its start begins
constexpr float k_StaticActionSeconds = 10.0f;

/// What a script last gave a creature to play
struct Request
{
	int32_t animation {0};
	uint32_t plays {0};
};

/// The agenda for what the script gave: the action that many times, then waiting until its body is free. An individual
/// action is played through each time; a static action starts (left to right half the time), loops until
/// k_StaticActionSeconds after it started and ends. A static action with no start before it never begins, so the agenda
/// never ends.
[[nodiscard]] std::vector<creature_mind::Step> Agenda(const Request& request);

/// Whether a creature has played what it was doing: its agenda is over, or it is only being idle. A creature busy with
/// any agenda, its own or a script's, hasn't.
[[nodiscard]] bool Played(const creature_mind::IdleMind& mind);

} // namespace openblack::creature_script_play
