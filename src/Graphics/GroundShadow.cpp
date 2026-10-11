/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GroundShadow.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{

namespace
{
/// The sun's way across the ground to where the villager stands, of length 1
glm::vec2 SunWay(const glm::vec3& standsAt)
{
	const auto way = glm::vec2(standsAt.x - GroundShadow::k_Sun.x, standsAt.z - GroundShadow::k_Sun.z);
	const auto length = glm::length(way);
	return length > 0.0f ? way / length : glm::vec2(0.0f);
}
} // namespace

uint8_t GroundShadow::Alpha(float cameraDistance, float radius)
{
	if (radius <= 0.0f)
	{
		return 0;
	}
	const auto radii = cameraDistance / radius;
	if (radii < k_FadeStart)
	{
		return 255;
	}
	if (radii <= k_FadeEnd)
	{
		return static_cast<uint8_t>(255.0f - ((radii - k_FadeStart) * 255.0f) / (k_FadeEnd - k_FadeStart));
	}
	return 0;
}

glm::vec2 GroundShadow::OnGround(const glm::vec3& point, float feet)
{
	// Nothing of the body is lower than its feet. Worked out at double precision: the sun is so far off that single
	// precision would lose a 32nd of a unit on the way.
	const auto height = static_cast<double>(std::max(point.y - feet, 0.0f));
	const auto sun = glm::dvec3(k_Sun);
	const auto along = sun.y / (sun.y - height);
	return {static_cast<float>(sun.x + ((static_cast<double>(point.x) - sun.x) * along)),
	        static_cast<float>(sun.z + ((static_cast<double>(point.z) - sun.z) * along))};
}

float GroundShadow::Reach(const glm::vec3& point, const glm::vec3& standsAt)
{
	return glm::dot(glm::vec2(point.x, point.z), SunWay(standsAt));
}

std::optional<GroundShadow::Footprint> GroundShadow::Measure(std::span<const glm::vec3> points, const glm::vec3& standsAt)
{
	if (points.empty())
	{
		return std::nullopt;
	}
	Footprint footprint {.low = glm::vec2(std::numeric_limits<float>::max()),
	                     .high = glm::vec2(std::numeric_limits<float>::lowest()),
	                     .nearest = std::numeric_limits<float>::max()};
	for (const auto& point : points)
	{
		const auto flat = OnGround(point, standsAt.y);
		footprint.low = glm::min(footprint.low, flat);
		footprint.high = glm::max(footprint.high, flat);
		footprint.nearest = std::min(footprint.nearest, Reach(point, standsAt));
	}
	if (footprint.high.x <= footprint.low.x || footprint.high.y <= footprint.low.y)
	{
		return std::nullopt;
	}
	return footprint;
}

glm::vec2 GroundShadow::Sample(const glm::vec2& onGround, const Footprint& footprint)
{
	const auto scale = glm::vec2(k_SampleColumns, k_SampleRows) / (footprint.high - footprint.low);
	const auto sample = (onGround - footprint.low) * scale;
	const auto lastColumn = static_cast<float>(k_SampleColumns - 1);
	const auto lastRow = static_cast<float>(k_SampleRows - 1);
	const auto across = sample.x < 0.0f ? 0.0f : (sample.x >= lastColumn ? lastColumn : sample.x);
	const auto down = sample.y < 0.0f ? 0.0f : (sample.y > lastRow ? lastRow : sample.y);
	return {across, std::floor(down)};
}

bool GroundShadow::Counted(const std::array<glm::vec2, 3>& corners)
{
	const auto& [first, second, third] = corners;
	return (first.x - third.x) * (second.y - third.y) <= (second.x - third.x) * (first.y - third.y);
}

uint8_t GroundShadow::Level(uint8_t covered, uint8_t alpha)
{
	if (alpha == 255)
	{
		return covered;
	}
	// The level sits in the top four bits of a 16 bit texel, which is scaled by the opacity and cut back to them
	const auto texel = static_cast<uint32_t>(covered) << 12U;
	return static_cast<uint8_t>(((texel * alpha) / 255U) >> 12U);
}

bool GroundShadow::OnBorder(uint8_t column, uint8_t row)
{
	return column == 0 || row == 0 || column >= k_Texels - 1 || row >= k_Texels - 1;
}

glm::vec2 GroundShadow::SlotClip(const glm::vec2& sample, uint8_t slot, uint8_t slots, bool originBottomLeft)
{
	// The game counts a sample when the triangle's row, taken at its top, spans past it, the triangle's corners rounded
	// down to whole rows. A texel is covered where its centre is inside: moving the corners half a texel left and down
	// makes those the same.
	const auto column = (static_cast<float>(slot) * k_SampleColumns) + sample.x - 0.5f;
	const auto row = sample.y + 0.5f;
	const auto width = static_cast<float>(std::max<uint8_t>(slots, 1)) * k_SampleColumns;
	const auto x = -1.0f + (2.0f * column / width);
	const auto y = 2.0f * row / static_cast<float>(k_SampleRows);
	return {x, originBottomLeft ? y - 1.0f : 1.0f - y};
}

glm::mat4 GroundShadow::ReceiverMatrix(const Footprint& footprint, const glm::vec3& standsAt, float ground)
{
	// The land is flattened onto the level of the feet along the sun's rays as if it all stood at the ground's height
	const auto sun = glm::dvec3(k_Sun);
	const auto along = (static_cast<double>(standsAt.y) - sun.y) / (static_cast<double>(ground) - sun.y);
	const auto low = glm::dvec2(footprint.low);
	const auto size = glm::dvec2(footprint.high) - low;
	const auto way = SunWay(standsAt);
	auto matrix = glm::mat4(0.0f);
	matrix[0][0] = static_cast<float>(along / size.x);
	matrix[3][0] = static_cast<float>(((sun.x * (1.0 - along)) - low.x) / size.x);
	matrix[2][1] = static_cast<float>(along / size.y);
	matrix[3][1] = static_cast<float>(((sun.z * (1.0 - along)) - low.y) / size.y);
	matrix[0][2] = way.x;
	matrix[2][2] = way.y;
	matrix[3][3] = 1.0f;
	return matrix;
}

} // namespace openblack::graphics
