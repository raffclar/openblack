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

#include <optional>
#include <span>
#include <vector>

#include "3D/SkeletalAnimation.h"

/// How the body of a young creature is not a small adult. A small creature has a large head, large hands and feet and
/// large eyes for its size. Each part shrinks in a line as the creature grows, until size 2. At size 2 the hands, feet
/// and eyes are of normal size and the head is smaller than normal.
namespace openblack::creature_proportions
{
/// How a part of the body changes with the size of the creature: its factor at size 0, and how much the factor falls
/// for each unit of size
struct Growth
{
	float atSizeZero;
	float perSize;
};

/// Sizes above this make the parts no smaller for their size
constexpr float k_MaxSizeEffect = 2.0f;
/// The head, from 1.8 at size 0 to 1 at size 1.6 and 0.8 at size 2
constexpr Growth k_Head {.atSizeZero = 1.8f, .perSize = 0.5f};
/// The hands and feet, from 1.5 at size 0 to 1 at size 2
constexpr Growth k_HandsAndFeet {.atSizeZero = 1.5f, .perSize = 0.25f};
/// The eyes, from 1.4 at size 0 to 1 at size 2
constexpr Growth k_Eyes {.atSizeZero = 1.4f, .perSize = 0.2f};

/// The factor that a part of the body is scaled by, for a creature of a size
[[nodiscard]] float Factor(Growth growth, float size);

/// The bones of a species that change with its size. Each one on the right side also has a mirror bone on the left.
struct Bones
{
	/// The bone that carries the head, the bone that the leash attaches to
	std::optional<uint32_t> head;
	std::optional<uint32_t> rightHand;
	std::optional<uint32_t> rightFoot;
	std::optional<uint32_t> rightEye;
};

/// A bone and the factor that its pose is scaled by
struct BoneScale
{
	uint32_t bone;
	float factor;
};

/// The bones to scale for a creature of a size, and their factors. The list has each bone on the right side and then
/// its mirror bone. A bone that is its own mirror is in the list two times, and it gets the factor two times.
[[nodiscard]] std::vector<BoneScale> ScaledBones(const Bones& bones, std::span<const uint32_t> mirror, float size);

/// Scale the rotation of each listed bone, relative to its parent, by its factor. The bones below it grow with it. The
/// position of the bone itself does not change.
void Apply(std::span<skeletal_animation::Pose> poses, std::span<const BoneScale> scales);
} // namespace openblack::creature_proportions
