/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OnlineMapThumbnail.h"

#include <algorithm>
#include <utility>

namespace
{
constexpr size_t k_PlaceSize = 9;

uint32_t ReadU32(std::span<const std::byte> bytes, size_t at)
{
	uint32_t value = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		value |= static_cast<uint32_t>(bytes[at + i]) << (8 * i);
	}
	return value;
}

/// Reads the bytes one part at a time, and finds when a part goes past the end
class Reader
{
public:
	explicit Reader(std::span<const std::byte> bytes)
	    : _bytes(bytes)
	{
	}

	[[nodiscard]] bool Has(size_t count) const { return _bytes.size() - _at >= count; }

	std::span<const std::byte> Take(size_t count)
	{
		const auto part = _bytes.subspan(_at, count);
		_at += count;
		return part;
	}

	uint32_t TakeU32() { return ReadU32(Take(4), 0); }

private:
	std::span<const std::byte> _bytes;
	size_t _at {0};
};

std::optional<std::vector<openblack::onlinemap::ThumbnailPlace>> TakePlaces(Reader& reader, uint32_t count)
{
	if (!reader.Has(static_cast<size_t>(count) * k_PlaceSize))
	{
		return std::nullopt;
	}
	std::vector<openblack::onlinemap::ThumbnailPlace> places;
	places.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto place = reader.Take(k_PlaceSize);
		places.push_back({.x = static_cast<int32_t>(ReadU32(place, 0)),
		                  .y = static_cast<int32_t>(ReadU32(place, 4)),
		                  .owner = static_cast<int8_t>(place[8])});
	}
	return places;
}
} // namespace

std::optional<openblack::onlinemap::ThumbnailFile> openblack::onlinemap::ParseThumbnail(std::span<const std::byte> bytes)
{
	Reader reader(bytes);
	if (!reader.Has(k_ThumbnailMagic.size()))
	{
		return std::nullopt;
	}
	const auto magic = reader.Take(k_ThumbnailMagic.size());
	if (!std::ranges::equal(magic, k_ThumbnailMagic, [](std::byte b, char c) { return static_cast<char>(b) == c; }))
	{
		return std::nullopt;
	}
	if (!reader.Has(16))
	{
		return std::nullopt;
	}
	// The first number is not part of the thumbnail
	reader.TakeU32();
	const auto citadelCount = reader.TakeU32();
	const auto townCount = reader.TakeU32();
	const auto descriptionLength = reader.TakeU32();
	if (!reader.Has(descriptionLength))
	{
		return std::nullopt;
	}
	const auto description = reader.Take(descriptionLength);
	auto citadels = TakePlaces(reader, citadelCount);
	if (!citadels.has_value())
	{
		return std::nullopt;
	}
	auto towns = TakePlaces(reader, townCount);
	if (!towns.has_value() || !reader.Has(4))
	{
		return std::nullopt;
	}
	const auto pictureLength = reader.TakeU32();
	if (!reader.Has(pictureLength))
	{
		return std::nullopt;
	}
	return ThumbnailFile {
	    .description = std::string_view(reinterpret_cast<const char*>(description.data()), description.size()),
	    .citadels = std::move(*citadels),
	    .towns = std::move(*towns),
	    .picture = reader.Take(pictureLength),
	};
}
