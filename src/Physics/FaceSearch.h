/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec3.hpp>

/// The search for the face of a body that a point of another body is behind. The search keeps the faces in blocks of
/// four, part by part, so that one processor instruction tests four faces. Each test does the same float operations in
/// the same order as a test of one face. Thus the result is the same to the last bit.
namespace openblack::physics::face_search
{

/// The number of faces in one block
inline constexpr std::size_t k_Lanes = 4;
/// A face that a point is behind faces the line through the point at most this much
inline constexpr float k_FacingThreshold = -0.0001f;

/// Four faces, part by part: the world normal and the three corners of each face. An empty place in the last block has
/// a zero normal, so that it never faces a line.
struct alignas(16) FaceBlock
{
	std::array<float, k_Lanes> nx {};
	std::array<float, k_Lanes> ny {};
	std::array<float, k_Lanes> nz {};
	std::array<float, k_Lanes> ax {};
	std::array<float, k_Lanes> ay {};
	std::array<float, k_Lanes> az {};
	std::array<float, k_Lanes> bx {};
	std::array<float, k_Lanes> by {};
	std::array<float, k_Lanes> bz {};
	std::array<float, k_Lanes> cx {};
	std::array<float, k_Lanes> cy {};
	std::array<float, k_Lanes> cz {};
};

/// One face in the world: its normal and its three corners
struct WorldFace
{
	glm::vec3 normal {0.0f};
	glm::vec3 a {0.0f};
	glm::vec3 b {0.0f};
	glm::vec3 c {0.0f};
};

/// Puts a face into its place in the blocks. The blocks must have room for the face.
void Store(std::span<FaceBlock> blocks, std::size_t face, const WorldFace& world);
/// The number of blocks that holds a number of faces
[[nodiscard]] constexpr std::size_t BlocksFor(std::size_t faces)
{
	return (faces + k_Lanes - 1) / k_Lanes;
}

/// Where a line through a point meets a face behind the point
struct Hit
{
	glm::vec3 point {0.0f};
	std::size_t face {0};
};

/// The nearest face behind a point along a direction, at most one direction length back. Of the faces that the line
/// meets inside their triangles, the face with the largest negative distance along the line wins. On a tie, the first
/// face wins. The search uses four lanes on processors that have them.
[[nodiscard]] std::optional<Hit> NearestBehind(std::span<const FaceBlock> blocks, glm::vec3 point, glm::vec3 direction);

/// The same search, one face at a time
[[nodiscard]] std::optional<Hit> NearestBehindOneByOne(std::span<const FaceBlock> blocks, glm::vec3 point, glm::vec3 direction);

} // namespace openblack::physics::face_search
