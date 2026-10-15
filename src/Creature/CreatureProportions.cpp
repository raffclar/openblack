/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureProportions.h"

#include <algorithm>

using namespace openblack;

float creature_proportions::Factor(Growth growth, float size)
{
	return growth.atSizeZero - (growth.perSize * std::clamp(size, 0.0f, k_MaxSizeEffect));
}

std::vector<creature_proportions::BoneScale> creature_proportions::ScaledBones(const Bones& bones,
                                                                               std::span<const uint32_t> mirror, float size)
{
	std::vector<BoneScale> scales;
	const auto add = [&scales](std::optional<uint32_t> bone, float factor) {
		if (bone.has_value())
		{
			scales.push_back({.bone = *bone, .factor = factor});
		}
	};
	const auto mirrorOf = [&mirror](std::optional<uint32_t> bone) -> std::optional<uint32_t> {
		if (!bone.has_value() || *bone >= mirror.size())
		{
			return std::nullopt;
		}
		return mirror[*bone];
	};
	const auto handsAndFeet = Factor(k_HandsAndFeet, size);
	const auto eyes = Factor(k_Eyes, size);
	add(bones.head, Factor(k_Head, size));
	add(bones.rightHand, handsAndFeet);
	add(mirrorOf(bones.rightHand), handsAndFeet);
	add(bones.rightFoot, handsAndFeet);
	add(mirrorOf(bones.rightFoot), handsAndFeet);
	add(bones.rightEye, eyes);
	add(mirrorOf(bones.rightEye), eyes);
	return scales;
}

void creature_proportions::Apply(std::span<skeletal_animation::Pose> poses, std::span<const BoneScale> scales)
{
	for (const auto& scale : scales)
	{
		if (scale.bone >= poses.size())
		{
			continue;
		}
		for (auto& row : poses[scale.bone].rotation)
		{
			std::ranges::transform(row, row.begin(), [&scale](float value) { return value * scale.factor; });
		}
	}
}
