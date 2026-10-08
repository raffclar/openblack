/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <3D/ObjectMatrix.h>
#include <ECS/Components/Transform.h>
#include <ECS/Town/Workshops.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/matrix.hpp>
#include <gtest/gtest.h>

// affine (3D/ObjectMatrix.h): the original game's matrix constructors, rows of the original = columns of glm.
// Signs and order: where each local axis goes (hand-derived from the original's cells), the glm composition
// each one equals (to rounding) and the ones it must not be.

namespace affine = openblack::affine;

namespace
{
constexpr float k_HalfPi = std::numbers::pi_v<float> / 2.0f;

void ExpectNear(const glm::mat3& a, const glm::mat3& b, float tolerance)
{
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(a[c][r], b[c][r], tolerance) << "column " << c << " row " << r;
		}
	}
}

float MaxDifference(const glm::mat3& a, const glm::mat3& b)
{
	float d = 0.0f;
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			d = std::max(d, std::abs(a[c][r] - b[c][r]));
		}
	}
	return d;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float tolerance)
{
	EXPECT_NEAR(a.x, b.x, tolerance);
	EXPECT_NEAR(a.y, b.y, tolerance);
	EXPECT_NEAR(a.z, b.z, tolerance);
}

glm::mat3 Rx(float a)
{
	return glm::mat3(glm::eulerAngleX(a));
}
glm::mat3 Ry(float a)
{
	return glm::mat3(glm::eulerAngleY(a));
}
glm::mat3 Rz(float a)
{
	return glm::mat3(glm::eulerAngleZ(a));
}

/// A matrix that is not symmetric, to tell left from right
const glm::mat3 k_Some = affine::RotationYXZ(0.4f, -0.9f, 1.3f);
} // namespace

TEST(GameMatrix, AngleY)
{
	// AngleY: rows (c, 0, s), (0, 1, 0), (-s, 0, c)
	const auto m = affine::AngleY(0.5f);
	EXPECT_EQ(m[0], glm::vec3(static_cast<float>(std::cos(0.5)), 0.0f, static_cast<float>(std::sin(0.5))));
	EXPECT_EQ(m[1], glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(m[2], glm::vec3(-static_cast<float>(std::sin(0.5)), 0.0f, static_cast<float>(std::cos(0.5))));
	// a quarter turn takes local X to world +Z and local Z to world -X: Ry(-a)
	const auto q = affine::AngleY(k_HalfPi);
	ExpectNear(q * glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1e-7f);
	ExpectNear(q * glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(-1.0f, 0.0f, 0.0f), 1e-7f);
	for (const float a : {-2.7f, -0.3f, 0.0f, 1.1f, 3.0f})
	{
		ExpectNear(affine::AngleY(a), Ry(-a), 1e-7f);
	}
}

TEST(GameMatrix, RotationYXZ)
{
	// RotationYXZ with only one angle: m7 = sb, m4 = cc cb, m1 = -(sc cb)
	const auto x = affine::RotationYXZ(0.0f, k_HalfPi, 0.0f);
	ExpectNear(x * glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1e-7f);  // row 2 = (-(cb sa), sb, cb ca)
	ExpectNear(x * glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 1e-7f); // row 1: Rx(-x)
	const auto z = affine::RotationYXZ(0.0f, 0.0f, k_HalfPi);
	ExpectNear(z * glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 1e-7f); // row 0 = (.., -(sc cb), ..): Rz(-z)
	// with x = z = 0 every term with sb or sc is 0: bit for bit the Y-only rotation
	for (const float a : {-2.7f, -0.3f, 0.0f, 1.1f, 3.0f})
	{
		EXPECT_EQ(affine::RotationYXZ(a, 0.0f, 0.0f), affine::AngleY(a)) << a;
	}
	// the order: Ry(-y) Rx(-x) Rz(-z) = glm::eulerAngleYXZ(-y, -x, -z), and not the +angles nor XYZ
	const float y = 0.7f;
	const float xa = -1.2f;
	const float za = 2.1f;
	const auto m = affine::RotationYXZ(y, xa, za);
	ExpectNear(m, Ry(-y) * Rx(-xa) * Rz(-za), 1e-6f);
	ExpectNear(m, glm::mat3(glm::eulerAngleYXZ(-y, -xa, -za)), 1e-6f);
	EXPECT_GT(MaxDifference(m, glm::mat3(glm::eulerAngleYXZ(y, xa, za))), 0.5f);
	EXPECT_GT(MaxDifference(m, glm::mat3(glm::eulerAngleXYZ(xa, y, za))), 0.5f);
}

TEST(GameMatrix, GetYXZInvertsYXZ)
{
	// DecomposeYXZ gives back the angles RotationYXZ was given (inside their ranges)
	for (const auto& [y, x, z] : {std::array {0.7f, -1.2f, 2.1f}, std::array {-2.9f, 0.4f, -0.6f},
	                              std::array {0.0f, 0.0f, 0.0f}, std::array {1.5f, 1.4f, -3.0f}})
	{
		float gy = 0.0f;
		float gx = 0.0f;
		float gz = 0.0f;
		affine::DecomposeYXZ(affine::RotationYXZ(y, x, z), gy, gx, gz);
		EXPECT_NEAR(gy, y, 1e-5f);
		EXPECT_NEAR(gx, x, 1e-5f);
		EXPECT_NEAR(gz, z, 1e-5f);
	}
	// the gimbal (m6 = m8 = 0): y is NaN, as in the original
	float gy = 0.0f;
	float gx = 0.0f;
	float gz = 0.0f;
	affine::DecomposeYXZ(affine::RotationYXZ(0.3f, k_HalfPi, 0.2f), gy, gx, gz);
	EXPECT_TRUE(std::isnan(gy) || std::abs(gx - k_HalfPi) < 1e-3f);
}

TEST(GameMatrix, SnappedScaffoldKeepsThePointsAngles)
{
	// a tilted, scaled slot point: the snapped scaffold gets the bare rotation of its three angles
	const auto rotation = affine::RotationYXZ(0.7f, -0.3f, 0.2f);
	const auto snapped = openblack::ecs::workshops::SnappedRotation(rotation * 2.5f);
	EXPECT_LT(MaxDifference(snapped, rotation), 1e-5f);
	// the angles in their own places: swapping y and x gives another matrix
	EXPECT_GT(MaxDifference(snapped, affine::RotationYXZ(-0.3f, 0.7f, 0.2f)), 0.1f);
	// no point: no rotation
	EXPECT_LT(MaxDifference(openblack::ecs::workshops::SnappedRotation(std::nullopt), glm::mat3(1.0f)), 1e-6f);
}

TEST(GameMatrix, RotationXYZ)
{
	// RotationXYZ: rows (1, 0, 0), (0, cx, -sx), (0, sx, cx), then the y and z turns
	const auto x = affine::RotationXYZ(0.5f, 0.0f, 0.0f);
	const auto cx = static_cast<float>(std::cos(0.5));
	const auto sx = static_cast<float>(std::sin(0.5));
	EXPECT_EQ(x[0], glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(x[1], glm::vec3(0.0f, cx, -sx));
	EXPECT_EQ(x[2], glm::vec3(0.0f, sx, cx));
	// (e0, e2) -> (cy e0 - sy e2, cy e2 + sy e0): local X goes to (c, 0, s) like AngleY
	ExpectNear(affine::RotationXYZ(0.0f, k_HalfPi, 0.0f) * glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 1e-7f);
	// (e0, e1) -> (cz e0 + sz e1, cz e1 - sz e0): local X goes to (c, -s, 0)
	ExpectNear(affine::RotationXYZ(0.0f, 0.0f, k_HalfPi) * glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 1e-7f);
	// the order: Rz(-z) Ry(-y) Rx(-x), not glm::eulerAngleXYZ(x, y, z) (what RandomiseOrientation used to be)
	const auto m = affine::RotationXYZ(0.3f, -1.1f, 2.4f);
	ExpectNear(m, Rz(-2.4f) * Ry(1.1f) * Rx(-0.3f), 1e-6f);
	EXPECT_GT(MaxDifference(m, glm::mat3(glm::eulerAngleXYZ(0.3f, -1.1f, 2.4f))), 0.5f);
}

TEST(GameMatrix, InPlaceTurnsMixRows)
{
	// RotateY: r0' = c r0 + s r2, r2' = c r2 - s r0; on the identity, AngleY bit for bit
	for (const float a : {-2.7f, 0.4f, 1.9f})
	{
		glm::mat3 m(1.0f);
		affine::RotateY(m, a);
		EXPECT_EQ(m, affine::AngleY(a)) << a;
	}
	// on another matrix: m Ry(-a) (on the right, the matrix's own axes), not Ry(-a) m
	glm::mat3 m = k_Some;
	affine::RotateY(m, 0.8f);
	ExpectNear(m, k_Some * Ry(-0.8f), 1e-6f);
	EXPECT_GT(MaxDifference(m, Ry(-0.8f) * k_Some), 0.1f);
	// RotateZ: r0' = c r0 - s r1, r1' = c r1 + s r0 = m Rz(-a)
	glm::mat3 z(1.0f);
	affine::RotateZ(z, k_HalfPi);
	ExpectNear(z * glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 1e-7f);
	z = k_Some;
	affine::RotateZ(z, -0.6f);
	ExpectNear(z, k_Some * Rz(0.6f), 1e-6f);
	EXPECT_GT(MaxDifference(z, Rz(0.6f) * k_Some), 0.1f);
}

TEST(GameMatrix, TurnRowsMixComponents)
{
	// the UpdateRuleRotatePrincipalAxis / AppearanceRuleTumble rules: each row's two components, R(-a) m
	// (on the left, the world's axes). Z: (x, y) -> (c x + s y, c y - s x)
	glm::mat3 m(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	affine::TurnRows(m, 2, k_HalfPi);
	ExpectNear(m[0], glm::vec3(0.0f, -1.0f, 0.0f), 1e-7f);
	ExpectNear(m[1], glm::vec3(1.0f, 0.0f, 0.0f), 1e-7f);
	EXPECT_EQ(m[2], glm::vec3(0.0f, 0.0f, 1.0f)); // the third component is not touched
	const glm::vec3 axes[3] = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
	for (int axis = 0; axis < 3; ++axis)
	{
		glm::mat3 t = k_Some;
		affine::TurnRows(t, axis, 0.9f);
		const auto r = glm::mat3(glm::rotate(glm::mat4(1.0f), -0.9f, axes[axis]));
		ExpectNear(t, r * k_Some, 1e-6f);
		EXPECT_GT(MaxDifference(t, k_Some * r), 0.1f) << axis;
	}
	// the overload with c and s: the same turn
	glm::mat3 a = k_Some;
	glm::mat3 b = k_Some;
	affine::TurnRows(a, 1, 0.9f);
	affine::TurnRows(b, 1, std::cos(static_cast<double>(0.9f)), std::sin(static_cast<double>(0.9f)));
	EXPECT_EQ(a, b);
}

TEST(GameMatrix, AxisAngle)
{
	// AxisAngle about X: m0 = ((1 - 1) c) + 1 = 1, m4 = m8 = c, m7 = s x, m5 = -s x: RotationXYZ's X step, bit for bit
	for (const float a : {-1.3f, 0.2f, 2.9f})
	{
		EXPECT_EQ(affine::AxisAngle(glm::vec3(1.0f, 0.0f, 0.0f), a), affine::RotationXYZ(a, 0.0f, 0.0f)) << a;
	}
	// glm::rotate(-a, axis): about Y it is AngleY
	ExpectNear(affine::AxisAngle(glm::vec3(0.0f, 1.0f, 0.0f), 0.7f), affine::AngleY(0.7f), 1e-7f);
	const auto axis = glm::normalize(glm::vec3(0.3f, -0.8f, 0.5f));
	ExpectNear(affine::AxisAngle(axis, 1.1f), glm::mat3(glm::rotate(glm::mat4(1.0f), -1.1f, axis)), 1e-6f);
	EXPECT_GT(MaxDifference(affine::AxisAngle(axis, 1.1f), glm::mat3(glm::rotate(glm::mat4(1.0f), 1.1f, axis))), 0.5f);
}

TEST(GameMatrix, Inverse)
{
	// Inverse of T(p) R S: the inverse matrix, translation -(t A^-1)
	const glm::mat4 world = affine::PlacementMatrix(glm::vec3(1800.0f, 25.0f, 2700.0f), 0.6f, 1.5f);
	const glm::mat4x3 lh {glm::vec3(world[0]), glm::vec3(world[1]), glm::vec3(world[2]), glm::vec3(world[3])};
	const auto inv = affine::Inverse(lh);
	const auto expected = glm::inverse(world);
	for (int c = 0; c < 4; ++c)
	{
		ExpectNear(inv[c], glm::vec3(expected[c]), 5e-3f);
	}
	// |det| < 1e-10 is clamped to +-1e-10: diag(1e-4) has det 1e-12, so the cofactor 1e-8 / 1e-10 = 100
	// (not 1e4); with det -1e-12, -1e-10 and -100
	const glm::mat4x3 tiny(glm::vec3(1e-4f, 0.0f, 0.0f), glm::vec3(0.0f, 1e-4f, 0.0f), glm::vec3(0.0f, 0.0f, 1e-4f),
	                       glm::vec3(0.0f));
	EXPECT_NEAR(affine::Inverse(tiny)[0][0], 100.0f, 1e-3f);
	const glm::mat4x3 negative(glm::vec3(-1e-4f, 0.0f, 0.0f), glm::vec3(0.0f, 1e-4f, 0.0f), glm::vec3(0.0f, 0.0f, 1e-4f),
	                           glm::vec3(0.0f));
	EXPECT_NEAR(affine::Inverse(negative)[0][0], -100.0f, 1e-3f);
	// a singular matrix (det 0) takes +1e-10
	const glm::mat4x3 zero(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f));
	EXPECT_EQ(affine::Inverse(zero)[0][0], 0.0f);
}

TEST(GameMatrix, TurnsWithCosineAndSine)
{
	// the turns with c and s given are the angle ones with the same c and s; RotateX turns rows 1 and 2
	const glm::mat3 base = affine::RotationYXZ(0.3f, -0.2f, 0.7f);
	glm::mat3 a = base;
	glm::mat3 b = base;
	affine::RotateY(a, 0.4f);
	affine::RotateY(b, std::cos(static_cast<double>(0.4f)), std::sin(static_cast<double>(0.4f)));
	EXPECT_EQ(a, b);
	affine::RotateZ(a, -0.9f);
	affine::RotateZ(b, std::cos(static_cast<double>(-0.9f)), std::sin(static_cast<double>(-0.9f)));
	EXPECT_EQ(a, b);
	glm::mat3 x = base;
	affine::RotateX(x, std::cos(0.5), std::sin(0.5));
	EXPECT_EQ(x[0], base[0]);
	ExpectNear(x,
	           glm::mat3(base[0], std::cos(0.5f) * base[1] - std::sin(0.5f) * base[2],
	                     std::cos(0.5f) * base[2] + std::sin(0.5f) * base[1]),
	           1e-6f);
}

TEST(GameMatrix, SetPositionAndModel)
{
	// PlacementMatrix: T(p) Ry(-a) S(s), the translation the position itself
	const glm::vec3 p(1788.4f, 28.9f, 2710.0f);
	const auto m = affine::PlacementMatrix(p, 0.6f, 1.5f);
	EXPECT_EQ(glm::vec3(m[3]), p);
	ExpectNear(glm::mat3(m), affine::AngleY(0.6f) * 1.5f, 1e-6f);
	EXPECT_EQ(m[1], glm::vec4(0.0f, 1.5f, 0.0f, 0.0f));
	// a == 0: diag(s), no turn
	const auto still = affine::PlacementMatrix(p, 0.0f, 2.0f);
	EXPECT_EQ(glm::mat3(still), glm::mat3(2.0f));
	// the translation: 0 + p with s != 1 (-0 becomes +0), p copied with s == 1
	const glm::vec3 negativeZero(-0.0f, 1.0f, 2.0f);
	EXPECT_FALSE(std::signbit(affine::PlacementMatrix(negativeZero, 0.6f, 1.5f)[3].x));
	EXPECT_FALSE(std::signbit(affine::PlacementMatrix(negativeZero, 0.0f, 1.5f)[3].x));
	EXPECT_TRUE(std::signbit(affine::PlacementMatrix(negativeZero, 0.6f, 1.0f)[3].x));
	// Model: the rows times the scale and the position as it is, = translate * R * scale of glm
	const openblack::ecs::components::Transform transform {p, affine::RotationYXZ(0.3f, 0.2f, -0.1f),
	                                                       glm::vec3(1.0f, 2.0f, 3.0f)};
	const auto model = affine::Model(transform);
	EXPECT_EQ(model, glm::translate(p) * glm::mat4(transform.rotation) * glm::scale(transform.scale));
	EXPECT_EQ(glm::vec3(model[3]), p);
}
