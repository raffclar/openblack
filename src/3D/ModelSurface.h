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
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class GameRandomInterface;
} // namespace openblack

namespace openblack::l3d
{
class L3DFile;
} // namespace openblack::l3d

namespace openblack::model_surface
{

/// A corner of one of a model's triangles, in the model's space
struct Corner
{
	glm::vec3 position {0.0f};
	glm::vec3 normal {0.0f};
};

using Triangle = std::array<Corner, 3>;

/// Which of a model's parts count as drawn: those at one of the detail levels in the mask, and of a state no later
/// than the one given (parts of later states are the scaffolding of a building going up)
struct DrawnParts
{
	uint32_t detailLevels {1};
	uint32_t latestState {0};
};

/// The triangles of a model's drawn parts, in the order of its parts and their primitives
[[nodiscard]] std::vector<Triangle> DrawnTriangles(const l3d::L3DFile& model, DrawnParts drawn);

/// A point on a model's surface and the surface's direction there, both placed in the world
struct Point
{
	glm::vec3 position {0.0f};
	glm::vec3 normal {0.0f};
};

/// A random point on one of the triangles, drawn from the local (unsynchronised) random numbers: a triangle, then two
/// fractions folded back into the triangle when they add up to more than one. Its normal is blended from the corners'
/// as its position is. Both are placed by the model's placement, the normal turned (not moved) and made of unit length.
/// None when there are no triangles.
[[nodiscard]] std::optional<Point> RandomPoint(std::span<const Triangle> triangles, const glm::mat4& placement,
                                               GameRandomInterface& random);

/// The surface's normal may point down no further than this
inline constexpr float k_LowestNormalY = -0.1f;
/// Points are drawn again until one faces no more than a little downwards. A model with no such point would have the
/// game draw for ever; here the drawing gives up after this many points and gives none.
inline constexpr uint32_t k_MostDraws = 10000;
[[nodiscard]] std::optional<Point> RandomUpwardPoint(std::span<const Triangle> triangles, const glm::mat4& placement,
                                                     GameRandomInterface& random);

} // namespace openblack::model_surface
