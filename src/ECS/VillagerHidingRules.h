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
#include <span>

#include "Enums.h"

/// The rules of a frightened villager taking cover, as the game has them: the building of its town it runs to, how long
/// it stays inside, how long it looks about once out, and how quickly it turns to look back when it panics in the open.
/// Pure: random numbers come from the caller, in the game's order.
namespace openblack::ecs::villager_hiding
{

/// A villager looks for a building to hide in nearer than this to the building's door, in metres
constexpr float k_HideSearchDistance = 100.0f;
/// Inside, it stays more turns than this plus a fresh random number of turns below it, drawn every turn
constexpr uint32_t k_HideTurns = 150;
/// Out again, it looks about more turns than this plus a fresh random number below it, drawn every turn
constexpr uint32_t k_LookAboutTurns = 25;
/// A villager panicking in the open turns to look back at what it ran from by this many of the game's 2048 angle steps
/// a turn: the quickest of the turns a villager makes to look at something
constexpr int32_t k_PanicLookStep = 0x100;

/// Whether a villager can hide in a building of its town: any that works, except the open places and those with no
/// room to hide (fields, football pitches, wonders, town centres and graveyards)
[[nodiscard]] bool CanBeHiddenIn(AbodeType type, bool functional);

/// One of the town's buildings, as a villager looking for cover sees it
struct Shelter
{
	/// From the villager to the building's door, in metres
	float distance {0.0f};
	bool canBeHiddenIn {false};
};

/// The building a villager runs to: the nearest it can hide in, nearer than the search distance; of two as near, the
/// first in the town's list
[[nodiscard]] std::optional<size_t> NearestShelter(std::span<const Shelter> shelters);

/// What a villager that reached its hiding place does this turn
enum class Hiding : uint8_t
{
	/// Its building can no longer hide it: it stops being frightened and goes back to what it was doing
	GiveUp,
	/// It isn't at the door yet: it walks there again
	WalkToDoor,
	/// What frightened it is still too near: it stays inside, the time inside not counting
	StayWhileThreatNear,
	/// It stays inside
	Stay,
	/// It has stayed long enough: it comes out to look about
	ComeOut,
};

/// The villager's hiding this turn: whether its building can still hide it, whether it is at the door, whether what
/// frightened it is still within its keep-away distance, and its turns inside counted so far (before this turn). The
/// random number below the hide turns is drawn only when it counts a turn.
struct HidingTurn
{
	bool shelterStands {false};
	bool atDoor {false};
	bool threatNear {false};
	uint32_t turnsInside {0};
};
struct HidingStep
{
	Hiding what {Hiding::Stay};
	/// Its turns inside after this turn
	uint32_t turnsInside {0};
};
[[nodiscard]] HidingStep HideStep(const HidingTurn& turn, const std::function<uint32_t(uint32_t)>& random);

/// Whether a villager looking about outside, with its turns so far (before this turn), has looked long enough and stops
/// being frightened; and its turns after this one. The random number is drawn every turn.
struct LookAboutStep
{
	bool safe {false};
	uint32_t turns {0};
};
[[nodiscard]] LookAboutStep LookAbout(uint32_t turns, const std::function<uint32_t(uint32_t)>& random);

} // namespace openblack::ecs::villager_hiding
