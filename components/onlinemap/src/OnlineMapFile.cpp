/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OnlineMapFile.h"

#include <limits>

namespace
{
constexpr size_t k_HeaderSize = 8;

uint32_t ReadLength(std::span<const std::byte> bytes)
{
	uint32_t value = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
	}
	return value;
}

void WriteLength(std::vector<uint8_t>& out, size_t length)
{
	for (size_t i = 0; i < 4; ++i)
	{
		out.push_back(static_cast<uint8_t>((length >> (8 * i)) & 0xFF));
	}
}
} // namespace

std::optional<openblack::onlinemap::OnlineMapFile> openblack::onlinemap::Parse(std::span<const std::byte> bytes)
{
	if (bytes.size() < k_HeaderSize)
	{
		return std::nullopt;
	}
	const size_t scriptLength = ReadLength(bytes.first(4));
	const size_t landscapeLength = ReadLength(bytes.subspan(4, 4));
	if (bytes.size() - k_HeaderSize != scriptLength + landscapeLength)
	{
		return std::nullopt;
	}
	const auto script = bytes.subspan(k_HeaderSize, scriptLength);
	return OnlineMapFile {
	    .script = std::string_view(reinterpret_cast<const char*>(script.data()), script.size()),
	    .landscape = bytes.subspan(k_HeaderSize + scriptLength, landscapeLength),
	};
}

std::optional<std::vector<uint8_t>> openblack::onlinemap::Write(std::string_view script, std::span<const uint8_t> landscape)
{
	constexpr size_t k_MostLength = std::numeric_limits<uint32_t>::max();
	if (script.size() > k_MostLength || landscape.size() > k_MostLength)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> out;
	out.reserve(k_HeaderSize + script.size() + landscape.size());
	WriteLength(out, script.size());
	WriteLength(out, landscape.size());
	out.insert(out.end(), script.begin(), script.end());
	out.insert(out.end(), landscape.begin(), landscape.end());
	return out;
}
