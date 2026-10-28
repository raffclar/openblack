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
#include <string>
#include <string_view>

#include "SaveSlots.h"

// The saves that the game does by itself, with no name from the player: the autosaves, the quick save, the save as the
// player leaves the game and the saves that scripts ask for.
namespace openblack::gamesave
{

/// An autosave comes when more turns than this go by after the last autosave: ten minutes of game time
inline constexpr uint32_t k_AutosaveInterval = 6000;
/// The game does not autosave in the Gods' Playground
inline constexpr uint32_t k_PlaygroundLand = 6;
/// A quick save or a quick load needs more turns than this after the last one
inline constexpr int32_t k_QuickTurnGap = 3;

/// The number of the help text that names each slot when the game saves into it by itself. The player's own slots have
/// none, so they take the first help text.
inline constexpr std::array<uint32_t, k_SlotCount> k_SlotNameTexts {0, 0, 0, 0,    0,    0,    0,    0,    0,    0,   0,
                                                                    0, 0, 0, 6688, 6690, 6690, 6690, 6689, 6661, 6691};

/// What the game looks at at the end of each turn to know whether an autosave is due
struct AutosaveState
{
	/// The player's option for autosaves
	bool autosaveOn {true};
	bool gameOver {false};
	/// A script holds the widescreen for a cut scene
	bool scriptCutScene {false};
	bool paused {false};
	/// The game is a network game, which never saves
	bool networkGame {false};
	uint32_t land {0};
	uint32_t turn {0};
	uint32_t lastAutosaveTurn {0};
};

/// Whether the game autosaves at the end of this turn. The count of turns since the last autosave wraps as a whole
/// number with no sign.
[[nodiscard]] constexpr bool AutosaveDue(const AutosaveState& state)
{
	return state.autosaveOn && !state.gameOver && !state.scriptCutScene && !state.networkGame &&
	       state.land != k_PlaygroundLand && state.turn - state.lastAutosaveTurn > k_AutosaveInterval && !state.paused;
}

/// The slot of an autosave: the autosaves of each land go in turn into the three autosave slots, by the count of the
/// land's autosaves that the player's profile keeps
[[nodiscard]] constexpr uint32_t AutosaveSlot(uint32_t landAutosaves)
{
	return k_FirstAutosaveSlot + (landAutosaves % 3);
}

/// Whether the key of the quick save or the quick load does its work: not in a network game, not in a script's cut
/// scene, not after the game is over, and only when the turn is more than three turns from the turn of the last one,
/// forward or back
[[nodiscard]] constexpr bool QuickTurnAllowed(uint32_t turn, uint32_t lastTurn, bool gameOver, bool scriptCutScene,
                                              bool networkGame)
{
	if (gameOver || scriptCutScene || networkGame)
	{
		return false;
	}
	const auto difference = static_cast<int32_t>(lastTurn - turn);
	// The game takes the size of the difference with 32 bits, so the most negative difference stays negative
	const auto size = difference < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(difference)) : difference;
	return size > k_QuickTurnGap;
}

/// The name of a save that the game does by itself: the slot's help text, with the land's number in the place of a
/// number in the text ("%3.0f" writes the number right-aligned in three characters)
[[nodiscard]] std::u16string InstantSaveName(std::u16string_view text, uint32_t land);

} // namespace openblack::gamesave
