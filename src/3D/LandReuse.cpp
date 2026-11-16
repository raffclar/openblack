/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandReuse.h"

#include <algorithm>
#include <string>
#include <utility>

#include <LNDFile.h>

namespace openblack::land_reuse
{

namespace
{
/// The path with each back slash as a forward slash. The paths of the game use back slashes, and a path on Windows can
/// use both. Only Windows reads a back slash as a separator, so the check does not use the separators of the system.
std::string SamePath(const std::filesystem::path& path)
{
	auto text = path.generic_string();
	std::ranges::replace(text, '\\', '/');
	return text;
}
} // namespace

bool KeepsLand(const Lands& lands)
{
	if (!lands.worldState || !lands.landLoaded || lands.savedLand.empty())
	{
		return false;
	}
	if (SamePath(lands.openLand) != SamePath(lands.savedLand))
	{
		return false;
	}
	// A land of a file can read another landscape: the landscape must be the same too
	return lands.builtByGame || SamePath(lands.openLandscape) == SamePath(lands.savedLandscape);
}

std::vector<uint16_t> ChangedCells(std::span<const lnd::LNDCell> cells, std::span<const uint8_t> built)
{
	std::vector<uint16_t> changed;
	const auto count = std::min(cells.size(), built.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (cells[i].altitude != built[i])
		{
			changed.push_back(static_cast<uint16_t>(i));
		}
	}
	return changed;
}

std::vector<CellAltitude> CornersOfBlock(uint16_t firstX, uint16_t firstZ, std::span<const uint16_t> changed,
                                         std::span<const lnd::LNDCell> cells)
{
	constexpr uint16_t k_RowCells = 17;
	std::vector<CellAltitude> corners;
	corners.reserve(changed.size());
	for (const auto index : changed)
	{
		corners.push_back({.x = static_cast<uint16_t>(firstX + index / k_RowCells),
		                   .z = static_cast<uint16_t>(firstZ + index % k_RowCells),
		                   .altitude = cells[index].altitude});
	}
	return corners;
}

namespace
{

/// The corners with one value each, as a world state keeps them: their count, then the x, z and value of each, in the
/// order of the map cells and once for each corner. A corner on the edge of a block is in each block that keeps a copy
/// of it, with the same value.
template <typename Corner, auto Value>
std::vector<uint8_t> WriteCorners(std::vector<Corner> corners)
{
	const auto place = [](const Corner& corner) { return std::pair {corner.x, corner.z}; };
	std::ranges::sort(corners, {}, place);
	const auto copies = std::ranges::unique(corners, {}, place);
	corners.erase(copies.begin(), copies.end());
	std::vector<uint8_t> bytes;
	bytes.reserve(4 + corners.size() * 5);
	const auto put = [&bytes](uint32_t value, size_t size) {
		for (size_t i = 0; i < size; ++i)
		{
			bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
		}
	};
	put(static_cast<uint32_t>(corners.size()), 4);
	for (const auto& corner : corners)
	{
		put(corner.x, 2);
		put(corner.z, 2);
		put(corner.*Value, 1);
	}
	return bytes;
}

/// The corners that WriteCorners wrote; nothing when the bytes are not whole
template <typename Corner, auto Value>
std::optional<std::vector<Corner>> ReadCorners(std::span<const uint8_t> bytes)
{
	const auto get = [&bytes](size_t at, size_t size) {
		uint32_t value = 0;
		for (size_t i = 0; i < size; ++i)
		{
			value |= static_cast<uint32_t>(bytes[at + i]) << (8 * i);
		}
		return value;
	};
	if (bytes.size() < 4)
	{
		return std::nullopt;
	}
	const auto count = get(0, 4);
	if (bytes.size() != 4 + static_cast<size_t>(count) * 5)
	{
		return std::nullopt;
	}
	std::vector<Corner> corners;
	corners.reserve(count);
	for (size_t at = 4; at < bytes.size(); at += 5)
	{
		Corner corner {.x = static_cast<uint16_t>(get(at, 2)), .z = static_cast<uint16_t>(get(at + 2, 2))};
		corner.*Value = static_cast<uint8_t>(get(at + 4, 1));
		corners.push_back(corner);
	}
	return corners;
}

} // namespace

std::vector<uint8_t> WriteAltitudes(std::vector<CellAltitude> altitudes)
{
	return WriteCorners<CellAltitude, &CellAltitude::altitude>(std::move(altitudes));
}

std::optional<std::vector<CellAltitude>> ReadAltitudes(std::span<const uint8_t> bytes)
{
	return ReadCorners<CellAltitude, &CellAltitude::altitude>(bytes);
}

gamesave::Chunk AltitudesChunk(std::vector<CellAltitude> altitudes)
{
	return {.tag = gamesave::k_AltitudesTag, .bytes = WriteAltitudes(std::move(altitudes))};
}

std::optional<std::vector<CellAltitude>> KeptAltitudes(const gamesave::WorldFile& world)
{
	const auto bytes = gamesave::FindChunk(world, gamesave::k_AltitudesTag);
	if (!bytes.has_value())
	{
		return std::vector<CellAltitude> {};
	}
	return ReadAltitudes(*bytes);
}

std::vector<uint16_t> ChangedCountryCells(std::span<const lnd::LNDCell> cells, std::span<const uint8_t> built)
{
	std::vector<uint16_t> changed;
	const auto count = std::min(cells.size(), built.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (cells[i].properties.country != built[i])
		{
			changed.push_back(static_cast<uint16_t>(i));
		}
	}
	return changed;
}

std::vector<CellCountry> CountriesOfBlock(uint16_t firstX, uint16_t firstZ, std::span<const uint16_t> changed,
                                          std::span<const lnd::LNDCell> cells)
{
	constexpr uint16_t k_RowCells = 17;
	std::vector<CellCountry> corners;
	corners.reserve(changed.size());
	for (const auto index : changed)
	{
		corners.push_back({.x = static_cast<uint16_t>(firstX + index / k_RowCells),
		                   .z = static_cast<uint16_t>(firstZ + index % k_RowCells),
		                   .country = static_cast<uint8_t>(cells[index].properties.country)});
	}
	return corners;
}

std::vector<uint8_t> WriteCountries(std::vector<CellCountry> countries)
{
	return WriteCorners<CellCountry, &CellCountry::country>(std::move(countries));
}

std::optional<std::vector<CellCountry>> ReadCountries(std::span<const uint8_t> bytes)
{
	return ReadCorners<CellCountry, &CellCountry::country>(bytes);
}

gamesave::Chunk CountriesChunk(std::vector<CellCountry> countries)
{
	return {.tag = gamesave::k_CountriesTag, .bytes = WriteCountries(std::move(countries))};
}

std::optional<std::vector<CellCountry>> KeptCountries(const gamesave::WorldFile& world)
{
	const auto bytes = gamesave::FindChunk(world, gamesave::k_CountriesTag);
	if (!bytes.has_value())
	{
		return std::vector<CellCountry> {};
	}
	return ReadCountries(*bytes);
}

uint64_t CornerDigest(uint16_t x, uint16_t z, uint8_t altitude, uint8_t country) noexcept
{
	// The mix of SplitMix64 spreads each bit of the place and of the values over the whole digest
	auto value = static_cast<uint64_t>(x) | (static_cast<uint64_t>(z) << 16) | (static_cast<uint64_t>(altitude) << 32) |
	             (static_cast<uint64_t>(country) << 40);
	value += 0x9e3779b97f4a7c15ULL;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31);
}

} // namespace openblack::land_reuse
