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
#include <bit>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// The game's shared distance routines: every object asks the same ones, so they are kept bit for bit.
///
/// Every length goes through one approximate inverse square root taken from a 1024-entry table built once at start-up:
/// the error is at most 0.097 %, and the integer Hypotenuse then truncates, so the result is NOT sqrt(dx^2 + dz^2). All
/// the game's distances are 2D (x, z); the y is never used.
///
/// The game logic runs with the FPU at 24 bits, so every operation rounds to a float: these routines are written in
/// float on purpose, no double and no FMA. The one exception is ConvertWholeDistanceToMeters, which multiplies the
/// exact integer before rounding.
///
/// Routines that look alike but are NOT the same (do not swap one for another):
/// - Hypotenuse(int32, int32) (16.16, truncated, no cut at zero) vs Hypotenuse(float, float) (metres, 0 when both
///   sides are within 1e-4, not truncated).
/// - GetDistanceInMetres (through the 16.16 hypotenuse, so quantised to 1/65536 of a cell) vs GetDistance(vec3)
///   (float metres straight away).
/// - GetMetresDistanceSq: the exact square, with no table at all.
/// - FastDistance (max + min / 2) and ChebyshevDistance (max): not euclidean lengths.
/// - SigmoidThreshold (a is the threshold) vs CreatureSigmoidThreshold (b <= 0 gives 0).
/// - GetDistanceModifier (a = 0.5) vs DistanceChangeToBelief (a = -0.9, b = -x / y).
namespace openblack::gutils
{

namespace detail
{
/// SigmoidThreshold's static table (41 entries of stored data, never written). It is a logistic
/// 1 / (1 + exp(-1.0232 (i - 20))) (inferred: fitted), but it is stored data, so the bits are copied as they are:
/// entry 0 is an exact 0 and 37..40 an exact 1
constexpr std::array<uint32_t, 41> k_SigmoidBits {
    0x00000000, 0x317763DF, 0x322BCC77, 0x32F084A7, 0x33A71301, 0x34684017, 0x35218B62, 0x35E0AE34, 0x369C419B,
    0x375955DE, 0x38172465, 0x38D235BD, 0x39922A17, 0x3A4B32F9, 0x3B0D1EB3, 0x3BC38892, 0x3C868D9B, 0x3D35D41D,
    0x3DEA5E18, 0x3E8762A1, 0x3F000000, 0x3F3C4EB0, 0x3F62B43D, 0x3F74A2BE, 0x3F7BCB93, 0x3F7E78EF, 0x3F7F72E1,
    0x3F7FCD33, 0x3F7FEDBB, 0x3F7FF96E, 0x3F7FFDA3, 0x3F7FFF27, 0x3F7FFFB2, 0x3F7FFFE4, 0x3F7FFFF6, 0x3F7FFFFC,
    0x3F7FFFFF, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000};
} // namespace detail

/// The 41 table entries as floats
constexpr std::array<float, 41> k_Sigmoid = [] {
	std::array<float, 41> table {};
	for (size_t i = 0; i < table.size(); ++i)
	{
		table[i] = std::bit_cast<float>(detail::k_SigmoidBits[i]);
	}
	return table;
}();

/// The cut of Hypotenuse(float, float)
constexpr float k_HypotenuseEpsilon = 1e-4f;
/// Metres per cell, the unit ConvertWholeDistanceToMeters works in
constexpr float k_MetresPerCell = 10.0f;

/// The inverse square root table, built once at start-up (right after the FPU is set to 24 bits). Entry i keeps the top
/// 10 mantissa bits (& 0x7FE000) of 1 / sqrt(f), with f the float 0x3F000000 | i << 14 (so the exponent's low bit and 9
/// mantissa bits); an exact 1 is stored as 0x7FE000
[[nodiscard]] const std::array<uint32_t, 1024>& InvSqrtTable();

/// Approximate 1 / sqrt: exponent = ((0xBE000000 - (bits & 0x7F800000)) >> 1) & 0x7F800000, mantissa = the table at
/// (bits >> 14) & 0x3FF (truncated, not rounded; the sign is never looked at). 0 gives about 2^63, so 1 / InvSqrt(0)
/// comes out as 0 on its own
[[nodiscard]] float InvSqrt(float value);

/// 16.16 in, 16.16 out (0x10000 = one cell = 10 m). x = dx * 2^-16, s = float(z * z + x * x), then
/// 65536.0 / InvSqrt(s), truncated toward zero, with no cut at zero
[[nodiscard]] int32_t Hypotenuse(int32_t dx, int32_t dz);

/// 0 when |a| and |b| are both <= 1e-4 (or NaN: the compare is unordered); else 1 / InvSqrt(float(a*a + b*b))
[[nodiscard]] float Hypotenuse(float a, float b);

/// 10 x 2^-16 x whole. The integer is exact before the rounding, so above 2^24 units (2560 m) this is not
/// float(whole) * c
[[nodiscard]] constexpr float ConvertWholeDistanceToMeters(int32_t whole)
{
	return map_coords::ToMetres(whole); // the same constant and the same single rounding
}

/// m / 10 * 65536, truncated toward zero. 65536 is a power of two, so this is map_coords::ToFixedGUtils, which scales
/// first
[[nodiscard]] constexpr int32_t ConvertMetersToWholeDistance(float metres)
{
	return map_coords::FtoL(metres / k_MetresPerCell * 65536.0f);
}

/// Hypotenuse(b.x - a.x, b.z - a.z) in 16.16, the altitude ignored
[[nodiscard]] int32_t GetDistance(const map_coords::MapCoords& a, const map_coords::MapCoords& b);

/// The same towards the centre of a map cell, (short(cell) << 16) + 0x8000
[[nodiscard]] int32_t GetDistanceToCell(const map_coords::MapCoords& a, map_coords::JustMapXZ cell);

/// ConvertWholeDistanceToMeters(GetDistance(a, b)): the distance most of the game uses
[[nodiscard]] float GetDistanceInMetres(const map_coords::MapCoords& a, const map_coords::MapCoords& b);
/// The same for two world points, which the original holds as map coordinates (so x and z are truncated to 16.16
/// first). (approximate) openblack keeps the positions in float metres
[[nodiscard]] float GetDistanceInMetres(glm::vec3 a, glm::vec3 b);
/// The same for two (x, z) pairs in metres
[[nodiscard]] float GetDistanceInMetres(glm::vec2 a, glm::vec2 b);
/// The same for two MapCoords kept as an ivec2 (x, z): careful, this one takes 16.16 units and the vec2 one metres
[[nodiscard]] float GetDistanceInMetres(glm::ivec2 a, glm::ivec2 b);

/// ConvertWholeDistanceToMeters(GetDistanceToCell(a, cell))
[[nodiscard]] float GetDistanceInMetresToCell(const map_coords::MapCoords& a, map_coords::JustMapXZ cell);

/// The x / z differences stored as floats and then Hypotenuse(float, float). Metres all the way, without map
/// coordinates
[[nodiscard]] float GetDistance(glm::vec3 a, glm::vec3 b);

/// ConvertWholeDistanceToMeters of each difference, then mz * mz + mx * mx. The exact square in float: no table, so it
/// is not GetDistanceInMetres squared
[[nodiscard]] float GetMetresDistanceSq(const map_coords::MapCoords& a, const map_coords::MapCoords& b);

/// |dx|, |dz|, then max + (min >> 1) (arithmetic shift). Whole map units, not a euclidean length
[[nodiscard]] constexpr int32_t FastDistance(const map_coords::MapCoords& a, const map_coords::MapCoords& b)
{
	const auto abs32 = [](int32_t v) { return v < 0 ? -v : v; };
	const int32_t dx = abs32(b.x - a.x);
	const int32_t dz = abs32(b.z - a.z);
	// signed compare: the larger one, plus half the other
	return dx >= dz ? (dz >> 1) + dx : (dx >> 1) + dz;
}

/// max(|dx|, |dz|), the two compared unsigned
[[nodiscard]] constexpr int32_t ChebyshevDistance(const map_coords::MapCoords& a, const map_coords::MapCoords& b)
{
	const auto abs32 = [](int32_t v) { return v < 0 ? -v : v; };
	const int32_t dx = abs32(a.x - b.x);
	const int32_t dz = abs32(a.z - b.z);
	return static_cast<uint32_t>(dx) <= static_cast<uint32_t>(dz) ? dz : dx;
}

/// The chance that b passes the threshold a, in 41 steps. a == 1 gives 0 (a NaN too); else
/// v = clamp(clamp(b, -1, 1) - a, -1, 1) and the answer is k_Sigmoid[min(unsigned)((v + 1) * 20.5 truncated toward zero, 40)].
/// Careful: the threshold is the FIRST argument
[[nodiscard]] float SigmoidThreshold(float a, float b);

/// SigmoidThreshold(0.5, 1 - min(d, max) / max). It falls off with the distance, from k_Sigmoid[30] = 0.99996 at d = 0
/// to k_Sigmoid[10] = 3.6e-5 at d >= max (21 of the 41 steps). max = 0 makes 0 / 0 and gives k_Sigmoid[0] = 0
[[nodiscard]] float GetDistanceModifier(float distance, float maximum);

/// The belief change with distance: SigmoidThreshold(-0.9, float(-(x / y))). Another curve over the same table
[[nodiscard]] float DistanceChangeToBelief(float x, float y);

/// The creature's variant: b <= 0 gives 0, else SigmoidThreshold
[[nodiscard]] float CreatureSigmoidThreshold(float a, float b);

} // namespace openblack::gutils
