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

#include <entt/entity/fwd.hpp>

/// The scripts' timers.
///
/// - A script timer is a thing at (0, 0, 0) with two words: the game turn it was set at (game_clock::Turn()) and its
///   length in turns. It is never processed: the time left is worked out from the turn when asked, so it runs with the
///   game turns (it stops in pause and follows the game speed). Its script object type is SCRIPT_OBJECT_TYPE_TIMER
///   (0x11), its save type 0x7D, and it is deleted once no script variable holds it any more.
/// - The countdown timer is the script engine's own: on, turns left, shown. It is counted down once a turn and drawn by
///   the game's 3D frame.
///
/// openblack: a timer is an entity with only components::ScriptTimer (no Transform: the original's position is never
/// set and nothing reads it); its script slot is ecs::script_held's. Its deletion on release is ScriptHeld.cpp's
/// (pending review): without it a timer stays until the end of the land.
namespace openblack::ecs::components
{

/// The turn a script timer was set at and its length
struct ScriptTimer
{
	uint32_t startTurn {0};    ///< game_clock::Turn() when set
	int32_t durationTurns {0}; ///< The length in turns
};

} // namespace openblack::ecs::components

namespace openblack::ecs::script_timer
{

/// Sets the start to `now` and the length to `turns`
void SetTurns(components::ScriptTimer& timer, int32_t turns, uint32_t now);
/// Sets the length from seconds: SetTurns(1000 / ms per turn (unsigned division) x seconds, truncated toward zero) =
/// game_clock::TicksForSeconds
void SetSeconds(components::ScriptTimer& timer, float seconds, uint32_t now);
/// The turns since it was set (unsigned)
[[nodiscard]] uint32_t ElapsedTurns(const components::ScriptTimer& timer, uint32_t now);
/// d = length - elapsed (signed), 0 below 0; d x ms per turn x 0.001, each step a float
[[nodiscard]] float RemainingSeconds(const components::ScriptTimer& timer, uint32_t now, uint32_t msPerTurn);
/// elapsed x ms per turn x 0.001 (it keeps growing after the timer ran out)
[[nodiscard]] float SecondsSinceSet(const components::ScriptTimer& timer, uint32_t now, uint32_t msPerTurn);

/// A new script timer, SetSeconds(seconds) at the current turn. CREATE_TIMER 146 and CREATE 027 with
/// SCRIPT_OBJECT_TYPE_TIMER (the sub-type as the seconds). The caller adds it to the script. (not ported) the script
/// name of the create
[[nodiscard]] entt::entity Create(float seconds);
/// Whether the thing is a script timer
[[nodiscard]] bool IsTimer(entt::entity thing);
/// SET_TIMER_TIME on a timer: false when `thing` is not one
bool SetTime(entt::entity thing, float seconds);
/// GET_TIMER_TIME_REMAINING: nullopt when `thing` is not a timer
[[nodiscard]] std::optional<float> Remaining(entt::entity thing);
/// GET_TIMER_TIME_SINCE_SET: nullopt when `thing` is not a timer
[[nodiscard]] std::optional<float> SinceSet(entt::entity thing);

/// A script timer saves and loads, after the base thing's own block, the start turn then the length (4 bytes each).
/// (not ported) openblack does not save the scripts' things; this is the record those would write
struct SaveRecord
{
	uint32_t startTurn;
	int32_t durationTurns;
};
[[nodiscard]] SaveRecord ToSave(const components::ScriptTimer& timer);
[[nodiscard]] components::ScriptTimer FromSave(const SaveRecord& record);

} // namespace openblack::ecs::script_timer

namespace openblack::ecs::script_countdown
{

/// The script engine's countdown timer
struct State
{
	bool on {false}; ///< COUNTDOWN_TIMER_EXISTS pushes it
	int32_t turnsLeft {0};
	bool shown {false}; ///< HIDE / REVEAL
};

/// START_COUNTDOWN_TIMER 084: on and shown; seconds <= 0 is the script error "Invalid time for timer" (returned as
/// false), below 0 it counts as 0; turns = 1000 / ms per turn x seconds, truncated toward zero
bool Start(float seconds);
/// REMOVE_COUNTDOWN_TIMER 087: off
void Remove();
/// GET_COUNTDOWN_TIMER 092: turns left / (1000 / ms per turn), both unsigned integer divisions, as a float: whole
/// seconds (it reads the turns left even when the timer is off)
[[nodiscard]] float RemainingSeconds();
/// COUNTDOWN_TIMER_EXISTS 099
[[nodiscard]] bool Exists();
/// HIDE_COUNTDOWN_TIMER 103 / REVEAL_COUNTDOWN_TIMER 144
void SetShown(bool shown);
/// Once a turn, before the scripts run: when on, --turns, off at 0
void ProcessTurn();
/// What the game's 3D frame draws while on and shown: sprintf "Time: %.1f" of RemainingSeconds at (320, 90), size
/// 24.0, red below 10 s else green ((inferred) the colour arguments are r, g, b), through the creature mind editor's
/// text drawer. (pending) openblack has no such text drawer: nobody draws it yet
struct Display
{
	bool visible;
	float seconds;
	bool red;
};
[[nodiscard]] Display GetDisplay();
[[nodiscard]] const State& Get();
/// A new game / the tests ((inferred) the script engine starts with the three at 0; not read)
void Reset();

} // namespace openblack::ecs::script_countdown
