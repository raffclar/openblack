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

#include <glm/common.hpp>
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
	// The game multiplies by a 255th as a float, which is a little more than the true one
	constexpr float k_Full = 255.0f;
	constexpr float k_Step = 1.0f / 255.0f;
	return static_cast<uint8_t>(std::clamp(static_cast<int32_t>(k_Full * fade * k_Step), 0, 255));
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
			towards *= nearest / std::sqrt((towards.y * towards.y) + (towards.z * towards.z) + (towards.x * towards.x));
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
	auto height = point.position.y - cast.level;
	if (point.keptAboveLevel)
	{
		height = std::max(height, 0.0f);
	}
	const auto& light = cast.light;
	const float along = -light.y / (height - light.y);
	return {((point.position.x - light.x) * along) + light.x, ((point.position.z - light.z) * along) + light.z};
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

namespace
{
/// Whether a triangle faces the counted way (see GroundShadow::Counted). Each triangle of a shadow uses this test. It is
/// inline, thus the builds that inline only such functions also inline it.
inline bool Faces(const glm::vec2& first, const glm::vec2& second, const glm::vec2& third)
{
	return (first.x - third.x) * (second.y - third.y) <= (second.x - third.x) * (first.y - third.y);
}

/// Covers the samples of a row from `from` up to but not including `to` (see GroundShadow::Coverage::Row::Fill)
inline void FillRow(GroundShadow::Coverage::Row& row, uint32_t from, uint32_t to)
{
	for (uint32_t word = 0; word < row.words.size(); ++word)
	{
		const auto low = std::max(from, word * 64);
		const auto high = std::min(to, (word * 64) + 64);
		if (low >= high)
		{
			continue;
		}
		const auto width = high - low;
		const auto mask = width >= 64 ? ~uint64_t {0} : ((uint64_t {1} << width) - 1);
		row.words[word] |= mask << (low - (word * 64));
	}
}
} // namespace

bool GroundShadow::Counted(const std::array<glm::vec2, 3>& corners)
{
	const auto& [first, second, third] = corners;
	return Faces(first, second, third);
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
inline void Edge(GroundShadow::Coverage& coverage, int32_t& first, int32_t& last, const glm::vec2& from, const glm::vec2& to)
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
	const float slope = (bottomX - topX) / static_cast<float>(bottom - top);
	bottom = std::min(bottom, k_Rows);
	if (top < 0)
	{
		topX = topX - (static_cast<float>(top) * slope);
		top = 0;
	}
	first = std::min(first, top);
	last = std::max(last, bottom);
	constexpr float k_One = 65536.0f;
	const auto step = static_cast<int32_t>(slope * k_One);
	auto x = static_cast<int32_t>(topX * k_One);
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
	FillRow(*this, from, to);
}

void GroundShadow::Cover(Coverage& coverage, const std::array<glm::vec2, 3>& corners, bool twoSided, bool secondRowsOnly)
{
	const auto& [first, second, third] = corners;
	if (!twoSided && !Faces(first, second, third))
	{
		return;
	}
	// The edges go round the way that puts the edges running up on the left
	const bool flipped = twoSided && Faces(first, third, second);
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
			FillRow(coverage.rows[index], static_cast<uint32_t>(from), static_cast<uint32_t>(to));
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
	const auto& light = cast.light;
	const float along = (cast.level - light.y) / (cast.ground - light.y);
	const auto& low = footprint.low;
	const auto size = footprint.high - low;
	return {along / size.x, ((light.x * (1.0f - along)) - low.x) / size.x, along / size.y,
	        ((light.z * (1.0f - along)) - low.y) / size.y};
}

bool GroundShadow::ReachesBlock(const Footprint& footprint, const glm::vec2& block)
{
	return block.x <= footprint.high.x && footprint.low.x <= block.x + k_BlockSize && block.y <= footprint.high.y &&
	       footprint.low.y <= block.y + k_BlockSize;
}

void GroundShadow::ShadowsOfBlocks(std::span<const Footprint> footprints, std::span<const glm::vec2> blocks,
                                   std::vector<uint32_t>& firsts, std::vector<uint32_t>& shadows)
{
	firsts.assign(blocks.size() + 1, 0);
	shadows.clear();
	const auto testAll = [&]() {
		for (size_t block = 0; block < blocks.size(); ++block)
		{
			firsts[block] = static_cast<uint32_t>(shadows.size());
			for (size_t shadow = 0; shadow < footprints.size(); ++shadow)
			{
				if (ReachesBlock(footprints[shadow], blocks[block]))
				{
					shadows.push_back(static_cast<uint32_t>(shadow));
				}
			}
		}
		firsts[blocks.size()] = static_cast<uint32_t>(shadows.size());
	};
	if (blocks.empty() || footprints.empty())
	{
		return;
	}

	// The grid cell of each block. Blocks at places that are not finite, or two blocks in one cell, use the test of all.
	constexpr float k_Limit = 1.0e6f;
	const auto cellOf = [](float position) { return static_cast<int32_t>(std::floor(position / k_BlockSize)); };
	glm::ivec2 low {std::numeric_limits<int32_t>::max()};
	glm::ivec2 high {std::numeric_limits<int32_t>::min()};
	for (const auto& block : blocks)
	{
		if (!(std::abs(block.x) < k_Limit && std::abs(block.y) < k_Limit))
		{
			testAll();
			return;
		}
		const glm::ivec2 cell {cellOf(block.x), cellOf(block.y)};
		low = glm::min(low, cell);
		high = glm::max(high, cell);
	}
	const auto size = high - low + 1;
	constexpr int32_t k_MostCells = 1 << 20;
	if (static_cast<int64_t>(size.x) * size.y > k_MostCells)
	{
		testAll();
		return;
	}
	constexpr uint32_t k_NoBlock = std::numeric_limits<uint32_t>::max();
	std::vector<uint32_t> grid(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), k_NoBlock);
	for (size_t block = 0; block < blocks.size(); ++block)
	{
		const auto cell = glm::ivec2 {cellOf(blocks[block].x), cellOf(blocks[block].y)} - low;
		auto& slot = grid[(static_cast<size_t>(cell.y) * static_cast<size_t>(size.x)) + static_cast<size_t>(cell.x)];
		if (slot != k_NoBlock)
		{
			testAll();
			return;
		}
		slot = static_cast<uint32_t>(block);
	}

	// Each shadow tests the blocks of the cells round its extent, one cell more on each side for the rounding. A shadow
	// with an extent that is not finite tests all the blocks.
	std::vector<std::pair<uint32_t, uint32_t>> found;
	for (size_t shadow = 0; shadow < footprints.size(); ++shadow)
	{
		const auto& footprint = footprints[shadow];
		const auto test = [&](size_t block) {
			if (ReachesBlock(footprint, blocks[block]))
			{
				found.emplace_back(static_cast<uint32_t>(block), static_cast<uint32_t>(shadow));
			}
		};
		if (!(std::abs(footprint.low.x) < k_Limit && std::abs(footprint.low.y) < k_Limit &&
		      std::abs(footprint.high.x) < k_Limit && std::abs(footprint.high.y) < k_Limit))
		{
			for (size_t block = 0; block < blocks.size(); ++block)
			{
				test(block);
			}
			continue;
		}
		const auto from =
		    glm::max(glm::ivec2 {cellOf(footprint.low.x - k_BlockSize), cellOf(footprint.low.y - k_BlockSize)} - 1, low);
		const auto to = glm::min(glm::ivec2 {cellOf(footprint.high.x), cellOf(footprint.high.y)} + 1, high);
		for (auto z = from.y; z <= to.y; ++z)
		{
			for (auto x = from.x; x <= to.x; ++x)
			{
				const auto block =
				    grid[(static_cast<size_t>(z - low.y) * static_cast<size_t>(size.x)) + static_cast<size_t>(x - low.x)];
				if (block != k_NoBlock)
				{
					test(block);
				}
			}
		}
	}

	// Sort the finds by block. The shadows of a block stay in their order, because each shadow adds its finds in turn.
	for (const auto& [block, shadow] : found)
	{
		++firsts[block + 1];
	}
	for (size_t block = 0; block < blocks.size(); ++block)
	{
		firsts[block + 1] += firsts[block];
	}
	shadows.resize(found.size());
	std::vector<uint32_t> next(firsts.begin(), firsts.end() - 1);
	for (const auto& [block, shadow] : found)
	{
		shadows[next[block]++] = shadow;
	}
}

} // namespace openblack::graphics
