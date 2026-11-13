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

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// A map package: an openblack file that holds the parts of a land of a network game that openblack makes itself. The
/// player's game adds the parts that come from the player's own game data, and gets the ".map" file of the land.
///
/// The format, little endian:
/// - the 8 characters "OBMAPPKG"
/// - a 32-bit format number
/// - a 32-bit length of the manifest, of the frame of the script and of the landscape
/// - the manifest, as JSON text
/// - the frame of the script, as text
/// - the landscape (an ".lnd" file)
namespace openblack::onlinemap::package
{

/// The extension of the file of a map package. The file's name is the SHA-256 of its bytes.
inline constexpr std::string_view k_Extension = ".obmap";
/// The first bytes of a map package
inline constexpr std::array<char, 8> k_Magic {'O', 'B', 'M', 'A', 'P', 'P', 'K', 'G'};
/// The format number that this code writes and reads
inline constexpr uint32_t k_Format = 1;

/// A file of the player's game data that the package needs
struct Need
{
	/// The path of the file in the game folder, for example "Online Maps/mpm_4p_1.map"
	std::string file;
	/// The god of the land whose village the package takes, for example "PLAYER_ONE"
	std::string god;

	bool operator==(const Need&) const = default;
};

/// The manifest: what the package is, and what it needs. The JSON keys are the names in quotes.
struct Manifest
{
	/// "file": the name of the ".map" file with no folder and no extension, for example "obm_32p_1"
	std::string file;
	/// "name": the name of the land in the list of lands
	std::string name;
	/// "players": the gods that the land takes
	int32_t players {0};
	/// "needs": the files of the game data that the package needs
	std::vector<Need> needs;
	/// "frame_sha256": the SHA-256 of the frame of the script, as 64 hex digits
	std::string frameSha256;
	/// "landscape_sha256": the SHA-256 of the landscape, as 64 hex digits
	std::string landscapeSha256;

	bool operator==(const Manifest&) const = default;
};

/// The three parts of a map package
struct MapPackage
{
	Manifest manifest;
	/// The script of the land with a mark line in place of each part that comes from the game data
	std::string frame;
	std::vector<uint8_t> landscape;
};

/// The bytes of a package. The same package always gives the same bytes. Nothing when a part is too long for its
/// 32-bit length.
[[nodiscard]] std::optional<std::vector<uint8_t>> Write(const MapPackage& package);

/// The parts of the bytes of a package. Nothing when the bytes do not start with the magic, have another format number,
/// have lengths that do not agree with their size, or have a manifest that does not read.
[[nodiscard]] std::optional<MapPackage> Read(std::span<const std::byte> bytes);

} // namespace openblack::onlinemap::package
