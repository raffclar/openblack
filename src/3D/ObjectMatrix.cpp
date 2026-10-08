/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectMatrix.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>

#include <glm/geometric.hpp>

#include "ECS/Components/Transform.h"

using namespace openblack;

namespace
{
constexpr float k_MinDeterminant = 1e-10f;

// ArcTanOctant's constants: float(pi / 2) and float(pi) kept as doubles
constexpr double k_HalfPiF = 1.5707963705062866;
constexpr double k_MinusHalfPiF = -1.5707963705062866;
constexpr double k_PiF = 3.1415927410125732;
// GetYAngle's and WrapAngle's float constants
constexpr float k_NoYAngle = 1e-6f;
constexpr float k_PiFloat = 3.14159274f;
constexpr float k_MinusPiFloat = -3.14159274f;
constexpr float k_TwoPiFloat = 6.28318548f;
static_assert(static_cast<double>(k_PiFloat) == k_PiF && static_cast<double>(k_PiFloat / 2.0f) == k_HalfPiF);

/// An unrounded sine or cosine times a float, rounded once to float by the product
float Mul(double extended, float value)
{
	return static_cast<float>(extended * static_cast<double>(value));
}

/// An in-place pair turn on components i and j of every row: (e_i, e_j) -> (c e_i + s e_j, c e_j - s e_i)
void TurnPair(glm::mat3& m, int i, int j, double c, double s)
{
	for (int r = 0; r < 3; ++r)
	{
		const float ei = m[r][i];
		const float ej = m[r][j];
		m[r][i] = Mul(c, ei) + Mul(s, ej);
		m[r][j] = Mul(c, ej) - Mul(s, ei);
	}
}

/// The same on rows i and j (each of the three columns): r_i' = c r_i + s r_j, r_j' = c r_j - s r_i
void TurnRowPair(glm::mat3& m, int i, int j, double c, double s)
{
	for (int k = 0; k < 3; ++k)
	{
		const float ei = m[i][k];
		const float ej = m[j][k];
		m[i][k] = Mul(c, ei) + Mul(s, ej);
		m[j][k] = Mul(c, ej) - Mul(s, ei);
	}
}
} // namespace

glm::mat3 affine::RotationYXZ(float y, float x, float z)
{
	const double ca = std::cos(static_cast<double>(y));                   // kept unrounded
	const double sa = std::sin(static_cast<double>(y));                   // kept unrounded
	const auto cb = static_cast<float>(std::cos(static_cast<double>(x))); // stored as a float
	const double sb = std::sin(static_cast<double>(x));                   // kept unrounded
	const double cc = std::cos(static_cast<double>(z));                   // kept unrounded
	const auto sc = static_cast<float>(std::sin(static_cast<double>(z))); // stored as a float
	const auto caCc = static_cast<float>(cc * ca);
	const float scSb = Mul(sb, sc); // not stored, but rounded to float
	const auto saCc = static_cast<float>(cc * sa);
	glm::mat3 m;
	m[0][0] = caCc - Mul(sa, scSb);
	m[0][1] = -(sc * cb);
	m[0][2] = Mul(ca, scSb) + saCc;
	m[1][0] = Mul(sb, saCc) + Mul(ca, sc);
	m[1][1] = Mul(cc, cb);
	m[1][2] = Mul(sa, sc) - Mul(sb, caCc);
	m[2][0] = -Mul(sa, cb);
	m[2][1] = static_cast<float>(sb);
	m[2][2] = Mul(ca, cb);
	return m;
}

void affine::DecomposeYXZ(const glm::mat3& m, float& yOut, float& xOut, float& zOut)
{
	// glm's m[r][c] is the original's cell 3r + c (the header's note)
	const double y = ArcTanOctant(m[2][2], -m[2][0]);
	yOut = static_cast<float>(y);
	const auto c = static_cast<float>(std::cos(-y));
	const double s = std::sin(-y); // kept unrounded
	const float m0 = c * m[0][0] - Mul(s, m[0][2]);
	const float m2 = m[0][2] * c + Mul(s, m[0][0]);
	const float m8 = Mul(s, m[2][0]) + c * m[2][2];
	const double x = ArcTanOctant(m8, m[2][1]);
	xOut = static_cast<float>(x);
	const auto c2 = static_cast<float>(std::cos(-x));
	const double s2 = std::sin(-x); // kept unrounded
	const float t = Mul(s2, m2) + c2 * m[0][1];
	zOut = static_cast<float>(ArcTanOctant(m0, -t));
}

glm::mat3 affine::AngleY(float a)
{
	const auto c = static_cast<float>(std::cos(static_cast<double>(a))); // stored as a float
	const auto s = static_cast<float>(std::sin(static_cast<double>(a))); // stored as a float
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-s, 0.0f, c)};
}

glm::mat3 affine::RotationXYZ(float x, float y, float z)
{
	const auto cx = static_cast<float>(std::cos(static_cast<double>(x)));
	const auto sx = static_cast<float>(std::sin(static_cast<double>(x)));
	glm::mat3 m {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, cx, -sx), glm::vec3(0.0f, sx, cx)};
	// (e0, e2) -> (cy e0 - sy e2, cy e2 + sy e0): TurnPair(0, 2) with -sy
	TurnPair(m, 0, 2, std::cos(static_cast<double>(y)), -std::sin(static_cast<double>(y)));
	// (e0, e1) -> (cz e0 + sz e1, cz e1 - sz e0)
	TurnPair(m, 0, 1, std::cos(static_cast<double>(z)), std::sin(static_cast<double>(z)));
	return m;
}

void affine::RotateY(glm::mat3& m, float a)
{
	RotateY(m, std::cos(static_cast<double>(a)), std::sin(static_cast<double>(a)));
}

void affine::RotateY(glm::mat3& m, double c, double s)
{
	TurnRowPair(m, 0, 2, c, s);
}

void affine::RotateZ(glm::mat3& m, double c, double s)
{
	// r0' = c r0 - s r1, r1' = c r1 + s r0: the pair (0, 1) with -s
	TurnRowPair(m, 0, 1, c, -s);
}

void affine::RotateX(glm::mat3& m, double c, double s)
{
	// r1' = c r1 - s r2, r2' = c r2 + s r1: the pair (1, 2) with -s
	TurnRowPair(m, 1, 2, c, -s);
}

void affine::RotateZ(glm::mat3& m, float a)
{
	// r0' = c r0 - s r1, r1' = c r1 + s r0: the pair (0, 1) with -s
	TurnRowPair(m, 0, 1, std::cos(static_cast<double>(a)), -std::sin(static_cast<double>(a)));
}

namespace
{
/// The 128-entry inverse square root lookup table, built once
const std::array<uint8_t, 128>& InverseSqrtTable()
{
	static const std::array<uint8_t, 128> k_Table = [] {
		std::array<uint8_t, 128> table {};
		for (uint32_t i = 0; i < table.size(); ++i)
		{
			// (i | 0x1F80) << 17 spans 0.5 .. 2; the square root and the division by 1.0 are each
			// rounded to a float
			const float x = std::bit_cast<float>((i | 0x1F80u) << 17);
			const float y = 1.0f / std::sqrt(x);
			// + 0x2000, >> 15, the low byte
			table.at(i) = static_cast<uint8_t>((std::bit_cast<uint32_t>(y) + 0x2000u) >> 15);
		}
		table[0x40] = 0xFF;
		return table;
	}();
	return k_Table;
}
} // namespace

float affine::InverseSquareRoot(float value)
{
	const auto bits = std::bit_cast<uint32_t>(value);
	// The first guess: the halved, negated exponent and the table's mantissa
	const uint32_t exponent = ((bits >> 23) & 0xFFu) << 22;
	const uint32_t guess =
	    ((0x5F000000u - exponent) & 0xFF800000u) | (static_cast<uint32_t>(InverseSqrtTable()[(bits >> 17) & 0x7Fu]) << 15);
	const float y = std::bit_cast<float>(guess);
	// One Newton step, ((3 - (x y) y) y) 0.5, in this order of float products
	float r = value * y;
	r = r * y;
	r = 3.0f - r;
	r = r * y;
	return r * 0.5f;
}

void affine::NormaliseRows(glm::mat3& m)
{
	for (int row = 0; row < 3; ++row)
	{
		m[row] *= InverseSquareRoot(glm::dot(m[row], m[row]));
	}
}

void affine::TurnRows(glm::mat3& m, int axis, double c, double s)
{
	switch (axis)
	{
	case 2:
		TurnPair(m, 0, 1, c, s);
		break;
	case 1: // (x, z) -> (c x - s z, c z + s x)
		TurnPair(m, 0, 2, c, -s);
		break;
	default:
		TurnPair(m, 1, 2, c, s);
		break;
	}
}

void affine::TurnRows(glm::mat3& m, int axis, float a)
{
	TurnRows(m, axis, std::cos(static_cast<double>(a)), std::sin(static_cast<double>(a)));
}

glm::mat3 affine::AxisAngle(const glm::vec3& axis, float a)
{
	const double c = std::cos(static_cast<double>(a)); // kept unrounded
	const double s = std::sin(static_cast<double>(a)); // kept unrounded
	const float xx = axis.x * axis.x;
	const float yy = axis.y * axis.y;
	const float zz = axis.z * axis.z;
	const float xy = axis.x * axis.y;
	const float xz = axis.x * axis.z;
	const float zy = axis.z * axis.y;
	const float sx = Mul(s, axis.x);
	const float sy = Mul(s, axis.y);
	const float sz = Mul(s, axis.z); // not stored, but rounded to float
	const float t = xy - Mul(c, xy);
	const float u = xz - Mul(c, xz);
	const float v = zy - Mul(c, zy);
	glm::mat3 m;
	m[0][0] = Mul(c, 1.0f - xx) + xx;
	m[1][0] = t + sz;
	m[2][0] = u - sy;
	m[0][1] = t - sz;
	m[1][1] = Mul(c, 1.0f - yy) + yy;
	m[2][1] = v + sx;
	m[0][2] = u + sy;
	m[1][2] = v - sx;
	m[2][2] = Mul(c, 1.0f - zz) + zz;
	return m;
}

double affine::ArcTanOctant(float a, float b)
{
	// The comparisons are written so that a NaN takes the same branch as in the original
	if (a >= b && !(-b > a))
	{
		return std::atan(static_cast<double>(b / a)); // not rounded
	}
	if (b >= a && !(-a > b))
	{
		// rounded to float by the subtraction
		return static_cast<float>(k_HalfPiF - std::atan(static_cast<double>(a / b)));
	}
	if (-b >= a && !(a >= b))
	{
		const double t = std::atan(static_cast<double>(b / a));
		// b < 0 subtracts pi, otherwise adds it (rounded to float)
		return static_cast<float>(!(b >= 0.0f) ? t - k_PiF : t + k_PiF);
	}
	// rounded to float by the subtraction
	return static_cast<float>(k_MinusHalfPiF - std::atan(static_cast<double>(a / b)));
}

double affine::GetYAngle(const glm::vec3& v)
{
	const float xx = v.x * v.x;
	const float zz = v.z * v.z;
	const float horizontal = xx + zz;
	if (!(horizontal > k_NoYAngle))
	{
		return 0.0;
	}
	return ArcTanOctant(-v.z, v.x);
}

namespace
{
/// atan2, unrounded; a negative angle gets 2 pi added and is rounded to float. (approximate) The original's arctangent
/// is 80-bit: std::atan2 in double may differ in the last bit before the caller rounds it to float
double PositiveYAngle(float z, float x)
{
	const double angle = std::atan2(static_cast<double>(z), static_cast<double>(x));
	if (angle < 0.0)
	{
		return static_cast<double>(static_cast<float>(angle + static_cast<double>(k_TwoPiFloat)));
	}
	return angle;
}
} // namespace

double affine::GetYAngleBetween(const glm::vec3& from, const glm::vec3& to)
{
	const float dz = to.z - from.z;
	const float dx = to.x - from.x;
	return PositiveYAngle(dz, dx);
}

double affine::GetYAngleOfXZ(const glm::vec3& v)
{
	return PositiveYAngle(v.z, v.x);
}

float affine::WrapAngle(float a)
{
	if (a > k_PiFloat)
	{
		return a - k_TwoPiFloat;
	}
	if (!(a >= k_MinusPiFloat))
	{
		return a + k_TwoPiFloat;
	}
	return a;
}

glm::mat4x3 affine::Inverse(const glm::mat4x3& m)
{
	// m0..m8 are the rows' cells (m[r][c] = cell 3r + c), t0..t2 the translation
	const float m0 = m[0][0];
	const float m1 = m[0][1];
	const float m2 = m[0][2];
	const float m3 = m[1][0];
	const float m4 = m[1][1];
	const float m5 = m[1][2];
	const float m6 = m[2][0];
	const float m7 = m[2][1];
	const float m8 = m[2][2];
	// a = m8 m4 - m7 m5; det = ((m2 m7 - m8 m1) m3 + (m5 m1 - m2 m4) m6) + a m0
	const float a = m8 * m4 - m7 * m5;
	float det = (m2 * m7 - m8 * m1) * m3 + (m5 * m1 - m2 * m4) * m6;
	det = det + a * m0;
	// kept when 1e-10 <= |det|, or unordered
	if (k_MinDeterminant > std::abs(det))
	{
		det = det < 0.0f ? -k_MinDeterminant : k_MinDeterminant;
	}
	const float inv = 1.0f / det;
	glm::mat4x3 out;
	out[0][0] = a * inv;
	out[1][0] = (m6 * m5 - m8 * m3) * inv;
	out[2][0] = (m7 * m3 - m6 * m4) * inv;
	out[0][1] = (m2 * m7 - m8 * m1) * inv;
	out[1][1] = (m8 * m0 - m6 * m2) * inv;
	out[2][1] = (m6 * m1 - m0 * m7) * inv;
	out[0][2] = (m5 * m1 - m2 * m4) * inv;
	out[1][2] = (m2 * m3 - m0 * m5) * inv;
	out[2][2] = (m0 * m4 - m3 * m1) * inv;
	const float t0 = m[3][0];
	const float t1 = m[3][1];
	const float t2 = m[3][2];
	out[3][0] = -((t1 * out[1][0] + t2 * out[2][0]) + t0 * out[0][0]);
	out[3][1] = -((out[2][1] * t2 + t0 * out[0][1]) + out[1][1] * t1);
	out[3][2] = -((t0 * out[0][2] + out[2][2] * t2) + out[1][2] * t1);
	return out;
}

glm::mat4 affine::PlacementMatrix(const glm::vec3& p, float a, float s)
{
	glm::mat4 m(0.0f);
	// s != 1 adds p to the zeroed translation, s == 1 copies it
	const glm::vec3 t = s != 1.0f ? glm::vec3(0.0f) + p : p;
	if (a == 0.0f) // diag(s), no turn
	{
		m[0][0] = s;
		m[1][1] = s;
		m[2][2] = s;
		m[3] = glm::vec4(t, 1.0f);
		return m;
	}
	// diag(s), then RotateY in place: rows (c s, 0, s s), (0, s, 0), (-(s s), 0, c s)
	const double c = std::cos(static_cast<double>(a));
	const double sn = std::sin(static_cast<double>(a));
	m[0] = glm::vec4(Mul(c, s), 0.0f, Mul(sn, s), 0.0f);
	m[1] = glm::vec4(0.0f, s, 0.0f, 0.0f);
	m[2] = glm::vec4(-Mul(sn, s), 0.0f, Mul(c, s), 0.0f);
	m[3] = glm::vec4(t, 1.0f);
	return m;
}

glm::mat4 affine::Model(const glm::vec3& p, const glm::mat3& r, const glm::vec3& s)
{
	return {glm::vec4(r[0] * s.x, 0.0f), glm::vec4(r[1] * s.y, 0.0f), glm::vec4(r[2] * s.z, 0.0f), glm::vec4(p, 1.0f)};
}

glm::mat4 affine::Model(const ecs::components::Transform& transform)
{
	return Model(transform.position, transform.rotation, transform.scale);
}
