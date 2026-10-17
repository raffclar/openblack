/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <array>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "X87.h"

/// The 3D engine's matrix maths, in the engine's order.
///
/// The engine keeps a matrix as three rows and a place, and multiplies a point as a row vector. In these functions the
/// columns of a glm matrix are the engine's rows, and its last column is the place.
namespace openblack::x87
{

/// For each world axis, the order in which the three products of a point with the rows are added: three row indices
/// (0, 1, 2) each
using AxisOrders = std::array<std::array<uint8_t, 3>, 3>;

/// The order in which the 3D engine adds the products when it moves the points of a mesh: z, y, x for the x axis and
/// x, z, y for the others
inline constexpr AxisOrders k_MeshPointOrder {{{2, 1, 0}, {0, 2, 1}, {0, 2, 1}}};

/// A point moved by a matrix: for each axis the three products are added in the given order, then the place is added
[[nodiscard]] inline glm::vec3 TransformPoint(const glm::mat4& m, const glm::vec3& point, const AxisOrders& orders)
{
	glm::vec3 moved {};
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		const auto& order = orders.at(static_cast<size_t>(axis));
		const auto product = [&m, &point, axis](uint8_t row) { return m[row][axis] * point[row]; };
		moved[axis] = ((product(order[0]) + product(order[1])) + product(order[2])) + m[3][axis];
	}
	return moved;
}

/// A point turned by a matrix: for each axis the three products are added in the given order, and the place is not
/// added
[[nodiscard]] inline glm::vec3 TurnPoint(const glm::mat4& m, const glm::vec3& point, const AxisOrders& orders)
{
	glm::vec3 turned {};
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		const auto& order = orders.at(static_cast<size_t>(axis));
		const auto product = [&m, &point, axis](uint8_t row) { return m[row][axis] * point[row]; };
		turned[axis] = (product(order[0]) + product(order[1])) + product(order[2]);
	}
	return turned;
}

/// The product of two matrices: a point moved by `first`, then by `second`. The engine moves each row of `first`
/// through the turn of `second` in the mesh order, and moves the place of `first` through all of `second`.
[[nodiscard]] inline glm::mat4 Product(const glm::mat4& first, const glm::mat4& second)
{
	glm::mat4 product {1.0f};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		product[row] = glm::vec4(TurnPoint(second, glm::vec3(first[row]), k_MeshPointOrder), 0.0f);
	}
	product[3] = glm::vec4(TransformPoint(second, glm::vec3(first[3]), k_MeshPointOrder), 1.0f);
	return product;
}

/// The same product as `Product`, in the other order of the engine: the engine puts `first` in front of a matrix that
/// it already has. The products for the first row of `first` are added z, y, x, and the products for the other rows
/// and the place are added x, z, y.
[[nodiscard]] inline glm::mat4 PreProduct(const glm::mat4& first, const glm::mat4& second)
{
	constexpr AxisOrders k_FirstRow {{{2, 1, 0}, {2, 1, 0}, {2, 1, 0}}};
	constexpr AxisOrders k_OtherRows {{{0, 2, 1}, {0, 2, 1}, {0, 2, 1}}};
	glm::mat4 product {1.0f};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		product[row] = glm::vec4(TurnPoint(second, glm::vec3(first[row]), row == 0 ? k_FirstRow : k_OtherRows), 0.0f);
	}
	product[3] = glm::vec4(TransformPoint(second, glm::vec3(first[3]), k_OtherRows), 1.0f);
	return product;
}

/// A turn by an angle about an axis, as the engine makes it: the axis made unit (its squares added z first) unless it is
/// zero, a quaternion of the half angle with its sine and cosine stored as floats, then the matrix of that quaternion.
/// The place is zero.
[[nodiscard]] inline glm::mat4 AxisTurn(const glm::vec3& axis, float angle)
{
	glm::vec3 unit = axis;
	if (axis != glm::vec3(0.0f))
	{
		const float share = 1.0f / std::sqrt(((axis.z * axis.z) + (axis.y * axis.y)) + (axis.x * axis.x));
		unit = axis * share;
	}
	const float half = angle * 0.5f;
	const double wideSine = Sin(half);
	const float sine = RoundToFloat(wideSine);
	// The first part takes the wide sine, the other two take the stored float
	const float x = Mul(wideSine, unit.x);
	const float y = sine * unit.y;
	const float z = sine * unit.z;
	const float w = RoundToFloat(Cos(half));
	const float scale = 2.0f / ((((w * w) + (x * x)) + (y * y)) + (z * z));
	const float xs = scale * x;
	const float ys = scale * y;
	const float zs = scale * z;
	const float wx = xs * w;
	const float wy = ys * w;
	const float wz = zs * w;
	const float xx = xs * x;
	const float xy = ys * x;
	const float xz = zs * x;
	const float yy = ys * y;
	const float yz = zs * y;
	const float zz = zs * z;
	glm::mat4 turn {1.0f};
	turn[0] = {1.0f - (zz + yy), xy + wz, xz - wy, 0.0f};
	turn[1] = {xy - wz, 1.0f - (zz + xx), yz + wx, 0.0f};
	turn[2] = {xz + wy, yz - wx, 1.0f - (yy + xx), 0.0f};
	turn[3] = {0.0f, 0.0f, 0.0f, 1.0f};
	return turn;
}

/// The inverse of a matrix's turn, as the engine finds it: from the cofactors, with the determinant kept at least
/// 1e-10 from zero
[[nodiscard]] inline glm::mat3 InverseTurn(const glm::mat3& frame)
{
	constexpr float k_SmallestDeterminant = 1e-10f;
	const auto m = [&frame](int row, int column) { return frame[row][column]; };
	const float first = (m(2, 2) * m(1, 1)) - (m(2, 1) * m(1, 2));
	const float across = (m(0, 2) * m(2, 1)) - (m(2, 2) * m(0, 1));
	const float down = (m(1, 2) * m(0, 1)) - (m(0, 2) * m(1, 1));
	float determinant = ((across * m(1, 0)) + (down * m(2, 0))) + (first * m(0, 0));
	if (std::abs(determinant) < k_SmallestDeterminant)
	{
		determinant = determinant < 0.0f ? -k_SmallestDeterminant : k_SmallestDeterminant;
	}
	const float share = 1.0f / determinant;
	glm::mat3 inverse;
	inverse[0][0] = first * share;
	inverse[1][0] = ((m(2, 0) * m(1, 2)) - (m(2, 2) * m(1, 0))) * share;
	inverse[2][0] = ((m(2, 1) * m(1, 0)) - (m(2, 0) * m(1, 1))) * share;
	inverse[0][1] = across * share;
	inverse[1][1] = ((m(2, 2) * m(0, 0)) - (m(2, 0) * m(0, 2))) * share;
	inverse[2][1] = ((m(2, 0) * m(0, 1)) - (m(0, 0) * m(2, 1))) * share;
	inverse[0][2] = down * share;
	inverse[1][2] = ((m(0, 2) * m(1, 0)) - (m(0, 0) * m(1, 2))) * share;
	inverse[2][2] = ((m(0, 0) * m(1, 1)) - (m(1, 0) * m(0, 1))) * share;
	return inverse;
}

/// The inverse of a matrix, as the engine finds it: the inverse of the turn, then the place moved back through it
[[nodiscard]] inline glm::mat4 Inverse(const glm::mat4& matrix)
{
	const auto turn = InverseTurn(glm::mat3(matrix));
	const glm::vec3 place {matrix[3]};
	glm::mat4 inverse {turn};
	inverse[3][0] = -(((place.y * turn[1][0]) + (place.z * turn[2][0])) + (place.x * turn[0][0]));
	inverse[3][1] = -(((turn[2][1] * place.z) + (place.x * turn[0][1])) + (turn[1][1] * place.y));
	inverse[3][2] = -(((place.x * turn[0][2]) + (turn[2][2] * place.z)) + (turn[1][2] * place.y));
	return inverse;
}

} // namespace openblack::x87
