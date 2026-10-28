/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OnlineMapFile.h"

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
