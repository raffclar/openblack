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
