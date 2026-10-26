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

/// A box in the world. A box with a minimum above its maximum holds no point.
struct Box
{
	glm::vec3 min {0.0f};
	glm::vec3 max {0.0f};
};

/// The number of face blocks that one bounds block holds
inline constexpr std::size_t k_BlocksPerBounds = 4;

/// The boxes of four face blocks, part by part. The box of a face block holds every hit that the search can find on
/// the faces of the block.
struct alignas(16) BoundsBlock
{
	std::array<float, k_BlocksPerBounds> minX {};
	std::array<float, k_BlocksPerBounds> minY {};
	std::array<float, k_BlocksPerBounds> minZ {};
	std::array<float, k_BlocksPerBounds> maxX {};
	std::array<float, k_BlocksPerBounds> maxY {};
	std::array<float, k_BlocksPerBounds> maxZ {};
};

/// The box that holds every hit that the search can find on a face. The box includes the errors of the float
/// operations of the search. A face that no line can hit gets an empty box. A face that is too thin for a safe box
/// gets an infinite box.
[[nodiscard]] Box ReachOf(const WorldFace& face);

/// The faces of a body in blocks of four, and a box for each block. The search does not test a block if the line
/// through the point cannot touch the box of the block. Thus it finds the same face and the same hit as a search of
/// all the blocks.
class FaceSet
{
public:
	/// Removes all faces and makes room for a number of faces
	void Reset(std::size_t faces);
	/// Puts a face into its place. The set must have room for the face.
	void Store(std::size_t face, const WorldFace& world);

	[[nodiscard]] std::span<const FaceBlock> Blocks() const { return _blocks; }
	[[nodiscard]] std::span<const BoundsBlock> Bounds() const { return _bounds; }

private:
	std::vector<FaceBlock> _blocks;
	std::vector<BoundsBlock> _bounds;
};

/// The same search as the search of all the blocks, but it does not test the blocks that the line cannot touch
[[nodiscard]] std::optional<Hit> NearestBehind(const FaceSet& faces, glm::vec3 point, glm::vec3 direction);

} // namespace openblack::physics::face_search
