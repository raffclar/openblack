/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAlignmentMotes.h"

namespace openblack::creature_alignment_motes
{
namespace
{
using physics::turn::PuffKind;

/// A start point near a random bone. The offsets come first on x, then on y, then on z, and the bone comes last.
glm::vec3 StartPoint(float shownSize, uint32_t boneCount, const BonePoint& bonePoint, const LocalRandom& random)
{
	const auto offset = [&random]() {
		return static_cast<float>(static_cast<int32_t>(random(k_SpreadRandomRange)) - k_SpreadRandomMiddle) * k_SpreadStep;
	};
	const float x = offset();
	const float y = offset();
	const float z = offset();
	const float spread = shownSize * k_SpreadPerSize;
	const auto bone = random(static_cast<int32_t>(boneCount));
	return bonePoint(bone) + glm::vec3(x, y, z) * spread;
}

/// The alpha byte of an alpha value: the fraction is dropped
uint32_t AlphaOf(float alpha)
{
	return static_cast<uint32_t>(static_cast<int32_t>(alpha)) << 24u;
}
} // namespace

std::vector<Puff> PuffsOf(float alignment, float shownSize, uint32_t boneCount, const BonePoint& bonePoint,
                          const LocalRandom& random)
{
	std::vector<Puff> puffs;
	if (boneCount == 0 || !MakesAny(alignment))
	{
		return puffs;
	}
	puffs.reserve(k_PerTurn);
	if (alignment > k_GoodAbove)
	{
		for (int32_t i = 0; i < k_PerTurn; ++i)
		{
			const auto position = StartPoint(shownSize, boneCount, bonePoint, random);
			const auto alpha = AlphaOf(alignment * k_MoteAlphaPerAlignment);
			const auto colour = k_MoteColours.at(random(static_cast<int32_t>(k_MoteColours.size())));
			const auto kind = random(2) != 0 ? PuffKind::Mote : PuffKind::MoteReversed;
			puffs.push_back({
			    .position = position,
			    .velocity = glm::vec3(0.0f),
			    .size = shownSize * k_MoteSizePerSize,
			    .argb = alpha + colour,
			    .kind = kind,
			});
		}
		return puffs;
	}
	const auto alpha = AlphaOf(-alignment * k_SmokeAlphaPerAlignment);
	for (int32_t i = 0; i < k_PerTurn; ++i)
	{
		const auto position = StartPoint(shownSize, boneCount, bonePoint, random);
		const auto colour = k_SmokeColours.at(random(static_cast<int32_t>(k_SmokeColours.size())));
		puffs.push_back({
		    .position = position,
		    .velocity = k_SmokeVelocity,
		    .size = shownSize * k_SmokeSizePerSize,
		    .argb = alpha + colour,
		    .kind = PuffKind::Smoke,
		});
	}
	return puffs;
}

} // namespace openblack::creature_alignment_motes
