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

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The print of the game data of a network game: the size and the hash of each file of the game's folder that the
/// world reads. Each player picks the folder of their own copy of the game, so two computers can have other data for
/// the same land. Then the worlds part at once. The server sends the names of the files that it read, each player's
/// computer sends the print of these files from its own folder, and the server names the first file that differs.
namespace openblack::online::game_data
{

/// The most files in a print
inline constexpr size_t k_MostPrintFiles = 512;
/// The most bytes of the path of a file in a print
inline constexpr size_t k_MostPrintPathBytes = 260;

/// The print of one file
struct FilePrint
{
	/// The path relative to the game's folder, with "/" between the parts
	std::string path;
	/// False when the computer does not have the file. Then the size and the hash are 0.
	bool present {false};
	uint64_t size {0};
	uint64_t hash {0};

	[[nodiscard]] bool operator==(const FilePrint&) const = default;
};

/// What the files of the top of the game's folder tell of the copy of the game. Empty when the folder does not tell.
struct InstallHints
{
	/// The last patch of the copy, such as "1.20", from the names of the patch notes
	std::string patch;
	/// The language of the copy, as the 4 hexadecimal digits of the Windows locale in the names of its sound banks
	std::string language;

	[[nodiscard]] bool operator==(const InstallHints&) const = default;
};

/// FNV-1a over the bytes of a file
[[nodiscard]] uint64_t HashOf(std::span<const uint8_t> bytes) noexcept;

/// Whether a path can be in a print: not empty, relative, with no drive, no ".." part, no "\" and not too long
[[nodiscard]] bool IsPrintPath(std::string_view path) noexcept;

/// The kind of a file of the game data, from its path
enum class FileKind : uint8_t
{
	/// A table, a land, a map or a script of the simulation. A difference refuses the join.
	Core,
	/// Another file that the simulation can read, such as a mesh. A difference only goes to the log and the facts.
	Sim,
	/// A sound, a texture, a video or another file that only the player sees or hears. It is not in the print.
	Presentation,
};

[[nodiscard]] FileKind KindOfPath(std::string_view path) noexcept;

/// The paths of a print from the files that the program read, relative to the game's folder: the core files first, then
/// the other files, each group in the order of the names. Each path is there once, and the print has at most
/// k_MostPrintFiles. A path that cannot be in a print, and a presentation file, are left out.
[[nodiscard]] std::vector<std::string> PrintPaths(std::span<const std::filesystem::path> readFiles);

/// The print of a file from its bytes, or of a file that the computer does not have
[[nodiscard]] FilePrint PrintOf(std::string path, const std::optional<std::vector<uint8_t>>& bytes);

/// The hints of the copy from the names of the files at the top of the game's folder
[[nodiscard]] InstallHints HintsOf(std::span<const std::string> topNames);

/// One number for a whole print, for the facts of a desync
[[nodiscard]] uint64_t HashOfPrint(std::span<const FilePrint> files) noexcept;

/// How the file of a player differs from the server's file
enum class DifferenceKind : uint8_t
{
	/// The player does not have the file
	Missing,
	/// The player's file has another size or hash
	Different,
	/// The player did not give a print of the file
	NotGiven,
};

struct Difference
{
	DifferenceKind kind {DifferenceKind::Different};
	FilePrint server;
	/// The player's print. Only the path when the player did not give one.
	FilePrint player;
};

/// The first file, in the order of the server's print, that the player's print does not match. None when each file
/// matches. A file that only the player has does not count: the server's world does not read it.
[[nodiscard]] std::optional<Difference> FirstDifference(std::span<const FilePrint> server, std::span<const FilePrint> player);

/// Whether a player whose file differs cannot join: a core file (KindOfPath). Another file that differs, such as a mesh,
/// only goes to the log and the facts of a desync.
[[nodiscard]] bool RefusesJoin(std::string_view path) noexcept;

/// The difference in words, for the logs and the refusal: "Data/info.dat is not the server's (size 100, hash
/// 0011223344556677; the server has size 101, hash ...)"
[[nodiscard]] std::string Describe(const Difference& difference);

} // namespace openblack::online::game_data
