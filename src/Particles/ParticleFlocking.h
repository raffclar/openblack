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
#include <glm/vec3.hpp>

#include "Common/X87Vector.h"

// The rules of the flocks the miracles' effects fly, and of the forest miracle's effect: the butterflies or bats circling
// over the new forest, the flock rule that has them fly together, and the caster's camera following the forest's path

namespace openblack::particles
{
class ParticleClassRegistry;

namespace flocking
{
/// How strongly something pulls at a distance (never under 0.01, scaled), by its falloff type: 0 the same everywhere,
/// 1 by the distance, 2 by its square; weaker with the distance, or stronger when inverted
[[nodiscard]] float Falloff(float distance, int type, bool invert, float scaleModifier);
/// The turn of a flying atom: along its heading, pitched by part of its climb and banked into its turn as if gravity
/// held it, its axes as columns
[[nodiscard]] glm::mat3 Banking(glm::vec3 velocity, glm::vec3 acceleration, float reducePitchBy, float gravityForBanking);

/// The way of an offset, one long, and its length, with the squares added in the given order. A zero offset has no
/// length and stays zero.
template <x87::SumOrder Order>
[[nodiscard]] glm::vec3 Direction(const glm::vec3& offset, float& distance)
{
	if (offset == glm::vec3(0.0f))
	{
		distance = 0.0f;
		return offset;
	}
	distance = x87::Length<Order>(offset);
	return offset * (1.0f / distance);
}
/// A vector made no longer than most: longer, it is made most long (the squares added z first)
[[nodiscard]] glm::vec3 LimitedLength(const glm::vec3& v, float most);
/// The same limit, but tested on the squares of the lengths
[[nodiscard]] glm::vec3 LimitedSquaredLength(const glm::vec3& v, float most);
} // namespace flocking

namespace forest_path
{
/// Where an atom circling the forest is from the middle: on a sphere squashed by the scales, its angles turning at
/// their speeds from its own two starting angles
[[nodiscard]] glm::vec3 PathPoint(float radius, float atomAge, float thetaSpeed, float phiSpeed, float theta0, float phi0,
                                  glm::vec3 scale);
} // namespace forest_path

/// The flock rule
void RegisterFlockingRules(ParticleClassRegistry& registry);
/// The forest's rules: the path its flocks circle on, the butterflies or bats by its player's alignment, and the caster's
/// camera along its path
void RegisterForestRules(ParticleClassRegistry& registry);

} // namespace openblack::particles
