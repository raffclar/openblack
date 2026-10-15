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

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <GlbFile.h>
#include <L3DFile.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/SkeletalAnimation.h"

/// EXPERIMENT: turns a skinned, animated glTF model into a creature body: a boned mesh as the game draws its creatures
/// and keyframe animations as the game plays them. This is no part of the original game.
///
/// glTF's axes are right-handed and a creature in a glTF file faces +z. The game's axes are left-handed and a creature
/// faces -z. Every point and matrix is changed by turning z round.
namespace openblack::gltf_creature
{

/// The id of the skin the converted mesh is drawn with
constexpr uint32_t k_SkinId = 0xDEAD0001;

/// The bone each joint of the skin becomes. A joint named "bone_<n>..." becomes bone n, as the l3dtool export names
/// them, so that the bones keep the numbers the species' files give them. When the names do not give each joint its
/// own bone from 0, the joints keep the skin's order.
[[nodiscard]] std::vector<uint32_t> BonesOfJoints(const glb::Model& model);

/// The bones' rest pose in the game's axes: each bone's parent (or none) and its matrix relative to the parent
struct Rest
{
	std::vector<uint32_t> parents;
	std::vector<glm::mat4> locals;

	/// Each bone's matrix in the space of the mesh
	[[nodiscard]] std::vector<glm::mat4> World() const;
};

/// The rest pose of the skin's joints as the nodes give it, with bones numbered as BonesOfJoints gives them. None when
/// a bone's parent does not come before it.
[[nodiscard]] std::optional<Rest> RestOf(const glb::Model& model, std::span<const uint32_t> bones);

/// A boned mesh with one submesh and one textured primitive, drawn with the skin given. Each vertex is moved by the bone
/// of its heaviest joint. A vertex with a second joint gets a partner vertex, in no triangle, moved by that joint's bone,
/// and is blended towards it by the second joint's share of the two weights, at most a half.
[[nodiscard]] l3d::L3DFile BuildMesh(const glb::Model& model, std::span<const uint32_t> bones, const Rest& rest,
                                     const l3d::L3DTexture& skin);

/// A node's matrix relative to its parent at a time of an animation, in the file's axes. Properties the animation does
/// not move keep the node's own.
[[nodiscard]] glm::mat4 SampleLocal(const glb::Model& model, const glb::Animation& animation, uint32_t node, float seconds);

/// How an animation is played: its length, how many keyframes it gets, whether it repeats, and how far it moves the
/// creature in a cycle
struct ClipTiming
{
	uint32_t durationMs;
	size_t frames;
	bool looping;
	glm::vec3 displacement {0.0f};
};

/// A glTF animation as the game's keyframes of every bone, played over timing.durationMs whatever the file's length. A
/// cycle's keyframes are spread over its length and wrap from the last to the first; a pose range's run from its start
/// to its end.
[[nodiscard]] skeletal_animation::Animation ToAnimation(const glb::Model& model, const glb::Animation& animation,
                                                        std::span<const uint32_t> bones,
                                                        const skeletal_animation::Skeleton& skeleton, const ClipTiming& timing);

/// For each name of the species' animations, the glTF animation of that name (without regard to case), or none
[[nodiscard]] std::vector<std::optional<size_t>> MatchClips(std::span<const std::string> names,
                                                            std::span<const glb::Animation> animations);

/// A skin from an image of 8-bit RGBA texels, each skin texel the average of the image texels it covers. A skin is too
/// big to keep on the stack.
[[nodiscard]] std::unique_ptr<l3d::L3DTexture> SkinFromRgba(std::span<const uint8_t> rgba, uint32_t width, uint32_t height,
                                                            uint32_t id);

} // namespace openblack::gltf_creature
