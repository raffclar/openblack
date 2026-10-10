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

#include "ECS/Components/Dance.h"

// How a dance runs from turn to turn: when it starts, how fast it goes and when its clock starts over. Pure rules on the
// dance's component.

namespace openblack::ecs::dance_rules
{

/// The game's turns in a second, by which the dance's times are reckoned
inline constexpr uint32_t k_TurnsPerSecond = 10;
/// Waiting for the dancers on their way, a dance starts at the latest this many seconds after its first dancer came
inline constexpr uint32_t k_LongestWaitSeconds = 90;

/// The rate the groups move at for a speed of 0 to 1: the speed in tenths, rounded down, times 0.4
[[nodiscard]] float RateForSpeed(float speed);

/// The dance is asked to go at a speed: its rate follows, and a new rate while it is danced starts it over
void SetSpeed(components::Dance& dance, float speed);

/// A worship site's dance follows how hard its dancers are chanting: danced while the intensity is above 0, stopped at
/// 0, at that speed
void SetWorshipSpeed(components::Dance& dance, float intensity);

/// Whether enough of its dancers have come for it to start: more than half of those on their way, or the longest wait
/// gone by since the first came. Notes the turn the first dancer came.
bool HasProperlyStarted(components::Dance& dance, uint32_t turn);

/// One game turn of the dance: a stopped dance that starts by itself does once its dancers have come; one that lasts
/// a while stops once that is over; while danced, its clock goes on, starting over at the end of its loop
void ProcessTurn(components::Dance& dance, bool startsAutomatically, uint32_t turn);

/// How many turns the dance's loop lasts before its clock starts over
[[nodiscard]] uint32_t LoopTurns(const components::Dance& dance);

} // namespace openblack::ecs::dance_rules
