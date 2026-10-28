/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRig.h"

#include <glm/gtx/transform.hpp>

using namespace openblack;
using namespace openblack::creature;

const skeletal_animation::Animation* CreatureRig::GetAnimation(Mesh mesh, size_t index) const
{
	const auto slot = static_cast<size_t>(mesh);
	if (slot < animations.size() && index < animations.at(slot).size() && animations.at(slot)[index].has_value())
	{
		return &*animations.at(slot)[index];
	}
	const auto& base = animations.front();
	return index < base.size() && base[index].has_value() ? &*base[index] : nullptr;
}

glm::mat4 creature::PlacementMatrix(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale)
{
	auto model = glm::mat4(rotation);
	model = glm::translate(model, position * rotation);
	return glm::scale(model, scale);
}

std::optional<std::vector<uint32_t>> creature::CreatureRig::MirrorBones(size_t boneCount) const
{
	if (boneCount == 0 || fileTail.size() < boneCount)
	{
		return std::nullopt;
	}
	std::vector<uint32_t> mirror(boneCount);
	const auto first = fileTail.size() - boneCount;
	for (size_t i = 0; i < boneCount; ++i)
	{
		const auto value = fileTail[first + i];
		if (value < 0 || static_cast<size_t>(value) >= boneCount)
		{
			return std::nullopt;
		}
		mirror[i] = static_cast<uint32_t>(value);
	}
	// In a version 20 file the first bone's mirror, when it isn't itself, is the second's
	constexpr uint32_t k_ShiftedVersion = 20;
	if (creatureVersion == k_ShiftedVersion && boneCount > 1 && mirror[0] != 0)
	{
		mirror[1] = mirror[0];
		mirror[0] = 0;
	}
	return mirror;
}

glm::mat4 creature::PosedBone(uint32_t bone, std::span<const glm::mat4> boneMatrices, const glm::mat4& placement)
{
	return bone < boneMatrices.size() ? placement * boneMatrices[bone] : placement;
}

creature::RulePose creature::TurnPose(std::span<const glm::mat4> newerTurnPose, std::span<const glm::mat4> boneMatrices,
                                      const glm::mat4& placement)
{
	if (!newerTurnPose.empty() && newerTurnPose.size() == boneMatrices.size())
	{
		return {.bones = newerTurnPose, .placement = glm::mat4(1.0f)};
	}
	return {.bones = boneMatrices, .placement = placement};
}

std::vector<glm::vec3> creature::TurnPoseBones(std::span<const glm::vec3> lastTurnBones,
                                               std::span<const glm::mat4> boneMatrices, const glm::mat4& placement)
{
	if (!lastTurnBones.empty() && lastTurnBones.size() == boneMatrices.size())
	{
		return {lastTurnBones.begin(), lastTurnBones.end()};
	}
	std::vector<glm::vec3> bones;
	bones.reserve(boneMatrices.size());
	for (uint32_t bone = 0; bone < boneMatrices.size(); ++bone)
	{
		bones.emplace_back(PosedBone(bone, boneMatrices, placement)[3]);
	}
	return bones;
}

namespace
{
/// A triangle of the body in the world, each vertex placed by the world matrix of its bone that `posedBone` gives
template <typename PosedBoneOf>
std::array<glm::vec3, 3> PlaceTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
                                       const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph,
                                       const creature::PlacingOrder& order, PosedBoneOf&& posedBone)
{
	const auto evilGood = morph.evilGood < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
	const auto thinFat = morph.thinFat < 0.0f ? CreatureRig::Mesh::Thin : CreatureRig::Mesh::Fat;
	const auto weakStrong = morph.weakStrong < 0.0f ? CreatureRig::Mesh::Weak : CreatureRig::Mesh::Strong;
	std::array<glm::vec3, 3> world {};
	for (size_t i = 0; i < world.size(); ++i)
	{
		const auto vertexOf = [&vertices, i](CreatureRig::Mesh mesh) { return vertices.at(static_cast<size_t>(mesh)).at(i); };
		const auto local = creature_morph::Blend(vertexOf(CreatureRig::Mesh::Base), vertexOf(evilGood), vertexOf(thinFat),
		                                         vertexOf(weakStrong), morph);
		world.at(i) = x87::TransformPoint(posedBone(bones.at(i)), local, order.at(i));
	}
	return world;
}
} // namespace

std::array<glm::vec3, 3> creature::PosedTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
                                                 const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph,
                                                 std::span<const glm::mat4> boneMatrices, const glm::mat4& placement,
                                                 const PlacingOrder& order)
{
	return PlaceTriangle(vertices, bones, morph, order,
	                     [&](uint32_t bone) { return PosedBone(bone, boneMatrices, placement); });
}

creature::PosedBones::PosedBones(std::span<const glm::mat4> boneMatrices, const glm::mat4& placement)
    : _boneMatrices(boneMatrices)
    , _placement(placement)
{
}

glm::mat4 creature::PosedBones::At(uint32_t bone)
{
	if (bone >= _known.size())
	{
		return PosedBone(bone, _boneMatrices, _placement);
	}
	if (!_known.at(bone))
	{
		_world.at(bone) = PosedBone(bone, _boneMatrices, _placement);
		_known.at(bone) = true;
	}
	return _world.at(bone);
}

std::array<glm::vec3, 3> creature::PosedTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
                                                 const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph,
                                                 PosedBones& posed, const PlacingOrder& order)
{
	return PlaceTriangle(vertices, bones, morph, order, [&posed](uint32_t bone) { return posed.At(bone); });
}
