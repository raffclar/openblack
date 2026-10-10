/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GroundBlobs.h"

#include <glm/geometric.hpp>

namespace openblack::graphics::ground_blobs
{

namespace
{
/// Half a blob's width, either way across the foot: 0.2 along the diagonals
const glm::vec3 k_Across = glm::normalize(glm::vec3(-1.0f, 0.0f, 1.0f)) * 0.2f;
/// The light's offset, 2 units along the diagonal
const glm::vec3 k_Offset = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)) * 2.0f;
/// How far behind the foot a blob starts, as a part of its fall
constexpr float k_Behind = -0.02f;
} // namespace

glm::vec3 Fall(const glm::vec3& landNormal, float scale)
{
	const auto offset = k_Offset * scale;
	return offset - (glm::dot(offset, landNormal) * landNormal);
}

namespace
{
Quad MakeQuad(const glm::vec3& foot, const glm::vec3& fall, const glm::vec3& across)
{
	const auto behind = fall * k_Behind;
	return {{foot + behind - across, foot + behind + across, foot + fall + across, foot + fall - across}};
}
} // namespace

Quad MakeQuad(const glm::vec3& foot, const glm::vec3& fall)
{
	return MakeQuad(foot, fall, k_Across);
}

std::array<Quad, 2> Feet(const glm::vec3& first, const glm::vec3& second, const glm::vec3& fall)
{
	return {MakeQuad(first, fall + ((second - first) * 0.5f)), MakeQuad(second, fall + ((first - second) * 0.5f))};
}

std::array<Quad, 2> Far(const glm::vec3& foot, float scale)
{
	const auto quad = MakeQuad(foot, Fall(glm::vec3(0.0f, 1.0f, 0.0f), scale), k_Across * k_FarWidening);
	return {quad, quad};
}

std::array<glm::vec3, 4> SmudgeCorners(const glm::vec3& position, float scale, float smudgeScale, const glm::vec3& right,
                                       const glm::vec3& up)
{
	const auto centre = position + glm::vec3(0.0f, k_SmudgeRise * scale, 0.0f);
	const auto across = right * (k_SmudgeHalfWidth * smudgeScale);
	const auto upward = up * (k_SmudgeHalfWidth * smudgeScale * k_SmudgeTallness);
	return {centre - across + upward, centre + across + upward, centre + across - upward, centre - across - upward};
}

} // namespace openblack::graphics::ground_blobs
