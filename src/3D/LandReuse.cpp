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

std::vector<uint8_t> WriteAltitudes(std::vector<CellAltitude> altitudes)
{
	// A corner on the edge of a block is in each block that keeps a copy of it, with the same altitude
	const auto place = [](const CellAltitude& corner) { return std::pair {corner.x, corner.z}; };
	std::ranges::sort(altitudes, {}, place);
	const auto copies = std::ranges::unique(altitudes, {}, place);
	altitudes.erase(copies.begin(), copies.end());
	std::vector<uint8_t> bytes;
	bytes.reserve(4 + altitudes.size() * 5);
	const auto put = [&bytes](uint32_t value, size_t size) {
		for (size_t i = 0; i < size; ++i)
		{
			bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
		}
	};
	put(static_cast<uint32_t>(altitudes.size()), 4);
	for (const auto& corner : altitudes)
	{
		put(corner.x, 2);
		put(corner.z, 2);
		put(corner.altitude, 1);
	}
	return bytes;
}

std::optional<std::vector<CellAltitude>> ReadAltitudes(std::span<const uint8_t> bytes)
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
	std::vector<CellAltitude> altitudes;
	altitudes.reserve(count);
	for (size_t at = 4; at < bytes.size(); at += 5)
	{
		altitudes.push_back({.x = static_cast<uint16_t>(get(at, 2)),
		                     .z = static_cast<uint16_t>(get(at + 2, 2)),
		                     .altitude = static_cast<uint8_t>(get(at + 4, 1))});
	}
	return altitudes;
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

} // namespace openblack::land_reuse
