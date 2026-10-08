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

#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// The original's one map position: MapCoords {int32 x, z; float altitude}, x and z in 16.16 fixed point with a 10 m
/// cell = 65536 (the high word is the cell, the low word the fraction) and altitude the height above the ground.
/// Conversions, the 512 x 512 cell grid, the neighbour tables and the spiral, exactly as the original does them: the
/// game logic runs with the FPU at 24 bits, so every product is a float product.
///
/// The ground height is not computed here: FromWorld / ToWorld ask the island's GetHeightAt.
namespace openblack::map_coords
{

constexpr float k_FixedPerMetre = 6553.6f;           ///< 6553.60009765625 as a float
constexpr float k_MetresPerFixed = 10.0f / 65536.0f; ///< exactly 10 / 65536
constexpr int32_t k_FixedPerCell = 0x10000;          ///< the high word is the cell
constexpr float k_CellSize = 10.0f;                  ///< metres per cell (65536 x k_MetresPerFixed)
/// The map's cells per side, set once when the game starts: both sides 512, so one value
constexpr uint32_t k_MapCells = 512;

/// JustMapXZ: one cell step {int16 x, z}
struct JustMapXZ
{
	int16_t x;
	int16_t z;

	constexpr bool operator==(const JustMapXZ& other) const { return x == other.x && z == other.z; }
};

/// MapCoords: x, z 16.16 fixed, altitude above the ground
struct MapCoords
{
	int32_t x {0};
	int32_t z {0};
	float altitude {0.0f};

	constexpr bool operator==(const MapCoords& other) const = default;

	/// x and z added as integers, the altitude too (b + a, as floats)
	constexpr MapCoords& operator+=(const MapCoords& other)
	{
		x = static_cast<int32_t>(static_cast<uint32_t>(x) + static_cast<uint32_t>(other.x));
		z = static_cast<int32_t>(static_cast<uint32_t>(z) + static_cast<uint32_t>(other.z));
		altitude = other.altitude + altitude;
		return *this;
	}
	/// The same with subtractions
	constexpr MapCoords& operator-=(const MapCoords& other)
	{
		x = static_cast<int32_t>(static_cast<uint32_t>(x) - static_cast<uint32_t>(other.x));
		z = static_cast<int32_t>(static_cast<uint32_t>(z) - static_cast<uint32_t>(other.z));
		altitude = altitude - other.altitude;
		return *this;
	}
	/// A copy, then +=
	[[nodiscard]] constexpr MapCoords operator+(const MapCoords& other) const
	{
		MapCoords sum = *this;
		sum += other;
		return sum;
	}
	/// A copy, then -=
	[[nodiscard]] constexpr MapCoords operator-(const MapCoords& other) const
	{
		MapCoords difference = *this;
		difference -= other;
		return difference;
	}
};

/// Float to int as the original's runtime does it on every SSE2 CPU: towards 0, and the "integer indefinite"
/// INT32_MIN for a NaN or a value out of the int32 range. The x87 path (the low 32 bits of the int64, corrected
/// towards 0) would give another value out of that range; it is not reproduced
/// Kept apart from TruncateToInt because it pins NaN and out-of-range values to INT32_MIN
[[nodiscard]] constexpr int32_t FtoL(float value)
{
	if (!(value > -2147483648.0f && value < 2147483648.0f))
	{
		return static_cast<int32_t>(0x80000000u);
	}
	return static_cast<int32_t>(value);
}

/// Metres -> 16.16: x k_FixedPerMetre, then FtoL (truncated towards 0), as the original's MapCoords setter and its
/// many inline copies
[[nodiscard]] constexpr int32_t ToFixed(float metres)
{
	return FtoL(metres * k_FixedPerMetre);
}

/// 16.16 -> metres: x k_MetresPerFixed. The integer load is exact and the product is rounded once to 24 bits; above
/// 2^24 a float of the integer would round twice, so the exact product is taken in double (fixed * 5 / 32768 needs
/// at most 34 bits) and rounded to float once, as the FPU does
[[nodiscard]] constexpr float ToMetres(int32_t fixed)
{
	return static_cast<float>(static_cast<double>(fixed) * (10.0 / 65536.0));
}

/// The utility functions' own metres -> 16.16: x 65536, / 10, FtoL (the angle offsets, the spiral increment). Not
/// ToFixed: 6553.6f is not 65536 / 10, so a value on a boundary truncates one unit apart
[[nodiscard]] constexpr int32_t ToFixedGUtils(float metres)
{
	return FtoL(metres * 65536.0f / 10.0f);
}

/// The magic hand's land lookup: (m x 65536) x 0.1, truncated toward zero. Neither ToFixed (x 6553.6) nor
/// ToFixedGUtils (/ 10); (inferred) the product rounded to float before the truncation (24-bit FPU)
[[nodiscard]] constexpr int32_t MetresToFixedForHandLookup(float metres)
{
	return FtoL((metres * 65536.0f) * 0.1f);
}

/// A metre value through a MapCoords and back (ToMetres(ToFixed(m))): what a position stored in a MapCoords is. Not
/// idempotent: another round trip may lose one more unit (ToFixed(ToMetres(8090858)) = 8090857), as in the original
[[nodiscard]] constexpr float Quantise(float metres)
{
	return ToMetres(ToFixed(metres));
}

/// The cell of a 16.16 value: the high word read unsigned: a negative value is cell 0xFFFF, off the map
[[nodiscard]] constexpr uint16_t CellOf(int32_t fixed)
{
	return static_cast<uint16_t>(static_cast<uint32_t>(fixed) >> 16u);
}
/// The high word read signed, as a JustMapXZ holds it
[[nodiscard]] constexpr int16_t SignedCellOf(int32_t fixed)
{
	return static_cast<int16_t>(CellOf(fixed));
}
[[nodiscard]] constexpr uint16_t CellX(const MapCoords& coords)
{
	return CellOf(coords.x);
}
[[nodiscard]] constexpr uint16_t CellZ(const MapCoords& coords)
{
	return CellOf(coords.z);
}
/// The cell (CellX, CellZ)
[[nodiscard]] constexpr glm::ivec2 Cell(const MapCoords& coords)
{
	return {CellX(coords), CellZ(coords)};
}
/// The cell of a world point: its MapCoords, then the high words
[[nodiscard]] constexpr glm::ivec2 CellOf(glm::vec2 metres)
{
	return {CellOf(ToFixed(metres.x)), CellOf(ToFixed(metres.y))};
}
[[nodiscard]] constexpr glm::ivec2 CellOf(glm::vec3 point)
{
	return CellOf(glm::vec2(point.x, point.z));
}

/// The unsigned high words against the map's size. `cells` is k_MapCells in the original; openblack's islands may
/// pass their GetCellsPerSide()
[[nodiscard]] constexpr bool InBounds(const MapCoords& coords, uint32_t cells = k_MapCells)
{
	return CellX(coords) < cells && CellZ(coords) < cells;
}
/// The same on a cell: a negative cell (a sign-extended JustMapXZ) is a large unsigned value, off the map
[[nodiscard]] constexpr bool InBounds(glm::ivec2 cell, uint32_t cells = k_MapCells)
{
	return static_cast<uint32_t>(cell.x) < cells && static_cast<uint32_t>(cell.y) < cells;
}
[[nodiscard]] constexpr bool InBounds(glm::vec3 point, uint32_t cells = k_MapCells)
{
	return InBounds(CellOf(point), cells);
}

/// The index cx * k_MapCells + cz of the map cell, -1 (no cell) off the map
[[nodiscard]] constexpr int32_t CellIndex(const MapCoords& coords)
{
	return InBounds(coords) ? static_cast<int32_t>(CellX(coords) * k_MapCells + CellZ(coords)) : -1;
}

/// MapCoords += JustMapXZ: 16-bit adds to the high words; the fractions stay and a carry out of the high word is
/// lost
constexpr void AddCells(MapCoords& coords, JustMapXZ step)
{
	const auto move = [](int32_t value, int16_t d) {
		const auto word = static_cast<uint16_t>(CellOf(value) + static_cast<uint16_t>(d));
		return static_cast<int32_t>((static_cast<uint32_t>(word) << 16u) | (static_cast<uint32_t>(value) & 0xFFFFu));
	};
	coords.x = move(coords.x, step.x);
	coords.z = move(coords.z, step.z);
}

/// The 4 neighbours: +x, +z, -x, -z. The spiral's table; also read without the spiral (a villager's nearest free
/// destination, the wallhug)
constexpr std::array<JustMapXZ, 4> k_Neighbours4 {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
/// The 8 neighbours and the cell itself: anticlockwise from +x, then (0, 0). Another table, not the spiral's
constexpr std::array<JustMapXZ, 9> k_Neighbours8 {
    {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {0, 0}}};

/// The spiral (dir, count): every caller starts with dir = count = 1, does the centre cell first and then steps by
/// Next() (MapCoords += JustMapXZ), checking InBounds on each cell. The steps from the start: (-1, 0), (0, -1),
/// (+1, 0) x 2, (0, +1) x 2, (-1, 0) x 3, ...: a square growing outwards. The number of cells is the caller's own
/// (CellSpiralSize, or its inline ceil(...)^2)
struct Spiral
{
	int32_t dir {1};
	int32_t count {1};

	/// --count; at 0: ++dir, count = dir / 2 (towards 0); then the step table[dir & 3]: the state is updated before the
	/// step is read
	constexpr const JustMapXZ& Next()
	{
		if (--count == 0)
		{
			++dir;
			count = dir / 2;
		}
		return k_Neighbours4[static_cast<size_t>(dir & 3)];
	}
};

/// The spiral increment, only used to find a town's clear area: the spiral's rule, then each axis moves `step` metres
/// and is truncated again with the utility formula: x = (table.x * step + x * 10 * (1 / 65536)) * 65536 / 10,
/// truncated toward zero.
/// x * 10 then * 1/65536 is ToMetres (a power of 2 does not change the rounding)
constexpr void SpiralIncrement(MapCoords& coords, Spiral& spiral, float step)
{
	const auto& d = spiral.Next();
	coords.x = ToFixedGUtils(static_cast<float>(d.x) * step + ToMetres(coords.x));
	coords.z = ToFixedGUtils(static_cast<float>(d.z) * step + ToMetres(coords.z));
}

/// n = r * 0.2 truncated toward zero; an unsigned compare with 1 (only 0 becomes 1); n^2
[[nodiscard]] constexpr int32_t CellSpiralSize(float radius)
{
	auto n = FtoL(radius * 0.2f);
	if (static_cast<uint32_t>(n) < 1u)
	{
		n = 1;
	}
	return n * n;
}

/// n = r * -2 / step truncated toward zero; (1 - n)^2
[[nodiscard]] constexpr int32_t IncrementSpiralSize(float radius, float step)
{
	const auto n = FtoL(radius * -2.0f / step);
	return (1 - n) * (1 - n);
}

/// x, z = ToFixed; altitude = y - the ground height at the truncated position. Without an island the ground is 0
[[nodiscard]] MapCoords FromWorld(const LandIslandInterface* island, glm::vec3 point);
/// x, z = ToMetres; y = the ground height + altitude
[[nodiscard]] glm::vec3 ToWorld(const LandIslandInterface* island, const MapCoords& coords);
/// The same with the Locator's island
[[nodiscard]] MapCoords FromWorld(glm::vec3 point);
[[nodiscard]] glm::vec3 ToWorld(const MapCoords& coords);

/// MapCoords without its altitude (x, z only), for the 2D callers
[[nodiscard]] constexpr MapCoords FromMetres(glm::vec2 metres)
{
	return {ToFixed(metres.x), ToFixed(metres.y), 0.0f};
}
[[nodiscard]] constexpr glm::vec2 ToMetres(const MapCoords& coords)
{
	return {ToMetres(coords.x), ToMetres(coords.z)};
}

} // namespace openblack::map_coords
