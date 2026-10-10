/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EDTFile.h"

#include <charconv>
#include <cstring>

#include <optional>
#include <span>
#include <string>

#include <PackFile.h>

using namespace openblack::edt;

namespace
{
/// A way's header: its size, two unknowns, its point count, its length and duration, and four pointers the game fills
/// in on load
constexpr size_t k_WayHeaderSize = 0x24;
/// Each point's bytes: the point, its two handles, its time and its speed
constexpr size_t k_WayPointSize = 12 + 24 + 4 + 4;
constexpr size_t k_CameraSize = 32;

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

/// The number after a block name's prefix, when the name is the prefix and digits only
std::optional<int32_t> NumberAfter(std::string_view name, std::string_view prefix)
{
	if (!name.starts_with(prefix) || name.size() == prefix.size())
	{
		return std::nullopt;
	}
	const auto digits = name.substr(prefix.size());
	int32_t number = 0;
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), number);
	if (error != std::errc() || end != digits.data() + digits.size())
	{
		return std::nullopt;
	}
	return number;
}

/// The way at `offset` and the bytes it takes, nothing when it is cut short or its size doesn't fit its points
std::optional<std::pair<EDTWay, size_t>> ReadWay(std::span<const uint8_t> bytes, size_t offset)
{
	if (offset + k_WayHeaderSize > bytes.size())
	{
		return std::nullopt;
	}
	const auto size = Read<uint16_t>(bytes, offset);
	const auto count = Read<uint32_t>(bytes, offset + 4);
	if (count < 2 || size != k_WayHeaderSize + k_WayPointSize * count || offset + size > bytes.size())
	{
		return std::nullopt;
	}
	EDTWay way {
	    .unknown2 = Read<uint16_t>(bytes, offset + 2),
	    .unknown8 = Read<float>(bytes, offset + 8),
	    .duration = Read<int32_t>(bytes, offset + 0x10),
	};
	auto at = offset + k_WayHeaderSize;
	way.points.reserve(count);
	for (uint32_t i = 0; i < count; ++i, at += 12)
	{
		way.points.push_back(ReadPoint(bytes, at));
	}
	way.handles.reserve(count);
	for (uint32_t i = 0; i < count; ++i, at += 24)
	{
		way.handles.push_back({ReadPoint(bytes, at), ReadPoint(bytes, at + 12)});
	}
	way.times.reserve(count);
	for (uint32_t i = 0; i < count; ++i, at += 4)
	{
		way.times.push_back(Read<float>(bytes, at));
	}
	way.speeds.reserve(count);
	for (uint32_t i = 0; i < count; ++i, at += 4)
	{
		way.speeds.push_back(Read<float>(bytes, at));
	}
	return std::make_pair(std::move(way), static_cast<size_t>(size));
}
} // namespace

std::string_view openblack::edt::ResultToStr(EDTResult result)
{
	switch (result)
	{
	case EDTResult::Success:
		return "Success";
	case EDTResult::ErrNotABlockFile:
		return "Not a block file";
	case EDTResult::ErrCameraTooSmall:
		return "A camera block is too small";
	case EDTResult::ErrTrackTooSmall:
		return "A track block is too small";
	case EDTResult::ErrWayMalformed:
		return "A track's way is cut short or malformed";
	}
	return "Unknown error";
}

EDTResult EDTFile::Open(const std::vector<uint8_t>& buffer)
{
	pack::PackFile file;
	if (file.Open(buffer) != pack::PackResult::Success)
	{
		return EDTResult::ErrNotABlockFile;
	}
	for (const auto& [name, data] : file.GetBlocks())
	{
		const std::span<const uint8_t> bytes(data);
		if (const auto camera = NumberAfter(name, "Cam"))
		{
			if (bytes.size() < k_CameraSize)
			{
				return EDTResult::ErrCameraTooSmall;
			}
			_cameras[*camera] = {.position = ReadPoint(bytes, 0),
			                     .focus = ReadPoint(bytes, 12),
			                     .unknown = {Read<float>(bytes, 24), Read<float>(bytes, 28)}};
		}
		else if (const auto track = NumberAfter(name, "Track"))
		{
			if (bytes.size() < 4)
			{
				return EDTResult::ErrTrackTooSmall;
			}
			auto position = ReadWay(bytes, 4);
			if (!position)
			{
				return EDTResult::ErrWayMalformed;
			}
			auto focus = ReadWay(bytes, 4 + position->second);
			if (!focus)
			{
				return EDTResult::ErrWayMalformed;
			}
			_tracks[*track] = {
			    .unknown0 = Read<uint32_t>(bytes, 0), .position = std::move(position->first), .focus = std::move(focus->first)};
		}
	}
	return EDTResult::Success;
}
