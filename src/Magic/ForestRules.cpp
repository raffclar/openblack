/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ForestRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A tree at the spiral's end grows to half the size of one in the middle
constexpr float k_EdgeShrink = 0.5f;
} // namespace

glm::vec2 forest::SpiralOffset(uint32_t index, uint32_t count)
{
	// Calculate the step and the turn between two trees one time for all the trees
	const float step = count > 1 ? 1.0f / static_cast<float>(count - 1) : 1.0f;
	const float turn = (static_cast<float>(count) * k_TurnsPerTree) * k_TwoPi;
	const float t = static_cast<float>(index) * step;
	const float angle = t * turn;
	const float out = 1.0f - t;
	const float radius = k_InnerRadius + ((k_OuterRadius - k_InnerRadius) * std::sqrt(std::max(1.0f - (out * out), 0.0f)));
	// Round the cosine to a float before it multiplies the radius. The sine multiplies the radius at full width.
	return {x87::RoundToFloat(x87::Cos(angle)) * radius, x87::Mul(x87::Sin(angle), radius)};
}

float forest::TargetScale(float distance)
{
	return 1.0f - (k_EdgeShrink * distance / k_OuterRadius);
}

map_coords::MapCoords forest::TreeSpot(glm::vec2 centre, glm::vec2 offset)
{
	return {.x = map_coords::ToFixed(centre.x + offset.x), .z = map_coords::ToFixed(centre.y + offset.y), .altitude = 0.0f};
}

float forest::TargetScaleAt(const map_coords::MapCoords& centre, const map_coords::MapCoords& spot)
{
	return TargetScale(gutils::GetDistanceInMetres(centre, spot));
}

uint32_t forest::TreesWeCanAfford(int maxObjectsToCreate, uint32_t finalTrees, float strength)
{
	const auto most = maxObjectsToCreate != -1 ? static_cast<uint32_t>(std::max(maxObjectsToCreate, 0)) : finalTrees;
	return strength > 0.0f ? most : 0;
}

uint32_t forest::MaxObjectsToCreate(int maxObjectsToCreate, uint32_t finalTrees, bool planted, std::optional<uint32_t> trees)
{
	const uint32_t made = trees.has_value() ? *trees : (planted ? 0 : finalTrees);
	const auto limit = maxObjectsToCreate != -1 ? static_cast<uint32_t>(std::max(maxObjectsToCreate, 0)) : finalTrees;
	return std::min(made, limit);
}

TreeInfo forest::SpeciesFor(const std::array<TreeInfo, 4>& kinds, uint32_t roll)
{
	return kinds.at(std::min<size_t>(roll, kinds.size() - 1));
}

float forest::Grow(float scale, float amount, float target)
{
	return std::min(scale + amount, target);
}

std::optional<float> forest::Wither(float scale, float amount)
{
	const float after = scale - amount;
	if (!(after > 0.0f))
	{
		return std::nullopt;
	}
	return after;
}
