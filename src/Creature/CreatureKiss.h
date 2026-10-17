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

#include <optional>
#include <utility>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How two creatures kiss. Each creature has a high and a low kiss animation. A creature plays a blend of the two, from
/// the low kiss towards the high kiss, so that the lips of the two creatures meet.
namespace openblack::creature_kiss
{

/// The high and the low kiss animations, in the order of the creature spec file
constexpr size_t k_HighKiss = 212;
constexpr size_t k_LowKiss = 213;

/// Where the lips of a creature are at the moment of the kiss, in the high and in the low kiss animation. Each point is
/// in the space of the creature, at its scale in the world.
struct KissPoints
{
	glm::vec3 high {0.0f};
	glm::vec3 low {0.0f};
};

/// Where the lips are in the blend of the two animations, at the weight of the high kiss
[[nodiscard]] glm::vec3 LipsAt(const KissPoints& points, float highWeight);

/// The weight of the high kiss that a creature uses to kiss another creature, or none when the two cannot kiss:
/// - When the creature is the taller of the two (its high kiss is higher), it bends down to the high kiss of the other.
///   The weight puts its lips at the height of the lips of the other in its high kiss.
/// - When the creature is not the taller, it uses its high kiss alone.
/// The two cannot kiss when the high kiss of the shorter creature is below the low kiss of the taller creature.
[[nodiscard]] std::optional<float> HighWeight(const KissPoints& self, const KissPoints& other);

/// How far in front of the body of the creature its lips are, in its kiss of the other creature. This is 0 when the
/// two cannot kiss.
[[nodiscard]] float KissingDistance(const KissPoints& self, const KissPoints& other);

/// Where two creatures stand to kiss. Each stands the distance from the point halfway between them, on its own side.
/// The first of the pair is the place of the creature. The second is the place of the other creature.
[[nodiscard]] std::pair<glm::vec2, glm::vec2> KissPlaces(glm::vec2 self, glm::vec2 other, float distance);

/// The distance of the two creatures from the halfway point: the mean of the kissing distance of each
[[nodiscard]] float MeanKissingDistance(const KissPoints& self, const KissPoints& other);

} // namespace openblack::creature_kiss
