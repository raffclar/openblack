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

#include "3D/MapCoords.h"

/// The original's angle utilities: LHArcTan on its table, the COS / SIN tables, the conversions, the routines that turn
/// an angle into a position, and the difference / direction of two angles.
///
/// - A game angle is an 11-bit integer, 2048 to the circle, kept as a u16 (the wall-hug walk's heading): 0 = +x,
///   0x200 = +z, 0x400 = -x, 0x600 = -z. It is atan2(dz, dx) in 2048ths, through the arctangent table (at most 2.27
///   steps off).
/// - A 3D angle is float radians, the same way round (0 = +x, growing towards +z), in [0, 2 pi) out of these routines.
/// - A "Scawen" angle is the 3D angle + pi / 2 (the creature, the ball, the dying dove, the facing direction).
///
/// No game-angle routine uses floating point but the conversions: LHArcTan is integer only (a shift and an unsigned
/// divide). The FPU runs at 24 bits (see GUtilsDistance.h), but its sine / cosine are not rounded by the precision
/// control: their result is extended and is rounded to a float once, by the product after them. So the routines below
/// that take a sine or cosine take it in double and round the product once; everything else is float or integer, no
/// FMA.
///
/// Not ported (no caller in the original): the angle between two points and the turn towards a target by a maximum
/// step; the "octagonal" unit step has one caller, not ported either. Not this family: the 3D library's Y angle, the
/// gestures' positive atan2, the puzzle game's own conversion.
namespace openblack::gutils
{

/// 2048 game angles to the circle
constexpr int32_t k_GameAngleCircle = 0x800;
/// The game angle mask
constexpr int32_t k_GameAngleMask = 0x7FF;
/// 2 pi (float) / 2048
constexpr float k_GameAngleTo3D = 0.0030679617f;
/// 2048 / 2 pi
constexpr float k_Angle3DToGame = 325.94931f;
/// pi (float) / 2048
constexpr float k_HalfGameAngleTo3D = 0.0015339808f;
/// pi / 2, the Scawen offset
constexpr float k_ScawenOffset = 1.5707964f;

/// The arctangent table: 257 u16, T[i] = trunc(atan(i / 256) x 1024 / pi), 0..256 (static data in the original; built
/// here in double, which gives every entry: 0 differences with trunc, 122 with round). Only LHArcTan reads it
[[nodiscard]] const std::array<uint16_t, 257>& ArcTanTable();
/// The SIN table: 2560 i32 (a circle and a quarter), S[i] = trunc(65536 sin(i 2 pi / 2048)) (static data; built here
/// in double, which gives every entry). COS is the same table 512 entries on: C[i] = S[i + 512]
[[nodiscard]] const std::array<int32_t, 2560>& SinTable();
/// 65536 cos. The original indexes with a & 0xFFFF (its callers pass 0..0x7FF; 2048 and up read past the table); here
/// a & 0x7FF (inferred: the same for every value the game passes)
[[nodiscard]] int32_t Cos(uint16_t angle);
/// 65536 sin, a & 0x7FF as Cos
[[nodiscard]] int32_t Sin(uint16_t angle);

/// LHArcTan(dx, dz): x = -dx, z = dz; 0 when both are 0; then the octant rules on t(n, d) = T[(uint32)(n << 8) / d]
/// (unsigned divide), the compares signed, a tie |dx| == |dz| to the first branch, and & 0x7FF. `n << 8` keeps the low
/// 32 bits, as the original's shift does
[[nodiscard]] uint16_t LHArcTan(int32_t dx, int32_t dz);
/// LHArcTan(dx, dz) & 0xFFFF
[[nodiscard]] uint16_t GetAngleFromDXDZ(int32_t dx, int32_t dz);
/// GetAngleFromDXDZ(to.x - from.x, to.z - from.z). The original also has the same with the four integers (from.x,
/// from.z, to.x, to.z)
[[nodiscard]] uint16_t GetAngleFromXZ(const map_coords::MapCoords& from, const map_coords::MapCoords& to);
/// The same on two MapCoords kept as an ivec2 (x, z)
[[nodiscard]] uint16_t GetAngleFromXZ(glm::ivec2 from, glm::ivec2 to);
/// The same on two (x, z) points in metres: each one a MapCoords first (map_coords::FromMetres), then the difference.
/// Not the angle of the metre difference: ToFixed(b) - ToFixed(a) may be one unit off ToFixed(b - a)
[[nodiscard]] uint16_t GetAngleFromXZ(glm::vec2 from, glm::vec2 to);
/// ConvertGameAngleTo3D(GetAngleFromDXDZ(to - from)). NOT atan2 in float: the angle is quantised to 2048 steps, with
/// the table's error
[[nodiscard]] float Get3DAngleFromXZ(const map_coords::MapCoords& from, const map_coords::MapCoords& to);
[[nodiscard]] float Get3DAngleFromXZ(glm::ivec2 from, glm::ivec2 to);
[[nodiscard]] float Get3DAngleFromXZ(glm::vec2 from, glm::vec2 to);

/// (r x 325.94931) & 0x7FF. Truncated towards 0, and a negative value wraps through the mask (-0.5 rad -> -162 ->
/// 1886)
[[nodiscard]] uint32_t ConvertAngle3DToGame(float radians);
/// (a & 0x7FF) (exact) x 0.0030679617, one rounding. Bit for bit float(a) x 2 pi (float) / 2048
[[nodiscard]] float ConvertGameAngleTo3D(int32_t angle);
/// ConvertAngle3DToGame(float(r - pi / 2))
[[nodiscard]] uint32_t ConvertScawenAngleToGameAngle(float radians);
/// float(float((a & 0xFFFF) << 1) x 0.0015339808) + pi / 2 (no & 0x7FF)
[[nodiscard]] float ConvertGameAngleToScawenAngle(uint16_t angle);

/// (COS[a] x d) >> 16 / (SIN[a] x d) >> 16 (32-bit product, arithmetic shift: towards -infinity). Only the octagonal
/// unit step calls them
[[nodiscard]] int32_t GetXByAngle(uint16_t angle, int32_t distance);
[[nodiscard]] int32_t GetZByAngle(uint16_t angle, int32_t distance);
/// float(float(COS[a]) x d) x 2^-16 (the table value exact, two products)
[[nodiscard]] float GetXByAngle(uint16_t angle, float distance);
[[nodiscard]] float GetZByAngle(uint16_t angle, float distance);
/// ((whole >> 4) x COS[a]) >> 12 and the same with SIN, both shifts arithmetic: the wall-hug walk's step. (x, z)
[[nodiscard]] glm::ivec2 StepFromAngle(uint16_t angle, int32_t whole);
/// ((whole >> 8) x COS[a]) >> 8, the same with SIN (arithmetic shifts). (x, z)
[[nodiscard]] glm::ivec2 StepFromAngleCoarse(uint16_t angle, int32_t whole);
/// float(COS[a]) x float(m / 10), truncated toward zero
[[nodiscard]] int32_t GetXByAngleMetersDistance(uint16_t angle, float metres);
[[nodiscard]] int32_t GetZByAngleMetersDistance(uint16_t angle, float metres);
/// {StepFromAngle(a, whole).x, .z, 0}
[[nodiscard]] map_coords::MapCoords GetPosFromGameAngle(uint16_t angle, int32_t whole);
/// The same with whole = ConvertMetersToWholeDistance(m) (m / 10 x 65536, truncated toward zero). The `>> 4` drops the low 4
/// bits of the distance
[[nodiscard]] map_coords::MapCoords GetPosFromGameAngle(uint16_t angle, float metres);

/// x = float(cos(a) m) x 65536 / 10 truncated toward zero, z the same with sin, altitude 0. The sine / cosine are extended and
/// the product with m rounds once, so the cosine is taken in double here (std::cos in float rounds twice: 0.19 % of the values
/// one unit off). The original computes a distance from the origin on the way and throws it away
[[nodiscard]] map_coords::MapCoords GetPosFromAngle(float radians, float metres);
/// p.x = (float(cos(a) m) + float(p.x x 10) x 2^-16) x 65536 / 10 truncated toward zero, the same on z with sin; the altitude
/// stays. The cosine in double as GetPosFromAngle
void AddDistanceFromAngle(map_coords::MapCoords& pos, float radians, float metres);
/// (float(cos(a) m), 0, float(sin(a) m)), the cosine in double
[[nodiscard]] glm::vec3 GetPointFromAngle(float radians, float metres);

/// (inferred name) d = |a - b| (integers, no mask), 0x800 - d when d > 0x400 (unsigned), so 0..0x400 for two angles in
/// 0..0x7FF
[[nodiscard]] uint32_t GetAngleDifference(int32_t a, int32_t b);
/// (inferred name) d = to - from; 0 -> 0; when |d| > 0x400 (unsigned) d wraps by 0x800 towards 0; then -1 if d < 0,
/// else +1. With |d| == 0x400 it does not wrap: +0x400 gives +1, -0x400 gives -1
[[nodiscard]] int32_t GetAngleSign(int32_t from, int32_t to);

} // namespace openblack::gutils
