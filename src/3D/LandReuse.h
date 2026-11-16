/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <WorldFile.h>

namespace openblack::lnd
{
struct LNDCell;
}

/// The rules that let a load of a world state keep the land that is open, in place of a land that it builds again
namespace openblack::land_reuse
{

/// The land that is open and the land of the world that loads
struct Lands
{
	/// The load brings the world state of a network game, which a client loads when it comes back in step
	bool worldState {false};
	/// A land is open now
	bool landLoaded {false};
	std::filesystem::path openLand;
	/// The landscape file that the open land read
	std::filesystem::path openLandscape;
	std::filesystem::path savedLand;
	std::filesystem::path savedLandscape;
	/// The game builds the saved land itself (the testbed, the map for 32 gods), so no landscape file names it
	bool builtByGame {false};
};

/// Whether the load can keep the open land. A world state of the land that is open has the same landscape: only the
/// altitudes that the world changed go back, and the load makes again only the blocks that change.
[[nodiscard]] bool KeepsLand(const Lands& lands);

/// The cells of a block, [x * 17 + z], whose altitude is not the altitude of the land as the game built it
[[nodiscard]] std::vector<uint16_t> ChangedCells(std::span<const lnd::LNDCell> cells, std::span<const uint8_t> built);

/// The altitude of a cell corner of the map, which is not the altitude of the land as the game built it
struct CellAltitude
{
	/// The corner's map cell
	uint16_t x {0};
	uint16_t z {0};
	uint8_t altitude {0};

	bool operator==(const CellAltitude&) const = default;
};

/// The country of a cell corner of the map, which is not the country of the land as the game built it
struct CellCountry
{
	/// The corner's map cell
	uint16_t x {0};
	uint16_t z {0};
	uint8_t country {0};

	bool operator==(const CellCountry&) const = default;
};

/// The corners of a block that changed, by their map cells: the block's first cell plus each cell [x * 17 + z]
[[nodiscard]] std::vector<CellAltitude> CornersOfBlock(uint16_t firstX, uint16_t firstZ, std::span<const uint16_t> changed,
                                                       std::span<const lnd::LNDCell> cells);

/// The changed altitudes as a world state keeps them: their count, then the x, z and altitude of each, in the order of
/// the map cells and once for each corner
[[nodiscard]] std::vector<uint8_t> WriteAltitudes(std::vector<CellAltitude> altitudes);

/// The changed altitudes that WriteAltitudes wrote; nothing when the bytes are not whole
[[nodiscard]] std::optional<std::vector<CellAltitude>> ReadAltitudes(std::span<const uint8_t> bytes);

/// The block that keeps the changed altitudes. A saved game of openblack and a world state keep the same block.
[[nodiscard]] gamesave::Chunk AltitudesChunk(std::vector<CellAltitude> altitudes);

/// The changed altitudes that a saved game or a world state keeps. A file of an older build has no block, which gives
/// no altitudes. A block that is not whole gives nothing.
[[nodiscard]] std::optional<std::vector<CellAltitude>> KeptAltitudes(const gamesave::WorldFile& world);

/// The cells of a block, [x * 17 + z], whose country is not the country of the land as the game built it
[[nodiscard]] std::vector<uint16_t> ChangedCountryCells(std::span<const lnd::LNDCell> cells, std::span<const uint8_t> built);

/// The countries of the corners of a block that changed, by their map cells, as CornersOfBlock gives the altitudes
[[nodiscard]] std::vector<CellCountry> CountriesOfBlock(uint16_t firstX, uint16_t firstZ, std::span<const uint16_t> changed,
                                                        std::span<const lnd::LNDCell> cells);

/// The changed countries as a world state keeps them: their count, then the x, z and country of each, in the order of the
/// map cells and once for each corner
[[nodiscard]] std::vector<uint8_t> WriteCountries(std::vector<CellCountry> countries);

/// The changed countries that WriteCountries wrote; nothing when the bytes are not whole
[[nodiscard]] std::optional<std::vector<CellCountry>> ReadCountries(std::span<const uint8_t> bytes);

/// The block that keeps the changed countries
[[nodiscard]] gamesave::Chunk CountriesChunk(std::vector<CellCountry> countries);

/// The changed countries that a saved game or a world state keeps. A file of an older build has no block, which gives no
/// countries. A block that is not whole gives nothing.
[[nodiscard]] std::optional<std::vector<CellCountry>> KeptCountries(const gamesave::WorldFile& world);

/// The part of the digest of a land that one cell corner of the map gives: a mix of its place, its altitude and its
/// country. The digest of a land is the parts of all its corners, joined by exclusive or. Thus a change of one corner
/// changes the digest with no new walk over the land.
[[nodiscard]] uint64_t CornerDigest(uint16_t x, uint16_t z, uint8_t altitude, uint8_t country) noexcept;

} // namespace openblack::land_reuse
