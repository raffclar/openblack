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
#include <span>
#include <string>
#include <vector>

// The small file that the save game room shows for a slot: the slot, the date and time of the save, and its name. The
// file always has 260 bytes. The game fills the space after each text with what was in its memory; this writer fills
// it with zeros.
namespace openblack::gamesave
{

inline constexpr size_t k_SaveGameInfoSize = 260;
/// The characters that each text can keep, its end mark included
inline constexpr size_t k_SaveGameInfoTextLength = 64;
/// The game keeps no date when its text has this many characters or more
inline constexpr size_t k_LongestDate = 62;

struct SaveGameInfo
{
	uint32_t slot {0};
	/// The time and the date of the save, the time first, with a space between
	std::u16string date;
	std::u16string name;

	bool operator==(const SaveGameInfo&) const = default;
};

[[nodiscard]] std::vector<uint8_t> Write(const SaveGameInfo& info);
/// Reads the file, or nothing when it has fewer than 260 bytes
[[nodiscard]] std::optional<SaveGameInfo> ReadSaveGameInfo(std::span<const uint8_t> bytes);

} // namespace openblack::gamesave
