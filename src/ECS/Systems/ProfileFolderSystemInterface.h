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
#include <span>
#include <string_view>
#include <vector>

namespace openblack::ecs::systems
{

/// The profile folder keeps the files that the game writes for its players. The game folder stays read only. Each
/// player has a folder of their own in the profile folder, with the name that the game makes from the name of the
/// profile. The game makes each folder when it writes the first file in it.
class ProfileFolderSystemInterface
{
public:
	virtual ~ProfileFolderSystemInterface() = default;

	/// The profile folder. There is no folder when the game cannot keep files for its players.
	[[nodiscard]] virtual std::optional<std::filesystem::path> GetFolder() const = 0;
	/// The folder of the player with the name. This does not make the folder.
	[[nodiscard]] virtual std::optional<std::filesystem::path> GetPlayerFolder(std::u16string_view player) const = 0;
	/// Makes the folder at the path in the profile folder if it is missing. Returns the full path of the folder, or
	/// nothing when the game cannot make it.
	virtual std::optional<std::filesystem::path> MakeFolder(const std::filesystem::path& relative) = 0;

	/// The bytes of a file in the folder of the player. There are no bytes when the file is missing.
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> ReadPlayerFile(std::u16string_view player,
	                                                                         std::string_view file) const = 0;
	/// Writes the bytes to a file in the folder of the player, and makes the folder first if it is missing. Returns
	/// true when it wrote all the bytes.
	virtual bool WritePlayerFile(std::u16string_view player, std::string_view file, std::span<const uint8_t> bytes) = 0;
	/// Removes every file and folder in the folder of the player, as the player's profile is deleted. The empty folder
	/// stays. Returns true when the folder is there and empty after the call.
	virtual bool EmptyPlayerFolder([[maybe_unused]] std::u16string_view player) { return false; }
};

} // namespace openblack::ecs::systems
