/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::ecs::systems
{

/// The saved games of the current player. A slot keeps the land, the turn and the creatures: each creature's record in
/// the game's layout, its mind file and its player's leash on it. A save is not complete yet: the towns, the villagers,
/// the story and the scripts are not in it. A load puts the creatures of the land that is open back as the save keeps
/// them. A slot of the game itself (its world file in the game's layout) loads its creatures too. A save after such a
/// load also writes the game's file again, with the creatures as openblack has them.
class SaveGameSystemInterface
{
public:
	struct Slot
	{
		/// The slot that the info file keeps
		uint32_t slot {0};
		/// The land of the folder for each land that keeps the save, or 0 for the slot's own folder
		uint32_t land {0};
		std::u16string date;
		std::u16string name;
	};

	/// A creature that came from a world file in the game's layout: whether the record that a save writes for it now has
	/// the same bytes as the file's record
	struct VanillaCheck
	{
		/// The number of the creature in the stream of the file
		uint32_t number {0};
		uint32_t entity {0};
		bool same {false};
		/// The size of the file's record
		size_t size {0};
		/// The first byte of the record that is not the same
		std::optional<size_t> firstDifference;
	};

	virtual ~SaveGameSystemInterface() = default;

	/// Saves the game into the slot with the name. Returns whether it wrote every file.
	virtual bool Save(uint32_t slot, std::u16string_view name) = 0;
	/// Loads the creatures of the slot into the land that is open. Returns whether the slot's land is open and the
	/// slot has a creature that the land has.
	virtual bool Load(uint32_t slot) = 0;
	/// The saved games that the save game room lists, in its order: the folders of the slots in the order the system
	/// lists them, each with its world file and its info file. The autosaves are those of the current land, the previous
	/// land's save that of the land before and the next land's save that of the land after.
	[[nodiscard]] virtual std::vector<Slot> Slots() const = 0;
	/// Deletes the saved game of the slot that the save game room lists: the files in its folder. A folder in the
	/// folder stays, and so does the folder that holds it. Returns whether the room listed a save in the slot.
	virtual bool Delete(uint32_t slot) = 0;
	/// A file of the saved games' pictures, by its name, nothing when there is none
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> ReadPictureFile(std::string_view name) const = 0;
	/// After a load of a world file in the game's layout: the creatures of the file, and whether openblack writes their
	/// records with the same bytes now
	[[nodiscard]] virtual std::vector<VanillaCheck> CheckVanillaRecords() const = 0;
};

} // namespace openblack::ecs::systems
