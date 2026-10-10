/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HNDFile.h"

#include <cstring>

using namespace openblack::hnd;

namespace
{
template <typename T>
T Read(std::span<const uint8_t> record, size_t offset)
{
	T value;
	std::memcpy(&value, record.data() + offset, sizeof(value));
	return value;
}

template <size_t N>
std::array<float, N> ReadFloats(std::span<const uint8_t> record, size_t offset)
{
	std::array<float, N> values {};
	std::memcpy(values.data(), record.data() + offset, sizeof(values));
	return values;
}
} // namespace

std::string_view openblack::hnd::ResultToStr(HNDResult result)
{
	switch (result)
	{
	case HNDResult::Success:
		return "Success";
	case HNDResult::ErrPartialRecord:
		return "The file isn't a whole number of records";
	}
	return "Unknown error";
}

HNDResult HNDFile::Open(std::span<const uint8_t> buffer)
{
	if (buffer.size() % k_RecordSize != 0)
	{
		return HNDResult::ErrPartialRecord;
	}
	records.clear();
	records.reserve(buffer.size() / k_RecordSize);
	for (size_t offset = 0; offset < buffer.size(); offset += k_RecordSize)
	{
		const auto bytes = buffer.subspan(offset, k_RecordSize);
		records.push_back({
		    .message = static_cast<HNDMessage>(Read<uint32_t>(bytes, 0x00)),
		    .heldPlacement = ReadFloats<12>(bytes, 0x04),
		    .cursor = ReadFloats<2>(bytes, 0x34),
		    .cameraPosition = ReadFloats<3>(bytes, 0x3C),
		    .cameraFocus = ReadFloats<3>(bytes, 0x48),
		    .hints = Read<uint32_t>(bytes, 0x54),
		    .hintValue = Read<float>(bytes, 0x58),
		    .trigger = Read<uint32_t>(bytes, 0x5C),
		    .time = Read<uint32_t>(bytes, 0x60),
		    .objectDistance = Read<float>(bytes, 0x64),
		    .objectPosition = {.x = Read<int32_t>(bytes, 0x68),
		                       .z = Read<int32_t>(bytes, 0x6C),
		                       .altitude = Read<float>(bytes, 0x70)},
		    .objectType = Read<uint32_t>(bytes, 0x74),
		    .objectSubtype = Read<uint32_t>(bytes, 0x78),
		});
	}
	return HNDResult::Success;
}
