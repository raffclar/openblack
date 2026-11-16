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
#include <variant>
#include <vector>

namespace openblack::server
{

/// Writes the file of the network map for 32 gods, as the server makes it from the game folder, to a file. The tools and
/// the operator of a server use it. Gives the error, or none.
[[nodiscard]] std::optional<std::string> WriteGodsRingMap(const std::filesystem::path& gamePath,
                                                          const std::filesystem::path& out);

/// Writes the map package of the network map for 32 gods (deviation 249) to a folder, as <SHA-256>.obmap. The package has
/// no game data, so it needs no game folder. Gives the path of the file, or the error.
[[nodiscard]] std::variant<std::filesystem::path, std::string> WriteMapPackage(const std::filesystem::path& folder);

/// Writes the land of the map download test (download_test_map), as the tool makes it from the game folder, to a file.
/// Gives the error, or none.
[[nodiscard]] std::optional<std::string> WriteDownloadTestMap(const std::filesystem::path& gamePath,
                                                              const std::filesystem::path& out);

/// A land of a network game that the server reads from a file outside the game folder
struct CustomMapRead
{
	/// The name of the file with no folder and no extension
	std::string file;
	/// The name of the land in the list of the lands
	std::string name;
	std::vector<uint8_t> bytes;
	/// The SHA-256 of the map package that gave the land, as 64 hex digits. Empty for a ".map" file.
	std::string packageSha256;
	/// The bytes of the map package
	int64_t packageSize {0};
};

/// Reads the ".map" file of --custom-map. The file name must be a name that a player's store of maps can keep, and the
/// bytes must be a land of a network game of at most the limit of a large transfer. Gives the land or the error.
[[nodiscard]] std::variant<CustomMapRead, std::string> ReadCustomMap(const std::filesystem::path& path);

/// Reads the map package of a custom land (--map-package, deviation 249). The package must need no game data, and its
/// parts must have the hashes of its manifest. Gives the land with the name of the manifest, or the error.
[[nodiscard]] std::variant<CustomMapRead, std::string> ReadMapPackage(const std::filesystem::path& path);

/// Writes the map package of the custom land in a ".map" file to a folder, as <SHA-256>.obmap. An empty name gives the
/// name of the file. The operator of a site puts the package in the folder of map packages. Gives the path or the error.
[[nodiscard]] std::variant<std::filesystem::path, std::string> WriteCustomMapPackage(const std::filesystem::path& customMap,
                                                                                     std::string_view name, int32_t players,
                                                                                     const std::filesystem::path& folder);

/// Writes the ".map" file that a map package of a custom land gives, so that the slot agent can send it to the lobby.
/// Gives the error, or none.
[[nodiscard]] std::optional<std::string> WritePackageMap(const std::filesystem::path& package,
                                                         const std::filesystem::path& out);

} // namespace openblack::server
