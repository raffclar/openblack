/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EXCFile.h"

#include <cstring>

#include <span>
#include <string>
#include <utility>

#include <PackFile.h>

using namespace openblack::exc;

namespace
{
constexpr std::string_view k_ZoneBlock = "cameraexc";
/// The header: the version, the four switches, the two heights and the fence's corner count
constexpr size_t k_HeaderSize = 32;
constexpr size_t k_CornerSize = 12;
/// An exclusion as the game keeps it: two links, an id, its middle, radius and height, its kind and whether it is saved
constexpr size_t k_ExclusionSize = 0x28;

template <typename T>
T Read(std::span<const uint8_t> bytes, size_t offset)
{
	T value {};
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

std::array<float, 3> ReadPoint(std::span<const uint8_t> bytes, size_t offset)
{
	return {Read<float>(bytes, offset), Read<float>(bytes, offset + 4), Read<float>(bytes, offset + 8)};
}
} // namespace

std::string_view openblack::exc::ResultToStr(EXCResult result)
{
	switch (result)
	{
	case EXCResult::Success:
		return "Success";
	case EXCResult::ErrNotABlockFile:
		return "Not a block file";
	case EXCResult::ErrNoZoneBlock:
		return "No cameraexc block";
	case EXCResult::ErrZoneTooSmall:
		return "The cameraexc block is cut short";
	}
	return "Unknown error";
}

EXCResult EXCFile::Open(const std::vector<uint8_t>& buffer)
{
	pack::PackFile file;
	if (file.Open(buffer) != pack::PackResult::Success)
	{
		return EXCResult::ErrNotABlockFile;
	}
	if (!file.HasBlock(std::string(k_ZoneBlock)))
	{
		return EXCResult::ErrNoZoneBlock;
	}
	const std::span<const uint8_t> bytes(file.GetBlock(std::string(k_ZoneBlock)));
	if (bytes.size() < k_HeaderSize)
	{
		return EXCResult::ErrZoneTooSmall;
	}
	EXCZones zones {
	    .version = Read<uint32_t>(bytes, 0),
	    .exclusionsOn = Read<uint32_t>(bytes, 4) != 0,
	    .fenceOn = Read<uint32_t>(bytes, 8) != 0,
	    .useMaxAltitude = Read<uint32_t>(bytes, 12) != 0,
	    .useHeightAboveLand = Read<uint32_t>(bytes, 16) != 0,
	    .maxAltitude = Read<float>(bytes, 20),
	    .heightAboveLand = Read<float>(bytes, 24),
	};
	const auto corners = Read<int32_t>(bytes, 28);
	auto at = k_HeaderSize;
	for (int32_t i = 0; i < corners; ++i)
	{
		if (at + k_CornerSize > bytes.size())
		{
			return EXCResult::ErrZoneTooSmall;
		}
		zones.fence.push_back(ReadPoint(bytes, at));
		at += k_CornerSize;
	}
	if (at + 8 > bytes.size())
	{
		return EXCResult::ErrZoneTooSmall;
	}
	const auto count = Read<int32_t>(bytes, at);
	const auto size = Read<uint32_t>(bytes, at + 4);
	at += 8;
	// Exclusions of another size than the game's own are read past and dropped, as the game does
	for (int32_t i = 0; i < count; ++i)
	{
		if (at + size > bytes.size())
		{
			return EXCResult::ErrZoneTooSmall;
		}
		if (size == k_ExclusionSize)
		{
			zones.exclusions.push_back({
			    .position = ReadPoint(bytes, at + 12),
			    .radius = Read<float>(bytes, at + 24),
			    .height = Read<float>(bytes, at + 28),
			    .kind = static_cast<EXCExclusion::Kind>(Read<uint32_t>(bytes, at + 32)),
			});
		}
		at += size;
	}
	_zones = std::move(zones);
	return EXCResult::Success;
}
