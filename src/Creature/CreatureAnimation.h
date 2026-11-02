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

#include <span>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/SkeletalAnimation.h"

/// How a creature's animations follow its body. A species has animations for its base mesh, and may have its own for
/// the evil, good, thin and fat meshes. A creature's animation is the base's pulled towards the evil or good one and
/// the thin or fat one by as much as its body is, as its rest pose is. Weak and strong bodies move as the base does.
namespace openblack::creature_animation
{
using skeletal_animation::Animation;

/// The animation of the stand pose, the first of a species' animations, which the others fall back on
constexpr size_t k_StandAnimation = 0;

/// The rotation matrix of each rotated joint in each frame of the animation, frame after frame. Many blends use the same
/// animation, so they can share these matrices and not make them again.
[[nodiscard]] std::vector<skeletal_animation::Matrix> FrameTurns(const Animation& animation);

/// A variant the species has no animation of its own for moves as the base does, turned and moved by how its stand pose
/// differs from the base's
[[nodiscard]] Animation AdjustFromStand(const Animation& animation, const Animation& baseStand, const Animation& variantStand);

/// The same adjustment with the frame turns of the animation that FrameTurns made. The result has the same bits.
[[nodiscard]] Animation AdjustFromStand(const Animation& animation, std::span<const skeletal_animation::Matrix> turns,
                                        const Animation& baseStand, const Animation& variantStand);

/// One side of a blend: the variant's animation and its stand pose, and how far towards it
struct BlendTerm
{
	const Animation* animation;
	const Animation* stand;
	float weight;
	/// The frame turns of the animation that FrameTurns made, or none. With none, the blend makes them.
	std::span<const skeletal_animation::Matrix> turns {};
};

/// The base animation pulled towards an evil or good and a thin or fat animation, keyframe by keyframe. Rotations blend
/// on their matrices' elements. A bone a variant doesn't move takes the first frame of that variant's stand. The
/// blend keeps the base's keyframes and length.
[[nodiscard]] Animation Blend(const Animation& base, const BlendTerm& evilGood, const BlendTerm& thinFat);

/// The same blend with the frame turns of the base that FrameTurns made. The result has the same bits.
[[nodiscard]] Animation Blend(const Animation& base, std::span<const skeletal_animation::Matrix> baseTurns,
                              const BlendTerm& evilGood, const BlendTerm& thinFat);

/// Two animations of a move blended into one, keyframe by keyframe, as a turn on the spot or a step off blends its
/// pair of 0, 90 and 180 degree animations for the angle. Each keyframe's rotations blend on their matrices' elements
/// and are turned back into angles, so the blend plays as true rotations however far apart the two turn the body; its
/// translations blend straight. A bone only one of them moves takes the first frame of the stand in the other. The
/// blend has the first's keyframes and looping, its length rounded between theirs, and its move the way between
/// theirs, as long as the length between their moves.
[[nodiscard]] Animation BlendPair(const Animation& from, const Animation& to, const Animation& stand, float weight);

/// A move between two: the direction of the blended move, as long as the blend of their lengths
[[nodiscard]] glm::vec3 BlendMoves(const glm::vec3& from, const glm::vec3& to, float weight);

/// The rest pose's matrices, in the mesh's space, pulled the same way
[[nodiscard]] std::vector<glm::mat4> BlendRest(std::span<const glm::mat4> base, std::span<const glm::mat4> evilGood,
                                               float evilGoodWeight, std::span<const glm::mat4> thinFat, float thinFatWeight);

/// A creature breathes once every five seconds at size 1, more slowly the bigger it is: in seconds
[[nodiscard]] float BreathPeriod(float size);
/// The period breathing is at when a walk starts: three seconds at size 1, or 1.4 seconds when the walk is at the run
/// speed, by the square root of the size
[[nodiscard]] float WalkBreathPeriod(float size, bool running);
/// How far through a breath it is, 0 to 1, after some seconds more
[[nodiscard]] float AdvanceBreath(float phase, float seconds, float period);
/// The period breathing is at after one step towards the one it should be at. It settles back to its resting period
/// with ten seconds' worth of game turns and changes to any other period with half a second's worth. Each step closes
/// that share of the gap. A creature that does not breathe yet starts at its target.
[[nodiscard]] float EaseBreathPeriod(float current, float target, float restingPeriod, float turnSeconds);
/// The time in a looping animation of the stand pose that a point in the breath shows
[[nodiscard]] uint32_t BreathTime(float phase, uint32_t duration);
} // namespace openblack::creature_animation
