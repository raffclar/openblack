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

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <ChallengePictureFiles.h>

namespace openblack::ecs::systems
{

/// The saved games of the current player. A slot keeps the whole world in openblack's own layout, and each creature's
/// record in the game's layout, its mind file and its player's leash on it. A save writes its world files to temporary
/// files first, and they take the place of the old files only when every write is complete. A load puts the whole world
/// of the slot in place of the land that is open. A slot of the game itself (its world file in the game's layout) loads
/// its creatures into the land that is open. A save after such a load also writes the game's file again, with the
/// creatures as openblack has them.
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

	/// How the system takes back the pictures of the screen that it asked the renderer for
	struct Screens
	{
		/// The picture made under a name, once; none while it is not made
		std::function<std::optional<std::vector<uint8_t>>(const std::string& name)> take;
		/// Whether the renderer gave up the picture under a name
		std::function<bool(const std::string& name)> failed;
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

	/// The whole world at the end of the last turn, as the bytes of a world file in openblack's own layout, with no file
	/// and no picture. The game server sends it to a player who comes back to a network game.
	[[nodiscard]] virtual std::vector<uint8_t> WorldState() { return {}; }
	/// Puts back the whole world from the bytes that WorldState gave. The land of the bytes starts again, and the world
	/// goes on from the turn of the bytes. Returns whether the world loaded.
	virtual bool LoadWorldState(std::span<const uint8_t> /*bytes*/) { return false; }

	/// The player goes into the temple: the game takes a picture of the screen for the saves that come next
	virtual void TakeTemplePicture() = 0;
	/// A save that the game does by itself into the slot: a new picture of the screen, and the slot's own name. Returns
	/// whether it wrote every file of the world.
	virtual bool InstantSave(uint32_t slot) = 0;
	/// The save as a game ends, into the slot of the save on exit, when the rules let it. When the player quits, the game
	/// makes the screen black before it saves, so the picture of this save is black. When another game starts after this
	/// one, the picture is the screen of the last frame.
	virtual bool SaveOnExit(bool quitting) = 0;
	/// Whether the save on exit of the current player is there and the game can open its world file. A plain start of
	/// the program continues from it.
	[[nodiscard]] virtual bool HasExitSave() const = 0;
	/// The end of a game turn: the game autosaves when an autosave is due
	virtual void EndTurn() = 0;
	/// The keys of the quick save and the quick load. Returns whether the game saved or loaded.
	virtual bool QuickSave() = 0;
	virtual bool QuickLoad() = 0;
	/// A new land starts: the turns go from zero again, so the turn of the last autosave does too
	virtual void NewLand() = 0;
	/// Whether a picture of the screen that the system asked for is not back yet
	[[nodiscard]] virtual bool AnyPictureWaiting() const = 0;
	/// Takes the pictures of the screen that are back, and writes the picture files with them
	virtual void CollectPictures(const Screens& screens) = 0;
	/// Keeps the picture of the challenge at a place of the challenge room in the challenge picture files of the game
	/// that the player plays now. The writer writes the two files that change on its own thread.
	virtual void PutChallengePicture(uint32_t place, std::span<const uint8_t> picture) = 0;
	/// The compressed blocks of the picture of the challenge at a place, as the files of the game that the player plays
	/// now hold them
	[[nodiscard]] virtual gamesave::ChallengePictureCells ChallengePictureCells(uint32_t place) = 0;
	/// Shows the box of a save or a load at once, as a frame of its own: whether it loads, and whether it makes the
	/// screen darker
	using ShowBox = std::function<void(bool loading, bool darken)>;
	/// The game gives the function that shows the box. Each save and each load shows the box before it starts.
	virtual void SetShowBox(ShowBox /*show*/) {}
	/// Tells the player that a save did not write its world files completely
	using ReportProblem = std::function<void()>;
	/// The game gives the function that tells the player
	virtual void SetReportProblem(ReportProblem /*report*/) {}
	/// Once a frame, on the game's thread: the player is told of the saves whose world files did not write completely
	/// since the last frame
	virtual void ReportWriteProblems() {}
};

} // namespace openblack::ecs::systems
