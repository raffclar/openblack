/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CM2File.h"

#include <cstring>

namespace
{
constexpr size_t k_HeaderSize = 12;

uint32_t ReadU32(std::span<const uint8_t> bytes, size_t at)
{
	uint32_t value = 0;
	std::memcpy(&value, bytes.data() + at, sizeof(value));
	return value;
}
} // namespace

std::optional<openblack::cam::CM2Path> openblack::cam::ReadCM2(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_HeaderSize)
	{
		return std::nullopt;
	}
	const auto duration = ReadU32(bytes, 4);
	const auto count = ReadU32(bytes, 8);
	if (count == 0 || (bytes.size() - k_HeaderSize) / sizeof(CM2Point) < count)
	{
		return std::nullopt;
	}
	CM2Path path {.durationMilliseconds = duration, .points = std::vector<CM2Point>(count)};
	std::memcpy(path.points.data(), bytes.data() + k_HeaderSize, count * sizeof(CM2Point));
	return path;
}
