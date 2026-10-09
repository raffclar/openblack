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

#include <functional>
#include <span>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The simple beam's maths: a few key points on the straight way from the beam's start to its end, the ones between the
// ends pushed aside by smooth noise that drifts along the beam over time and bulges most at its middle, kept above the
// land; then a smooth curve through them, with the ribbon's joints spread evenly along it, thickest at the middle. Free
// of state, so they are tested on their own.

namespace openblack::particles::maths
{

/// How a beam's key points wiggle
struct BeamWiggle
{
	/// How many times the noise changes along the beam
	float frequency {4.0f};
	/// How fast the noise drifts along the beam, a second
	float speed {1.0f};
	/// How far the key points are pushed aside
	float amount {0.1f};
	/// The key points between the ends keep at least this high above the land
	float minHeight {0.0f};
};

/// The noise across the beam drifts at the wiggle's speed, the noise across it the other way at this share of it, and
/// the noise upwards at this share
inline constexpr float k_BeamDepthDriftShare = 0.7f;
inline constexpr float k_BeamHeightDriftShare = 1.3f;
/// The upward push is half the sideways one, and only ever upwards
inline constexpr float k_BeamHeightShare = 0.5f;

/// How much of the push a point a share t of the way along the beam takes: none at the ends, all of it at the middle
[[nodiscard]] float BeamBulge(float t);

/// The beam's key points from its start to its end, count of them evenly along the way (count is at least two). The
/// ends are the start and the end themselves. Each point between is pushed sideways and upwards by the noise at the
/// point's share of the way times the frequency, plus the beam's age times the speed (times the shares above), plus
/// the beam's number so that no two beams wiggle alike; then raised to the minimum height above the land if under it.
[[nodiscard]] std::vector<glm::vec3> BeamKeyPoints(glm::vec3 start, glm::vec3 end, int count, const BeamWiggle& wiggle,
                                                   float age, int beam, const std::function<float(float)>& noise,
                                                   const std::function<float(glm::vec2)>& landHeight);

/// One of the ribbon's joints
struct BeamJoint
{
	glm::vec3 position {0.0f};
	float scale {1.0f};
};

/// The ribbon's joints along a smooth curve through the key points, count of them evenly in the curve's own parameter
/// from the beam's start to its end. The curve passes through each key point at its share of the way and leaves and
/// arrives flat at both ends. Each joint's scale goes from the smallest at the ends to the largest at the middle.
[[nodiscard]] std::vector<BeamJoint> BeamJoints(std::span<const glm::vec3> keys, size_t count, float minScale, float maxScale);

// The plasma beam a temple heart fires: a curve that leaves its start along one tangent and arrives at its end along
// another, its key points between the ends wiggled by the same drifting noise, faded in and out over its life

/// A beam's alpha at an age: none at its birth and at the end of its life, its alpha times the rule's most at the middle
/// of its life, as a whole number scaled into 0..255 and cut down to a byte
[[nodiscard]] uint8_t PlasmaAlpha(float age, float life, uint8_t alpha, int32_t maxAlpha);

/// A cubic curve from one point to another, given by its coefficients: the point a share t of the way along is
/// ((c3 t^3 + c2 t^2) + c1 t) + c0
struct HermiteCurve
{
	glm::vec3 c3 {0.0f};
	glm::vec3 c2 {0.0f};
	glm::vec3 c1 {0.0f};
	glm::vec3 c0 {0.0f};
};

/// The curve that starts at start along startTangent and arrives at end along endTangent
[[nodiscard]] HermiteCurve PlasmaCurve(glm::vec3 start, glm::vec3 end, glm::vec3 startTangent, glm::vec3 endTangent);
[[nodiscard]] glm::vec3 At(const HermiteCurve& curve, float t);

/// The tangents a beam is laid with this step: each nudged by a point of the unit ball times the random share, then both
/// made as long as the beam times the scale
[[nodiscard]] std::pair<glm::vec3, glm::vec3> PlasmaTangents(glm::vec3 start, glm::vec3 end, glm::vec3 startTangent,
                                                             glm::vec3 endTangent, glm::vec3 startNudge, glm::vec3 endNudge,
                                                             float randomShare, float scale);

/// A plasma beam's key points, count of them evenly along the curve. Each point between the ends is pushed sideways
/// and upwards by the noise at its share of the way times the frequency, the beam's number and its drift (its age times
/// its speed times the wiggle's speed, times the shares above across and up)
[[nodiscard]] std::vector<glm::vec3> PlasmaKeyPoints(const HermiteCurve& curve, int count, float frequency, float amount,
                                                     float drift, int beam, const std::function<float(float)>& noise);

} // namespace openblack::particles::maths
