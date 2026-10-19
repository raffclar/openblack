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
#include <optional>
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
/// The folder that a load of the slot reads its world file from, relative to the player's folder. A save of the previous
/// land or the next land goes into the folder of the land it saves, so on a land the previous land's save is in the
/// folder of the land before, and the next land's save in the folder of the land after.
[[nodiscard]] std::filesystem::path LoadSlotFolder(uint32_t slot, uint32_t land);
/// The slot that the game saves the land that is open into before a load of the slot from the save game room: the next
/// land's slot before a load of the previous land, the previous land's slot before a load of the next land
[[nodiscard]] std::optional<uint32_t> SaveBeforeRoomLoad(uint32_t slot);
/// The folder of the slot's mind files, relative to the player's folder
[[nodiscard]] std::filesystem::path MindFolder(uint32_t slot, uint32_t land);
/// The mind file of the creature at that place in the game's list of creatures
[[nodiscard]] std::string SavedCreatureFileName(uint32_t index);
/// The world file of the slot, in openblack's own layout, with a name that the original game does not read
[[nodiscard]] std::string WorldFileName(uint32_t slot);
/// The world file of the slot in the game's own layout, and the file of the slot's script machine beside it
[[nodiscard]] std::string VanillaWorldFileName(uint32_t slot);
[[nodiscard]] std::string VanillaScriptFileName(uint32_t slot);

/// A folder in "Saved Games" that the save game room reads as it comes up
struct RoomFolder
{
	/// The folder, relative to "Saved Games": the folder found, or for an autosave, the previous land or the next land,
	/// its folder for the land
	std::filesystem::path folder;
	/// The number in the names of the world file the room looks for in the folder, read from the start of the found
	/// folder's name
	int32_t fileSlot {0};
};

/// The number at the start of a folder's name, as the C runtime reads a whole number from text: spaces first, then a
/// sign and the digits. Zero when there are no digits.
[[nodiscard]] int32_t LeadingNumber(std::string_view name);
/// The folder that the save game room reads for a folder that it finds in "Saved Games", in the land of that number.
/// The folders of the autosaves go to the land's folder in them, the previous land's to the folder of the land before
/// (only from the second land), and the next land's to the folder of the land after (only up to the fourth land). The
/// folders "." and ".." give none.
[[nodiscard]] std::optional<RoomFolder> RoomFolderOf(std::string_view name, uint32_t land);
/// The order in which the system lists the folders of a folder: by their names, with the case of the letters ignored
[[nodiscard]] bool ListedBefore(std::string_view a, std::string_view b);

} // namespace openblack::gamesave
