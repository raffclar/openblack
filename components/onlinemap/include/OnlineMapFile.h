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
#include <string_view>
#include <vector>

/// A land of a network game, as the game keeps it in its "Online Maps" folder: a ".map" file. The file has two parts:
/// the land's script and the landscape (an ".lnd" file).
///
/// The format, little endian:
/// - a 32-bit length of the script
/// - a 32-bit length of the landscape
/// - the script, as text
/// - the landscape
namespace openblack::onlinemap
{

/// Views over the two parts of a ".map" file. The views point into the bytes that the file gave.
struct OnlineMapFile
{
	std::string_view script;
	std::span<const std::byte> landscape;
};

/// The two parts of the bytes of a ".map" file. Nothing when the lengths do not agree with the size of the bytes.
[[nodiscard]] std::optional<OnlineMapFile> Parse(std::span<const std::byte> bytes);

/// The bytes of a ".map" file with the two parts. Nothing when a part is too long for its 32-bit length.
[[nodiscard]] std::optional<std::vector<uint8_t>> Write(std::string_view script, std::span<const uint8_t> landscape);

} // namespace openblack::onlinemap
