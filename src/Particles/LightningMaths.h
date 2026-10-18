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
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The lightning bolt's maths: how many of the land's cells it searches, how a creature in its cone draws every fork to
// itself, where two players' bolts meet when they clash, and the electric arcs a strike leaves crawling over what it hit.
// Free of state, so they are tested on their own.

namespace openblack::particles::maths
{

/// A bolt searches this many of the land's cells, spiralling out from the hand's: a square twice its radius across
inline constexpr float k_StrikeCellSize = 10.0f;
[[nodiscard]] size_t StrikeSearchCells(float radius);
/// A bolt from a cloud or a parent searches a quarter as many: a square its radius across. A bolt with a radius of zero,
/// or a little less than zero, searches no cells.
[[nodiscard]] size_t CloudSearchCells(float radius);
/// The forks a bolt keeps ready: two for each target that it found
[[nodiscard]] size_t ForkCount(size_t targets);

/// Which search a ground point is for. Each search makes the offset of the point with different float steps.
enum class GroundPointSearch : uint8_t
{
	/// In front of a hand: the cosine becomes a float before it multiplies the distance
	Hand,
	/// Round a cloud or a parent: the sine and the cosine multiply the distance at their full precision
	Around,
	/// Round the targets of a miracle: the sine becomes a float before it multiplies the distance
	SpellTargets,
};
/// The offset across the land from the origin of a bolt to a ground point at an angle and a distance
[[nodiscard]] glm::vec2 GroundPointOffset(GroundPointSearch search, float angle, float distance);

/// The middle of the target tips of a bolt or a fork: the sum of the tips times the inverse of their count
[[nodiscard]] glm::vec3 TipsMiddle(std::span<const glm::vec3> tips);

/// Whether the land stops a fork: the line from the start of the fork through its split point meets the land (or the
/// sea) nearer across the land than the split point
[[nodiscard]] bool LandHitsFirst(glm::vec3 from, glm::vec3 split, glm::vec3 hit);

/// The square of the distance that a bolt has moved since its last search, with the squares added z first
[[nodiscard]] float MovedSquared(glm::vec3 searchOrigin, glm::vec3 origin);

/// A creature less faded away than this draws the bolt
inline constexpr float k_DrawsBoltBelowFizz = 0.5f;
/// Every one of a bolt's targets is pointed at the creatures among them in turn, when there are any: a creature in the
/// cone takes all the forks. Returns, for each target, the index of the target it becomes.
[[nodiscard]] std::vector<size_t> PreferCreatures(const std::vector<bool>& isCreature);

/// What a bolt shows another to see whether they clash
struct BoltPose
{
	glm::vec3 origin {0.0f};
	/// The middle of its targets' tips
	glm::vec3 centroid {0.0f};
	/// Where it points across the land, in radians from east towards north
	float heading {0.0f};
};
/// How far a bolt's trunk reaches towards another's middle before they meet, and the widest angle off its heading the
/// meeting may be at, in radians
inline constexpr float k_ClashShare = 0.7f;
inline constexpr float k_ClashWidestAngle = 1.036725640296936f;
/// Where a newer bolt meets an older one, if they clash: on the way between the older one's middle and its hand, pulled
/// towards the newer one's heading, within the newer one's reach and in front of it
[[nodiscard]] std::optional<glm::vec3> ClashPoint(const BoltPose& newer, const BoltPose& older, float reach);
/// The older bolt carries on past where they meet this much thicker, fully opaque
inline constexpr float k_ClashForkScale = 3.0f;
/// Each strike of a clash hits twice as hard, for both miracles
inline constexpr float k_ClashStrength = 2.0f;

/// A storm's forked bolt comes down from a point of light under its cloud, which lives this long, picked between the two;
/// its bolt goes with it
inline constexpr float k_SkyStrikeLifeMin = 0.4f;
inline constexpr float k_SkyStrikeLifeMax = 0.8f;
/// No more than this many storm bolts are alive at once; a storm's bolt due while they are is lost
inline constexpr size_t k_MostSkyStrikes = 50;
/// The size of a storm bolt's thunder, 1 large, 2 medium or 3 small, by a random number in 0..1: small below 0.3, medium
/// below 0.75, else large
[[nodiscard]] int SkyStrikeThunderSize(float random);

/// An electric arc crawling over what a bolt struck: from one point of its model to another, leaving and arriving along
/// the model's normals there, the tangents scaled by the arc's length
struct ArcEnds
{
	glm::vec3 from {0.0f};
	glm::vec3 to {0.0f};
	glm::vec3 fromNormal {0.0f};
	glm::vec3 toNormal {0.0f};
};
/// The arc's length: the distance from one end to the other
[[nodiscard]] float ArcLength(const ArcEnds& ends);
/// The arc's joints along the curve: a cubic from one end to the other with the given tangents, each joint jittered by a
/// point of the unit ball times the arc's length times the jitter
[[nodiscard]] std::vector<glm::vec3> ArcJoints(const ArcEnds& ends, glm::vec3 fromTangent, glm::vec3 toTangent,
                                               std::span<const glm::vec3> jitters, float randomFrac);
/// An end's tangent: its normal, nudged by a point of the unit ball times the random tangent share. The arc's length
/// times the tangent scale then multiplies it.
[[nodiscard]] glm::vec3 ArcTangent(glm::vec3 normal, glm::vec3 nudge, float randomTangents, float length, float scale);

} // namespace openblack::particles::maths
