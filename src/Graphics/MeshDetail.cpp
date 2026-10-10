/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MeshDetail.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

namespace openblack::graphics::mesh_detail
{

float ViewDepth(const glm::vec3& point, const glm::vec3& eye, const glm::vec3& forward) noexcept
{
	return glm::dot(point - eye, forward);
}

float ScaledRadius(const glm::vec3& boxSize, float scale) noexcept
{
	return glm::length(boxSize) * 0.5f * scale;
}

float Reach(float importance, float scaledRadius, float modelDetail) noexcept
{
	return std::min((importance + 1.0f) * scaledRadius * modelDetail, k_MostReach);
}

Choice Choose(float depth, float reach, bool disappears) noexcept
{
	if (depth < k_StandardFrom * reach)
	{
		return {.mesh = Mesh::High, .alpha = std::nullopt};
	}
	if (depth < k_LowFrom * reach)
	{
		return {.mesh = Mesh::Standard, .alpha = std::nullopt};
	}
	const float fadeFrom = k_FadeFrom * reach;
	if (depth < fadeFrom)
	{
		return {.mesh = Mesh::Low, .alpha = std::nullopt};
	}
	const float goneFrom = k_GoneFrom * reach;
	if (depth < goneFrom)
	{
		if (!disappears)
		{
			return {.mesh = Mesh::Low, .alpha = std::nullopt};
		}
		// Whole where the fading starts and gone where it ends, rounded to the nearest step
		const float span = goneFrom - fadeFrom;
		const float alpha = std::nearbyint(((span - (depth - fadeFrom)) * 255.0f) / span);
		return {.mesh = Mesh::Low, .alpha = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 255.0f))};
	}
	if (!disappears)
	{
		return {.mesh = Mesh::Low, .alpha = std::nullopt};
	}
	return {.mesh = std::nullopt, .alpha = std::nullopt};
}

float VillagerImportance(uint32_t ageWhenMade) noexcept
{
	return static_cast<float>(ageWhenMade) * 0.001f;
}

} // namespace openblack::graphics::mesh_detail
