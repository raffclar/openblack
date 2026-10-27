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

/// The openblack commit that a build comes from. The build writes it into a text file next to the game (version.txt):
/// the full git commit hash, with "-dirty" when the source tree had changes, or "unknown" when git did not give it.
/// The game reads this file at start, and the web page reads the same file.
namespace openblack::build_version
{

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
