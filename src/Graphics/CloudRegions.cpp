/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CloudRegions.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack::graphics;

namespace
{

/// How much each unit of the billows changes the cover, as a share of the cover
constexpr float k_CoverContrast = 2.4f;
/// The least and the most share of the cover in a region
constexpr float k_LeastShare = 0.03f;
constexpr float k_MostShare = 2.5f;
/// The edge of a cell goes over this part of the field for each width of a cell in one pixel
constexpr float k_EdgePerPixel = 1.2f;
constexpr float k_SoftestEdge = 1.5f;

float Smooth(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

} // namespace

float cloud_regions::RegionShare(float distance)
{
	return Smooth(k_KeepNear, k_KeepFar, distance);
}

glm::vec3 cloud_regions::SizeWeights(float size)
{
	const float s = std::clamp(size, 0.0f, 1.0f);
	if (s < 0.5f)
	{
		return {1.0f - 2.0f * s, 2.0f * s, 0.0f};
	}
	return {0.0f, 2.0f - 2.0f * s, 2.0f * s - 1.0f};
}

float cloud_regions::MixPatterns(glm::vec3 values, glm::vec3 weights)
{
	const float length = std::sqrt(glm::dot(weights, weights));
	if (length <= 0.0f)
	{
		return 0.5f;
	}
	return std::clamp(0.5f + glm::dot(weights, values - 0.5f) / length, 0.0f, 1.0f);
}

float cloud_regions::CellWidth(glm::vec3 weights)
{
	return glm::dot(weights, glm::vec3 {k_SmallCells, 1.0f, k_LargeCells}) * k_UsualCellWidth;
}

float cloud_regions::EdgeSoftness(float pixel, float cellWidth)
{
	return std::clamp(k_EdgePerPixel * pixel / std::max(cellWidth, 1.0f), 0.0f, k_SoftestEdge);
}

float cloud_regions::OpenRing(float field)
{
	return 1.0f - Smooth(0.0f, k_RingWidth, std::abs(field - 0.5f));
}

float cloud_regions::CoverShare(float billows, float regionShare)
{
	return std::clamp(1.0f + k_CoverContrast * billows * regionShare, k_LeastShare, k_MostShare);
}
