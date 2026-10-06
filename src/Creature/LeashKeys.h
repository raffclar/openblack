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

#include <array>
#include <bitset>
#include <optional>

#include <glm/vec2.hpp>

#include "Creature/LeashRules.h"
#include "Enums.h"
#include "Input/GameActionMapInterface.h"

/// The leash's controls. The keyboard shortcuts, as the game binds them by default: L puts the picked leash on the
/// player's creature, unties it back to the hand when it is tied to something, or takes it off; V and B pick the
/// previous and next leash the creature knows, swapping the one it wears. Ctrl+L is the quick load, not the leash. The
/// right mouse button clicked on the player's creature puts the leash on (see creature_hand::IsClick), and shaking the
/// hand takes a leash held in the hand off again.
namespace openblack::creature_leash
{

/// A leash shortcut
enum class LeashKey : uint8_t
{
	Leash,
	PreviousLeash,
	NextLeash,
};

/// The shortcut an action of the game's controls is, if it is one of them
[[nodiscard]] std::optional<LeashKey> KeyFor(input::BindableActionMap action);

/// What the shortcuts need to know about the player's creature
struct KeyState
{
	/// Whether it wears the leash, and whether the leash is tied to something rather than held
	bool worn {false};
	bool tied {false};
	/// The leashes it knows, by their place in k_Types
	std::bitset<k_Types.size()> known;
	/// The leash picked to put on next, or the one worn
	LeashType selected {LeashType::Rope};
};

/// What a shortcut does
struct KeyCommand
{
	enum class Kind : uint8_t
	{
		/// Nothing, as for a creature that knows no leash
		None,
		PutOn,
		TakeOff,
		UntieToHand,
		ChangeType,
	};
	Kind kind {Kind::None};
	/// The leash put on or changed to
	LeashType type {LeashType::None};

	bool operator==(const KeyCommand&) const = default;
};

/// What pressing the shortcut does to the player's creature. The leash key needs the learning leash known, as every
/// leash does; the picked leash goes on when known, the learning leash otherwise.
[[nodiscard]] KeyCommand CommandFor(LeashKey key, const KeyState& state);
/// The next leash the creature knows after the picked one, going round, forwards or backwards; none when it knows no
/// other
[[nodiscard]] std::optional<LeashType> StepKnown(LeashType selected, const std::bitset<k_Types.size()>& known, bool forwards);

/// Shaking the hand: moving it quickly back and forth. The game recognises a shake as one of the gestures it matches
/// drawn hand paths against; these thresholds are tuned to feel the same rather than taken from it. The cursor is
/// measured in screen heights, so the shake is the same at any resolution.
/// Each swing must cover this much of the screen's height before it turns back
constexpr float k_ShakeSwing = 0.04f;
/// It takes this many turns, within this many seconds, to make a shake
constexpr size_t k_ShakeTurns = 4;
constexpr float k_ShakeSeconds = 0.8f;

/// The cursor's swings along one axis of the screen
struct ShakeAxis
{
	/// Which way it is swinging, +1 or -1, or 0 before it has swung far enough to tell
	int direction {0};
	/// Where the swing started, and the furthest it has gone since
	float start {0.0f};
	float furthest {0.0f};
};

/// Follows the cursor to tell when the hand is shaken
struct ShakeTracker
{
	bool started {false};
	std::array<ShakeAxis, 2> axes {};
	/// When each recent turn was, in seconds of the tracker's clock, along either axis
	std::array<float, k_ShakeTurns> turns {};
	size_t turnCount {0};
	float clock {0.0f};
};

/// Moves the cursor on, some seconds after the last time: whether the hand has now been shaken, which starts the
/// tracker afresh. The cursor is in screen heights from the top left.
[[nodiscard]] bool TrackShake(ShakeTracker& tracker, glm::vec2 cursor, float seconds);

} // namespace openblack::creature_leash
