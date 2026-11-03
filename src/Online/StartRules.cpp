/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StartRules.h"

#include "Online/NetworkLand.h"

namespace openblack::online::start_rules
{

uint64_t LandChecksum(std::span<const uint8_t> file) noexcept
{
	constexpr uint8_t k_LineFeed = 10;
	constexpr uint32_t k_PlaceBits = 0x3F;
	uint32_t low = 0;
	uint32_t high = 0;
	for (size_t place = 0; place < file.size(); ++place)
	{
		const uint32_t byte = file[place];
		const uint32_t lineBonus = byte == k_LineFeed ? static_cast<uint32_t>(place) & k_PlaceBits : 0;
		low += lineBonus + byte;
		high += byte * 9 + byte / 3;
	}
	return (static_cast<uint64_t>(high) << 32) | low;
}

bool IsLandPath(std::string_view land) noexcept
{
	// A name that a server gives is a plain relative name before any other check
	if (!network_land::IsPlainWireName(land) || land.front() == '/' || land.front() == '\\' ||
	    land.find(':') != std::string_view::npos)
	{
		return false;
	}
	// No part of the path goes up a folder
	size_t start = 0;
	while (start <= land.size())
	{
		const auto end = land.find_first_of("/\\", start);
		const auto part = land.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (part == "..")
		{
			return false;
		}
		if (end == std::string_view::npos)
		{
			break;
		}
		start = end + 1;
	}
	return true;
}

game_socket::StartReply Reply(const game_socket::StartQuery& query,
                              const std::optional<std::vector<uint8_t>>& landFile) noexcept
{
	if (query.testbed)
	{
		return {.have = true, .checksum = LandChecksum({})};
	}
	if (!landFile.has_value())
	{
		return {.have = false, .checksum = 0};
	}
	return {.have = true, .checksum = LandChecksum(*landFile)};
}

Verdict Judge(uint64_t serverChecksum, const game_socket::StartReply& reply) noexcept
{
	if (!reply.have)
	{
		return Verdict::Missing;
	}
	return reply.checksum == serverChecksum ? Verdict::Same : Verdict::Different;
}

} // namespace openblack::online::start_rules
