/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceShapeFile.h"

#include <cstring>

using namespace openblack::dance;

namespace
{
template <typename T>
bool Read(std::span<const uint8_t> buffer, std::size_t& offset, T& value)
{
	if (buffer.size() - offset < sizeof(value))
	{
		return false;
	}
	std::memcpy(&value, buffer.data() + offset, sizeof(value));
	offset += sizeof(value);
	return true;
}
} // namespace

DanceResult DanceShapeFile::Open(std::span<const uint8_t> buffer)
{
	std::size_t offset = 0;
	uint32_t count = 0;
	if (!Read(buffer, offset, count))
	{
		return DanceResult::ErrTruncated;
	}
	points.clear();
	for (uint32_t i = 0; i < count; ++i)
	{
		DanceShapePoint point;
		if (!Read(buffer, offset, point.x) || !Read(buffer, offset, point.z))
		{
			return DanceResult::ErrTruncated;
		}
		points.push_back(point);
	}
	return offset == buffer.size() ? DanceResult::Success : DanceResult::ErrTrailingBytes;
}
