/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpiritPose.h"

#include <cmath>

#include <algorithm>

#include <glm/matrix.hpp>

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
/// A sixth of a turn, the furthest the head turns each way
constexpr float k_HeadLimit = 1.04719758f;
/// What the head's angles are scaled by to be the times into its look clips
constexpr float k_HeadScale = 0.47746482f;

uint32_t TimeOf(int32_t milliseconds)
{
	return milliseconds > 0 ? static_cast<uint32_t>(milliseconds) : 0u;
}
} // namespace

SpiritRig help::spirits::MakeSpiritRig(std::span<const uint32_t> parents, std::span<const glm::mat4> rest)
{
	SpiritRig rig;
	rig.skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, rest);
	rig.rest = skeletal_animation::PosesFromBoneMatrices(rest, parents);
	return rig;
}

glm::mat4 help::spirits::ModelMatrix(const glm::mat3& rows, const glm::vec3& position)
{
	glm::mat4 model(1.0f);
	for (glm::length_t i = 0; i < 3; ++i)
	{
		model[i] = glm::vec4(rows[i], 0.0f);
	}
	model[3] = glm::vec4(position, 1.0f);
	return model;
}

std::vector<glm::mat4> help::spirits::EvaluatePose(const SpiritRig& rig, const DudeData& data,
                                                   std::span<const AnimLayer> layers, const glm::mat4& model)
{
	if (rig.Empty())
	{
		return {};
	}
	const auto clipOf = [&data](uint32_t i) -> const skeletal_animation::Animation* {
		return i < k_AnimSlots && data.clips.at(i).has_value() ? &*data.clips.at(i) : nullptr;
	};
	const auto* stand = clipOf(anim::k_Stand);
	auto poses = rig.rest;
	for (const auto& layer : layers)
	{
		switch (layer.kind)
		{
		case AnimLayer::Kind::Set:
			if (const auto* clip = clipOf(layer.clip); clip != nullptr)
			{
				skeletal_animation::SetLayer(poses, *clip, TimeOf(layer.milliseconds), rig.skeleton,
				                             layer.fill ? stand : nullptr);
			}
			break;
		case AnimLayer::Kind::SetBlend:
		{
			const auto* first = clipOf(layer.clip);
			const auto* second = clipOf(layer.clipB);
			if (first != nullptr && second != nullptr)
			{
				skeletal_animation::BlendLayers(poses, *first, TimeOf(layer.milliseconds), *second, TimeOf(layer.millisecondsB),
				                                layer.blend, rig.skeleton);
			}
			break;
		}
		case AnimLayer::Kind::Add:
			if (const auto* clip = clipOf(layer.clip); clip != nullptr)
			{
				skeletal_animation::AddLayer(poses, *clip, TimeOf(layer.milliseconds), layer.referenceKey, rig.skeleton);
			}
			break;
		case AnimLayer::Kind::ScaleEyes:
			// Each eye's bones grow or shrink, the first eye's three by one scale and the second's by the other; a bone
			// of 0 is none
			for (size_t k = 0; k < 6; ++k)
			{
				const auto bone = data.faceBones.at(k);
				if (bone == 0 || bone >= poses.size())
				{
					continue;
				}
				const float scale = k < 3 ? layer.blend : layer.blendB;
				for (auto& row : poses[bone].rotation)
				{
					for (auto& cell : row)
					{
						cell = scale * cell;
					}
				}
			}
			break;
		}
	}
	auto world = skeletal_animation::ComposeBoneMatrices(poses, rig.skeleton.parents);
	for (auto& bone : world)
	{
		bone = model * bone;
	}
	return world;
}

glm::vec2 help::spirits::HeadAngles(std::span<const glm::mat4> world,
                                    const std::array<uint32_t, helpdude::k_FaceBones>& faceBones, const glm::vec3& target)
{
	const auto firstEye = faceBones[0];
	const auto secondEye = faceBones[3];
	const auto head = faceBones[6];
	if (firstEye >= world.size() || secondEye >= world.size() || head >= world.size())
	{
		return glm::vec2(0.0f);
	}
	// The head's axes, about the point halfway between the eyes
	glm::mat4 frame = world[head];
	frame[3] = glm::vec4((glm::vec3(world[secondEye][3]) + glm::vec3(world[firstEye][3])) * 0.5f, 1.0f);
	glm::vec3 local = glm::vec3(glm::inverse(frame) * glm::vec4(target, 1.0f));
	if (local.y < 0.0f)
	{
		return glm::vec2(0.0f);
	}
	if (local != glm::vec3(0.0f))
	{
		local *= 1.0f / std::sqrt((local.x * local.x + local.y * local.y) + local.z * local.z);
	}
	const float across = std::clamp(std::atan2(local.z, local.y), -k_HeadLimit, k_HeadLimit);
	const float up = std::clamp(std::asin(std::clamp(local.x, -1.0f, 1.0f)), -k_HeadLimit, k_HeadLimit);
	return glm::vec2(across * k_HeadScale, up * k_HeadScale);
}

glm::vec3 help::spirits::Fingertip(std::span<const glm::mat4> world, const DudeData& data, const glm::vec3& fallback)
{
	if (world.empty())
	{
		return fallback;
	}
	// The root bone in the world, its axes turned by its pose and the advisor's own
	const glm::mat4& root = world.front();
	const float across = data.fingertipOffsetRow0 * data.modelSize;
	const float ahead = data.fingertipOffsetRow2 * data.modelSize;
	const glm::vec3 first = glm::vec3(root[0]) * across;
	const glm::vec3 third = glm::vec3(root[2]) * ahead;
	return (first + glm::vec3(root[3])) + third;
}
