/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpStatsFile.h"

#include <cstring>

#include <bit>

namespace openblack::helpstats
{

// The file holds the counts as the game held them in its memory, in the byte order of little-endian machines
static_assert(std::endian::native == std::endian::little);

std::optional<HelpStats> Read(std::span<const uint8_t> bytes)
{
	if (bytes.size() != k_FileSize)
	{
		return std::nullopt;
	}
	HelpStats stats {};
	std::memcpy(stats.data(), bytes.data(), k_FileSize);
	return stats;
}

std::vector<uint8_t> Write(const HelpStats& stats)
{
	std::vector<uint8_t> bytes(k_FileSize);
	std::memcpy(bytes.data(), stats.data(), k_FileSize);
	return bytes;
}

} // namespace openblack::helpstats
