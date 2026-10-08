/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/mat3x3.hpp>
#include <glm/mat4x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
struct Transform;
}

/// The original game's object matrix as openblack keeps it (wiki: engine-math.md, "LH matrices"). Pure math.
///
/// Conventions. The original's matrix is 3 rows and a translation, for row vectors (p' = p M): row k is the image of
/// local axis k. glm uses column vectors, so column k of a glm matrix is row k of the original's, with the same memory
/// (a glm::mat4x3 is its 12 floats, column 3 being the translation). Rx/Ry/Rz(t) below are glm's right-handed rotations
/// (glm::rotate(t, axis), glm::eulerAngleY(t)); the original turns the other way, so its angle a is glm's -a.
///
/// Precision. The game runs with the FPU at 24 bits, so every product and sum is a float one, but sine and cosine are
/// not rounded by the precision control: a value the original keeps unrounded is taken here in double and rounded once
/// by the product that uses it, and a value it stores is a float, as gutils does (engine-math.md, "GUtils angles").
/// (approximate) double is not the 80-bit register: the last bit may differ in rare cases.
namespace openblack::affine
{

/// RotationYXZ(y, x, z) = glm::eulerAngleYXZ(-y, -x, -z) = Ry(-y) Rx(-x) Rz(-z), cell by cell in the original's order (a = y,
/// b = x, c = z): m0 = (ca cc) - ((sc sb) sa), m1 = -(sc cb), m2 = ((sc sb) ca) + (sa cc), m3 = ((sa cc) sb) + (sc ca),
/// m4 = cc cb, m5 = (sc sa) - ((ca cc) sb), m6 = -(cb sa), m7 = sb, m8 = cb ca. cb and sc are stored as floats, and so
/// are ca cc and sa cc; ca, sa, sb and cc stay unrounded. The translation is not touched (the caller's)
[[nodiscard]] glm::mat3 RotationYXZ(float y, float x, float z);

/// The yaw rotation: rows (c, 0, s), (0, 1, 0), (-s, 0, c) with c and s stored as floats = glm::eulerAngleY(-a) =
/// Ry(-a). The same rotation as PlacementMatrix, an object's world matrix and the scale-then-turn of the creations, and
/// as RotationYXZ(a, 0, 0)
[[nodiscard]] glm::mat3 AngleY(float a);

/// RotationXYZ(x, y, z) = Rz(-z) Ry(-y) Rx(-x): rows (1, 0, 0), (0, cx, -sx), (0, sx, cx) with cx and sx stored; then each
/// row's (e0, e2) -> (cy e0 - sy e2, cy e2 + sy e0) and each row's (e0, e1) -> (cz e0 + sz e1, cz e1 - sz e0), cy, sy,
/// cz and sz unrounded
[[nodiscard]] glm::mat3 RotationXYZ(float x, float y, float z);

/// RotateY(a), in place: r0' = c r0 + s r2, r2' = c r2 - s r0 (rows; c and s unrounded); r1 and the translation stay.
/// In glm: m * Ry(-a), a turn about the matrix's own Y axis (on the right)
void RotateY(glm::mat3& m, float a);
/// RotateZ(a), in place: r0' = c r0 - s r1, r1' = c r1 + s r0 = m * Rz(-a) (on the right)
void RotateZ(glm::mat3& m, float a);
/// The same two turns with c and s given, for the inline copies that keep them with other precisions (the help
/// spirit's HUD pose stores c as a float and keeps s unrounded)
void RotateY(glm::mat3& m, double c, double s);
void RotateZ(glm::mat3& m, double c, double s);
/// (openblack name: the original only has it inline, for the help spirit's pitch) in place: r1' = c r1 - s r2,
/// r2' = c r2 + s r1, the RotateZ pattern on rows 1 and 2; r0 and the translation stay
void RotateX(glm::mat3& m, double c, double s);

/// The world turn of the principal-axis rotation and tumble rules: in every row, the two
/// components about the axis turn (axis 2 = Z: (x, y) -> (c x + s y, c y - s x); 1 = Y: (x, z) -> (c x - s z,
/// c z + s x); 0 = X: (y, z) -> (c y + s z, c z - s y)) = R_axis(-a) * m (on the left). The callers keep c and s with
/// different precisions (one stores c as a float, the other keeps both), so they are given here
void TurnRows(glm::mat3& m, int axis, double c, double s);
/// The same with c and s of the angle unrounded (the tumble rule)
void TurnRows(glm::mat3& m, int axis, float a);

/// The Rodrigues matrix by rows (m0 = ((1 - xx) c) + xx, m3 = (xy - xy c) + s z ...) = glm::rotate(-a, axis); the
/// translation 0. `axis` is unit length
[[nodiscard]] glm::mat3 AxisAngle(const glm::vec3& axis, float a);

/// atan2(b, a) by octants, the arctangent of the smaller over the larger (each quotient a float division):
/// - a >= b and !(-b > a) (a >= |b|): atan(b / a);
/// - b >= a and !(-a > b) (b >= |a|): 1.5707963705062866 - atan(a / b);
/// - -b >= a and !(a >= b) (a <= -|b|): atan(b / a) + 3.1415927410125732, or - when !(b >= 0) (-0 counts as >= 0);
/// - else (b <= -|a|): -1.5707963705062866 - atan(a / b).
/// The ties |a| == |b| go to the first branch that takes them; (0, 0) is 0 / 0 in the first, NaN. The arctangent is not
/// rounded by the precision control, so the first branch returns the extended value (here the double, for the caller's
/// store or product to round); the other three end in a float addition or subtraction, so they return a float value.
/// The constants are float(pi / 2) and float(pi) kept as doubles
[[nodiscard]] double ArcTanOctant(float a, float b);
/// The yaw of a direction: x x + z z (float products and sum) <= 1e-6 -> 0; else ArcTanOctant(-z, x) = atan2(x, -z):
/// 0 along -z, pi / 2 along +x. Returned unrounded (ArcTanOctant's value), y is not read. The villagers' end of physics
/// adds pi (a float addition) and passes the float to WrapAngle. Not GetYAngleOfXZ below, a different function:
/// atan2(z, x), plus 2 pi when negative, in [0, 2 pi)
[[nodiscard]] double GetYAngle(const glm::vec3& v);
/// The arctangent of (b.z - a.z) over (b.x - a.x) (float subtractions), so atan2(dz, dx); negative -> + 2 pi, a float
/// addition. y is not read. Returned unrounded: the extended arctangent (here the double) when not negative, the float
/// sum otherwise; the caller's store or arithmetic rounds it (an object's focus stores it as a float). Not
/// GetYAngle(v) above
[[nodiscard]] double GetYAngleBetween(const glm::vec3& from, const glm::vec3& to);
/// The same of one point, atan2(v.z, v.x) (no subtraction), + 2 pi when negative; in [0, 2 pi)
[[nodiscard]] double GetYAngleOfXZ(const glm::vec3& v);
/// a > pi -> a - 2 pi; else !(a >= -pi) -> a + 2 pi; else a. Once only (10 -> 3.7168), and +-pi itself stays; the
/// result is a float (a float subtraction or addition, or the argument)
[[nodiscard]] float WrapAngle(float a);
/// DecomposeYXZ(y, x, z), the inverse of RotationYXZ: y = ArcTanOctant(m8, -m6); rows 0 and 2 turned by -y (c = (float)cos
/// stored, s = sin kept: m0' = c m0 - s m2, m2' = m2 c + s m0, m8' = s m6 + c m8); x = ArcTanOctant(m8', m7); z =
/// ArcTanOctant(m0', -(s2 m2' + c2 m1)) with c2 = (float)cos(-x) stored and s2 kept. m3..m5 are not read; at the gimbal (m6 =
/// m8 = 0) y is NaN, as in the original
void DecomposeYXZ(const glm::mat3& m, float& y, float& x, float& z);

/// 1 / sqrt(x) from a 128-byte table of the exponent's last bit and the mantissa's first 6 (index (bits >> 17) & 0x7F)
/// under the exponent (0x5F000000 - (e << 22)) & 0xFF800000, then one Newton step ((3 - (x y) y) y) 0.5, every
/// product a float one (the FPU at 24 bits). The table holds ((bits(1 / sqrt(x)) + 0x2000) >> 15) & 0xFF for
/// x = bits((i | 0x1F80) << 17) (0.5 <= x < 2), and then entry 0x40 = 0xFF. It comes out a little below the true value
[[nodiscard]] float InverseSquareRoot(float x);

/// In place: each row (glm's column) times InverseSquareRoot of its length squared, no re-orthogonalisation
void NormaliseRows(glm::mat3& m);

/// The inverse of an object matrix (glm::mat4x3: the 3 rows, then the translation): the adjugate over the determinant,
/// with |det| < 1e-10 clamped to +-1e-10 (the sign of det, + for 0), and the translation -(t A^-1)
[[nodiscard]] glm::mat4x3 Inverse(const glm::mat4x3& m);

/// An object's PlacementMatrix(p, a, s): T(p) Ry(-a) S(s). Four branches on a == 0 and s == 1; with a != 0 the rotation is
/// RotateY in place on diag(s), so the cells are c s and s s with c and s unrounded. The translation: with s != 1
/// 0 + p (the zeroed cells add p: -0 becomes +0), with s == 1 p copied
[[nodiscard]] glm::mat4 PlacementMatrix(const glm::vec3& p, float a, float s);

/// What every Set* of the original writes: the rotation's rows times the scale (the scale multiplies the rows, glm's
/// columns) and the position straight into the translation: T(p) R S
[[nodiscard]] glm::mat4 Model(const glm::vec3& p, const glm::mat3& r, const glm::vec3& s);
[[nodiscard]] glm::mat4 Model(const ecs::components::Transform& transform);

} // namespace openblack::affine
