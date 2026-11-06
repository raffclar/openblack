/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace openblack::server
{

/// Writes the file of the network map for 32 gods, as the server makes it from the game folder, to a file. The tools and
/// the operator of a server use it. Gives the error, or none.
[[nodiscard]] std::optional<std::string> WriteGodsRingMap(const std::filesystem::path& gamePath,
                                                          const std::filesystem::path& out);

/// Writes the land of the map download test (download_test_map), as the tool makes it from the game folder, to a file.
/// Gives the error, or none.
[[nodiscard]] std::optional<std::string> WriteDownloadTestMap(const std::filesystem::path& gamePath,
                                                              const std::filesystem::path& out);

/// A land of a network game that the server reads from a file outside the game folder
struct CustomMapRead
{
	/// The name of the file with no folder and no extension
	std::string file;
	std::vector<uint8_t> bytes;
};

/// Reads the ".map" file of --custom-map. The file name must be a name that a player's store of maps can keep, and the
/// bytes must be a land of a network game of at most the limit of a large transfer. Gives the land or the error.
[[nodiscard]] std::variant<CustomMapRead, std::string> ReadCustomMap(const std::filesystem::path& path);

} // namespace openblack::server
