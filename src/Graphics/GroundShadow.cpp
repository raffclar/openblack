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
#include <bit>
#include <limits>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::graphics
{

float GroundShadow::Fade(float cameraDistance, float radius)
{
	if (radius <= 0.0f)
	{
		return 0.0f;
	}
	const auto radii = cameraDistance / radius;
	if (radii < k_FadeStart)
	{
		return 255.0f;
	}
	if (radii <= k_FadeEnd)
	{
		return 255.0f - ((radii - k_FadeStart) * 255.0f) / (k_FadeEnd - k_FadeStart);
	}
	return 0.0f;
}

uint8_t GroundShadow::Alpha(float cameraDistance, float radius)
{
	return static_cast<uint8_t>(Fade(cameraDistance, radius));
}

uint8_t GroundShadow::ScaledAlpha(float fade)
{
	if (fade >= 255.0f)
	{
		return 255;
	}
	// A 255th as a single-precision number is a hair over the true one
	constexpr auto k_Step = static_cast<double>(1.0f / 255.0f);
	return static_cast<uint8_t>(std::clamp(255.0 * static_cast<double>(fade) * k_Step, 0.0, 255.0));
}

glm::vec3 GroundShadow::CreatureLight(const glm::vec3& sceneLight, const glm::vec3& standsAt, float radius)
{
	auto towards = sceneLight - standsAt;
	const auto nearest = k_CreatureLightReach * radius;
	auto across = std::sqrt((towards.x * towards.x) + (towards.z * towards.z));
	if (across < nearest)
	{
		if (static_cast<double>(across) < k_CreatureLightNudge)
		{
			towards.x += 1.0f;
			towards.z += 1.0f;
		}
		if (towards != glm::vec3(0.0f))
		{
			towards *= nearest / glm::length(towards);
		}
		across = std::sqrt((towards.x * towards.x) + (towards.z * towards.z));
	}
	// No lower than 45 degrees
	if (towards.y / across < 1.0f)
	{
		towards.y = across;
	}
	return standsAt + towards;
}

glm::vec2 GroundShadow::OnLevel(const Point& point, const Cast& cast)
{
	// Worked out at double precision: a far light would lose a 32nd of a unit on the way at single precision
	auto height = static_cast<double>(point.position.y - cast.level);
	if (point.keptAboveLevel)
	{
		height = std::max(height, 0.0);
	}
	const auto light = glm::dvec3(cast.light);
	const auto along = light.y / (light.y - height);
	return {static_cast<float>(light.x + ((static_cast<double>(point.position.x) - light.x) * along)),
	        static_cast<float>(light.z + ((static_cast<double>(point.position.z) - light.z) * along))};
}

glm::vec2 GroundShadow::LightWay(const glm::vec3& standsAt, const glm::vec3& light)
{
	const auto way = glm::vec2(standsAt.x - light.x, standsAt.z - light.z);
	const auto length = glm::length(way);
	return length > 0.0f ? way / length : glm::vec2(0.0f);
}

float GroundShadow::Reach(const glm::vec3& point, const glm::vec3& standsAt, const glm::vec3& light)
{
	return glm::dot(glm::vec2(point.x, point.z), LightWay(standsAt, light));
}

std::optional<GroundShadow::Footprint> GroundShadow::Measure(std::span<const Point> points, const glm::vec3& standsAt,
                                                             const Cast& cast)
{
	std::vector<glm::vec2> flattened(points.size());
	return Measure(points, standsAt, cast, flattened);
}

std::optional<GroundShadow::Footprint> GroundShadow::Measure(std::span<const Point> points, const glm::vec3& standsAt,
                                                             const Cast& cast, std::span<glm::vec2> flattened)
{
	if (points.empty() || flattened.size() < points.size())
	{
		return std::nullopt;
	}
	const auto way = LightWay(standsAt, cast.light);
	Footprint footprint {.low = glm::vec2(std::numeric_limits<float>::max()),
	                     .high = glm::vec2(std::numeric_limits<float>::lowest()),
	                     .nearest = std::numeric_limits<float>::max()};
	for (size_t i = 0; i < points.size(); ++i)
	{
		const auto flat = OnLevel(points[i], cast);
		flattened[i] = flat;
		footprint.low = glm::min(footprint.low, flat);
		footprint.high = glm::max(footprint.high, flat);
		footprint.nearest = std::min(footprint.nearest, glm::dot(glm::vec2(points[i].position.x, points[i].position.z), way));
	}
	if (footprint.high.x <= footprint.low.x || footprint.high.y <= footprint.low.y)
	{
		return std::nullopt;
	}
	return footprint;
}

glm::vec2 GroundShadow::Sample(const glm::vec2& onLevel, const Footprint& footprint)
{
	const auto scale = glm::vec2(k_SampleColumns, k_SampleRows) / (footprint.high - footprint.low);
	const auto sample = (onLevel - footprint.low) * scale;
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

namespace
{
/// Steps an edge of a triangle down the rows it spans, from its top corner, in 16.16 fixed point: where it crosses each
/// row goes into the starts when the edge runs up (a left edge) and into the ends when it runs down (a right edge)
void Edge(GroundShadow::Coverage& coverage, int32_t& first, int32_t& last, const glm::vec2& from, const glm::vec2& to)
{
	auto top = static_cast<int32_t>(from.y);
	auto bottom = static_cast<int32_t>(to.y);
	if (top == bottom)
	{
		return;
	}
	auto topX = from.x;
	auto bottomX = to.x;
	auto* rows = &coverage.ends;
	if (top > bottom)
	{
		std::swap(top, bottom);
		std::swap(topX, bottomX);
		rows = &coverage.starts;
	}
	constexpr auto k_Rows = static_cast<int32_t>(GroundShadow::k_SampleRows);
	if (top >= k_Rows || bottom <= 0)
	{
		return;
	}
	const auto slope = (static_cast<double>(bottomX) - static_cast<double>(topX)) / static_cast<double>(bottom - top);
	bottom = std::min(bottom, k_Rows);
	if (top < 0)
	{
		topX = static_cast<float>(static_cast<double>(topX) - (static_cast<double>(top) * slope));
		top = 0;
	}
	first = std::min(first, top);
	last = std::max(last, bottom);
	constexpr double k_One = 65536.0;
	const auto step = static_cast<int32_t>(slope * k_One);
	auto x = static_cast<int32_t>(static_cast<double>(topX) * k_One);
	for (auto row = top; row < bottom; ++row)
	{
		(*rows)[static_cast<size_t>(row)] = x >> 16;
		x += step;
	}
}
} // namespace

size_t GroundShadow::Coverage::Row::count() const
{
	size_t covered = 0;
	for (const auto word : words)
	{
		covered += static_cast<size_t>(std::popcount(word));
	}
	return covered;
}

void GroundShadow::Coverage::Row::Fill(uint32_t from, uint32_t to)
{
	for (uint32_t word = 0; word < words.size(); ++word)
	{
		const auto low = std::max(from, word * 64);
		const auto high = std::min(to, (word * 64) + 64);
		if (low >= high)
		{
			continue;
		}
		const auto width = high - low;
		const auto mask = width >= 64 ? ~uint64_t {0} : ((uint64_t {1} << width) - 1);
		words[word] |= mask << (low - (word * 64));
	}
}

void GroundShadow::Cover(Coverage& coverage, const std::array<glm::vec2, 3>& corners, bool twoSided, bool secondRowsOnly)
{
	const auto& [first, second, third] = corners;
	if (!twoSided && !Counted(corners))
	{
		return;
	}
	// The edges go round the way that puts the edges running up on the left
	const bool flipped = twoSided && Counted({first, third, second});
	const auto& b = flipped ? third : second;
	const auto& c = flipped ? second : third;
	auto top = static_cast<int32_t>(k_SampleRows);
	auto bottom = 0;
	Edge(coverage, top, bottom, first, c);
	Edge(coverage, top, bottom, c, b);
	Edge(coverage, top, bottom, b, first);
	constexpr auto k_Columns = static_cast<int32_t>(k_SampleColumns);
	for (auto row = top; row < bottom; ++row)
	{
		if (secondRowsOnly && row % 2 == 0)
		{
			continue;
		}
		const auto index = static_cast<size_t>(row);
		const auto from = std::max(coverage.starts[index], 0);
		const auto to = std::min(coverage.ends[index], k_Columns);
		if (from < to && from < k_Columns && to > 0)
		{
			coverage.rows[index].Fill(static_cast<uint32_t>(from), static_cast<uint32_t>(to));
		}
	}
}

std::array<uint8_t, GroundShadow::k_Texels * GroundShadow::k_Texels> GroundShadow::Levels(const Coverage& coverage,
                                                                                          uint8_t alpha)
{
	std::array<uint8_t, k_Texels * k_Texels> levels {};
	for (uint8_t row = 1; row < k_Texels - 1; ++row)
	{
		for (uint8_t column = 1; column < k_Texels - 1; ++column)
		{
			uint8_t covered = 0;
			const auto first = static_cast<size_t>(column) * k_SamplesAcross;
			for (uint8_t down = 0; down < k_SamplesDown; ++down)
			{
				const auto& words = coverage.rows.at((row * k_SamplesDown) + down).words;
				const auto samples = (words.at(first / 64) >> (first % 64)) & 0xFU;
				covered += static_cast<uint8_t>(std::popcount(samples));
			}
			levels.at((row * k_Texels) + column) = Level(covered, alpha);
		}
	}
	return levels;
}

glm::vec4 GroundShadow::Receiver(const Footprint& footprint, const Cast& cast)
{
	// The land is flattened onto the level along the light's rays as if it all stood at the ground's height
	const auto light = glm::dvec3(cast.light);
	const auto along = (static_cast<double>(cast.level) - light.y) / (static_cast<double>(cast.ground) - light.y);
	const auto low = glm::dvec2(footprint.low);
	const auto size = glm::dvec2(footprint.high) - low;
	return {static_cast<float>(along / size.x), static_cast<float>(((light.x * (1.0 - along)) - low.x) / size.x),
	        static_cast<float>(along / size.y), static_cast<float>(((light.z * (1.0 - along)) - low.y) / size.y)};
}

bool GroundShadow::ReachesBlock(const Footprint& footprint, const glm::vec2& block)
{
	return block.x <= footprint.high.x && footprint.low.x <= block.x + k_BlockSize && block.y <= footprint.high.y &&
	       footprint.low.y <= block.y + k_BlockSize;
}

} // namespace openblack::graphics
