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

#include <optional>
#include <string>
#include <string_view>

/// The version of this build of openblack. The build writes the definitions from the project's version and the git
/// commit of the sources. The build also writes the full commit hash into a text file next to the game (version.txt),
/// with "-dirty" when the source tree had changes, or "unknown" when git did not give it. The game reads this file at
/// start, and the web page reads the same file.
namespace openblack::build_version
{

/// The semantic version of the project, such as "0.1.0"
[[nodiscard]] std::string_view Semantic() noexcept;
/// The first 12 characters of the git commit of the sources, or "unknown" when the build had no git
[[nodiscard]] std::string_view Commit() noexcept;
/// The semantic version and the commit, such as "0.1.0+0123456789ab". The --version option writes it, and the game
/// list compares it with the version of each game server.
[[nodiscard]] std::string_view Text() noexcept;

/// The name of the file next to the game
constexpr std::string_view k_FileName = "version.txt";
/// The text for a build that does not know its commit
constexpr std::string_view k_Unknown = "unknown";
/// The text after the hash when the source tree had changes
constexpr std::string_view k_DirtySuffix = "-dirty";
/// The number of characters of the short hash
constexpr size_t k_ShortLength = 7;
/// The number of characters of a full git commit hash
constexpr size_t k_FullLength = 40;
/// The address of a commit on GitHub, without the hash
constexpr std::string_view k_CommitAddress = "https://github.com/raffclar/openblack/commit/";

struct BuildVersion
{
	/// The full commit hash in lower case, or empty when not known
	std::string hash;
	/// The source tree had changes that are not in the commit
	bool dirty {false};

	[[nodiscard]] bool Known() const noexcept { return !hash.empty(); }
	/// The full hash with "-dirty" when the tree had changes, or "unknown"
	[[nodiscard]] std::string Text() const;
	/// The first characters of the hash with "-dirty" when the tree had changes, or "unknown"
	[[nodiscard]] std::string Short() const;
	/// The address of the commit on GitHub, when the hash is known
	[[nodiscard]] std::optional<std::string> CommitAddress() const;
};

/// The version in the text of the file. Spaces and line ends around the text do not change it. Text that is not a full
/// hash, with or without "-dirty", gives an unknown version.
[[nodiscard]] BuildVersion Parse(std::string_view text);

} // namespace openblack::build_version
