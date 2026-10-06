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

#include <bitset>
#include <optional>

#include "Creature/LeashRules.h"
#include "Enums.h"
#include "Input/GameActionMapInterface.h"

/// The leash's keyboard shortcuts, as the game binds them by default: L puts the picked leash on the player's creature,
/// unties it back to the hand when it is tied to something, or takes it off; V and B pick the previous and next leash
/// the creature knows, swapping the one it wears. Ctrl+L is the quick load, not the leash.
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

} // namespace openblack::creature_leash
