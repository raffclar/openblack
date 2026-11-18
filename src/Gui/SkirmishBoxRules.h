/*******************************************************************************
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
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The rules of the skirmish box: the lands that its list shows, their order and their names. The box shows every .txt
/// file of the playground folder. These rules keep no state, so that the game and the tests use them both.
namespace openblack::gui::skirmish_box
{

/// The name of a file that the list shows: a name that ends in ".txt", with any case of the letters
[[nodiscard]] bool IsLandFile(std::u16string_view fileName) noexcept;

/// The name of a land without its file's last four characters, the ".txt". A name with fewer than four characters
/// stays as it is.
[[nodiscard]] std::u16string_view Stem(std::u16string_view fileName) noexcept;

/// The text that the list shows in place of the name of a land. Two Gods, Three Gods and Four Gods show the texts that
/// say how many gods the player must defeat. The names match with any case of the letters. Other lands show their
/// names.
[[nodiscard]] std::optional<std::string_view> TitleTextOf(std::u16string_view stem) noexcept;

/// The order of the files in the folder, as the NTFS file system of Windows gives them: by their characters as
/// capitals, one character after the other, and a shorter name before a longer name that starts with it. The game uses
/// this order on every platform. Letters other than the Latin letters of the Basic Latin and Latin-1 blocks keep their
/// case.
[[nodiscard]] bool FolderOrderLess(std::u16string_view a, std::u16string_view b) noexcept;

/// A land of the list
struct Land
{
	/// The name of the file in the playground folder
	std::u16string fileName;
	/// The name of the land without ".txt"
	std::u16string stem;
	/// The text that the list shows in place of the name, if there is one
	std::optional<std::string_view> titleText;
};

/// The lands of the list, in the order of the folder, from the names of the files that the folder holds
[[nodiscard]] std::vector<Land> ListLands(std::span<const std::u16string> fileNames);

/// The computer gods that the box gives a choice of start alignment (deviation 280): the gods of the slots 1, 2 and 3
/// (from 0), which the lands of the playground folder switch on. Slot 0 is the god of the player.
inline constexpr size_t k_AlignmentChoices = 3;
inline constexpr size_t k_FirstComputerSlot = 1;

/// The label of the check box of a computer god: "God 2: random alignment" for the slot 1 (from 0)
[[nodiscard]] std::u16string RandomAlignmentLabel(size_t slot);

/// The god slots (from 0) whose computer god starts at a random alignment, from the check boxes in the order of the box
[[nodiscard]] std::vector<uint8_t> RandomAlignmentSlots(std::span<const bool, k_AlignmentChoices> checked);

/// Start Game shows only while the player has picked a land of the list
[[nodiscard]] constexpr bool StartShows(std::optional<size_t> selection, size_t lands) noexcept
{
	return selection.has_value() && *selection < lands;
}

} // namespace openblack::gui::skirmish_box
