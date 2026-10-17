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

#include <filesystem>
#include <string>
#include <string_view>

// The slots of the saved games and the names of their folders and files. Each player keeps the saved games in the
// folder "Saved Games" of the player's own folder, one folder for each slot. The slots for the previous land, the
// autosaves and the next land have a folder for each land in the slot's folder.
namespace openblack::gamesave
{

inline constexpr uint32_t k_SlotCount = 21;
/// The slots 0 to 13 are for the player's own saves
inline constexpr uint32_t k_LastPlayerSlot = 13;
inline constexpr uint32_t k_PreviousLandSlot = 14;
inline constexpr uint32_t k_FirstAutosaveSlot = 15;
inline constexpr uint32_t k_LastAutosaveSlot = 17;
inline constexpr uint32_t k_NextLandSlot = 18;
inline constexpr uint32_t k_QuickSaveSlot = 19;
inline constexpr uint32_t k_ExitSlot = 20;

/// The folder of the saved games in the player's folder
inline constexpr std::string_view k_SavedGamesFolder = "Saved Games";
inline constexpr std::string_view k_InfoFileName = "SaveGame.inf";

[[nodiscard]] constexpr bool IsSlot(uint32_t slot)
{
	return slot < k_SlotCount;
}

/// Whether the player can save into the slot from the save game room
[[nodiscard]] constexpr bool IsPlayerSlot(uint32_t slot)
{
	return slot <= k_LastPlayerSlot;
}

/// Whether the world file of the slot is in a folder for each land
[[nodiscard]] constexpr bool HasLandFolder(uint32_t slot)
{
	return slot >= k_PreviousLandSlot && slot <= k_NextLandSlot;
}

/// Whether the creatures' mind files of the slot are in a folder for each land. Only the autosaves have one: the minds
/// of the previous land and the next land share the slot's own folder for every land.
[[nodiscard]] constexpr bool HasLandFolderForMinds(uint32_t slot)
{
	return slot >= k_FirstAutosaveSlot && slot <= k_LastAutosaveSlot;
}

/// The folder of the slot's world file, relative to the player's folder
[[nodiscard]] std::filesystem::path SlotFolder(uint32_t slot, uint32_t land);
/// The folder of the slot's mind files, relative to the player's folder
[[nodiscard]] std::filesystem::path MindFolder(uint32_t slot, uint32_t land);
/// The mind file of the creature at that place in the game's list of creatures
[[nodiscard]] std::string SavedCreatureFileName(uint32_t index);
/// The world file of the slot, in openblack's own layout, with a name that the original game does not read
[[nodiscard]] std::string WorldFileName(uint32_t slot);

} // namespace openblack::gamesave
