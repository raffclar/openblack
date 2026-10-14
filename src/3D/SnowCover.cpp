/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SnowCover.h"

#include <algorithm>
#include <array>

namespace openblack::snow_cover
{

namespace
{
constexpr float k_InverseCell = 0.025f;
/// A turn's time, and how much of a storm's snow settles in it
constexpr float k_Turn = 0.1f;
constexpr float k_Settles = 0.03f;
/// The rows of the grid that melt together, every so often, and by how much
constexpr int32_t k_Bands = 8;
constexpr int32_t k_BandRows = k_GridSize / k_Bands;
constexpr float k_MeltEvery = 0.3f;
constexpr float k_MeltBy = 2.0f;
constexpr float k_ObjectLeast = 20.0f;

size_t Index(int32_t x, int32_t z)
{
	return (static_cast<size_t>(z) * k_GridSize) + static_cast<size_t>(x);
}

/// The depth at a corner of the grid in 65536ths, cut towards zero; none off the grid
int32_t FixedDepth(std::span<const float> grid, int32_t x, int32_t z)
{
	if (x < 0 || z < 0 || x >= k_GridSize || z >= k_GridSize)
	{
		return 0;
	}
	return static_cast<int32_t>(grid[Index(x, z)] * 65536.0f);
}
} // namespace

float StormSnowPerTurn(int8_t snow, float strength)
{
	const auto whole = static_cast<int32_t>(static_cast<float>(snow) * strength);
	return static_cast<float>(whole) * k_Turn * k_Settles;
}

void AddStorm(std::span<float> grid, glm::vec2 centre, float innerRadius, float outerRadius, float amount)
{
	const auto middle = centre * k_InverseCell;
	const float inner = innerRadius * k_InverseCell;
	float outer = outerRadius * k_InverseCell;
	if (outer <= inner)
	{
		outer = inner + 0.02f;
	}
	const auto x0 = static_cast<int32_t>(middle.x - outer);
	const auto z0 = static_cast<int32_t>(middle.y - outer);
	const auto x1 = std::min(static_cast<int32_t>(middle.x + outer + 2.0f), k_GridSize);
	const auto z1 = std::min(static_cast<int32_t>(middle.y + outer + 2.0f), k_GridSize);
	if (x0 >= k_GridSize || z0 >= k_GridSize || x1 <= 0 || z1 <= 0)
	{
		return;
	}
	const float inner2 = inner * inner;
	const float outer2 = outer * outer;
	for (int32_t z = std::max(z0, 0); z < z1; ++z)
	{
		const float dz = static_cast<float>(z) - middle.y;
		for (int32_t x = std::max(x0, 0); x < x1; ++x)
		{
			const float dx = static_cast<float>(x) - middle.x;
			const float distance2 = (dx * dx) + (dz * dz);
			if (distance2 >= outer2)
			{
				continue;
			}
			// Beyond the inner radius the snow thins with the square of the distance, and turns to melting before the edge
			const float laid = distance2 > inner2 ? ((outer2 - distance2) - inner2) * (amount / (outer2 - inner2)) : amount;
			auto& depth = grid[Index(x, z)];
			depth = std::clamp(depth + laid, 0.0f, k_MaxDepth);
		}
	}
}

void Melt(std::span<float> grid, Melting& melting, float seconds)
{
	melting.clock += seconds;
	while (melting.clock > k_MeltEvery)
	{
		melting.clock -= k_MeltEvery;
		melting.band = (melting.band + 1) % k_Bands;
		const auto first = Index(0, melting.band * k_BandRows);
		const auto band = grid.subspan(first, static_cast<size_t>(k_BandRows) * k_GridSize);
		std::ranges::for_each(band, [](float& depth) { depth = depth > k_MeltBy ? depth - k_MeltBy : 0.0f; });
	}
}

float DepthAt(std::span<const float> grid, glm::vec2 xz)
{
	const auto cell = xz * k_InverseCell;
	constexpr auto k_Last = static_cast<float>(k_GridSize - 1);
	if (cell.x < 0.0f || cell.x >= k_Last || cell.y < 0.0f || cell.y >= k_Last)
	{
		return 0.0f;
	}
	const auto x = static_cast<int32_t>(cell.x);
	const auto z = static_cast<int32_t>(cell.y);
	const float wx = cell.x - static_cast<float>(x);
	const float wz = cell.y - static_cast<float>(z);
	const float d00 = grid[Index(x, z)];
	const float d10 = grid[Index(x + 1, z)];
	const float d01 = grid[Index(x, z + 1)];
	const float d11 = grid[Index(x + 1, z + 1)];
	const float near = d00 + ((d10 - d00) * wx);
	const float far = d01 + ((d11 - d01) * wx);
	return near + ((far - near) * wz);
}

int32_t ObjectLevel(float depth, int32_t rate, int32_t cap)
{
	const auto beyond = static_cast<int32_t>(std::max(depth - k_ObjectLeast, 0.0f));
	return std::min((beyond * rate) >> 8, cap);
}

int32_t LevelStep(int32_t gameMilliseconds)
{
	return gameMilliseconds >> 4;
}

int32_t EaseLevel(int32_t level, int32_t target, int32_t step)
{
	return std::clamp(target, level - step, level + step);
}

int32_t ObjectThreshold(int32_t level)
{
	const auto clamped = std::clamp(level, 0, 255);
	return std::max(250 - clamped, 0);
}

int32_t LandLevel(float depth, uint8_t noise)
{
	const auto whole = static_cast<int32_t>(depth);
	return std::clamp((whole - static_cast<int32_t>(noise)) >> 3, 0, 16);
}

uint8_t LandWhite(uint8_t noise)
{
	return static_cast<uint8_t>((((noise >> 2) + (noise >> 1)) & 1) + 14);
}

int32_t TexelDepth(std::span<const float> grid, glm::ivec2 cell, glm::ivec2 step)
{
	const auto low = FixedDepth(grid, cell.x, cell.y);
	const auto lowFar = FixedDepth(grid, cell.x, cell.y + 1);
	const auto high = FixedDepth(grid, cell.x + 1, cell.y);
	const auto highFar = FixedDepth(grid, cell.x + 1, cell.y + 1);
	// Each step along x and then along z adds the same whole part of the difference, cut towards zero
	const auto near = low + (step.x * ((high - low) / k_TexelsPerCell));
	const auto far = lowFar + (step.x * ((highFar - lowFar) / k_TexelsPerCell));
	return near + (step.y * ((far - near) / k_TexelsPerCell));
}

bool CellTakesSnow(std::span<const float> grid, glm::ivec2 cell)
{
	constexpr auto k_Least = static_cast<int32_t>(k_LandLeast * 65536.0f);
	return std::ranges::any_of(
	    std::array {glm::ivec2(0, 0), glm::ivec2(0, 1), glm::ivec2(1, 0), glm::ivec2(1, 1)},
	    [&](glm::ivec2 corner) { return FixedDepth(grid, cell.x + corner.x, cell.y + corner.y) > k_Least; });
}

glm::u8vec3 PaintTexel(glm::u8vec3 colour, int32_t level, uint8_t white)
{
	if (level <= 0)
	{
		return colour;
	}
	if (level >= 16)
	{
		return {white, white, 15};
	}
	// Each 4-bit level is scaled down and rounded down, and the grey is added in the same way
	const auto grey = (white * level) >> 4;
	const auto mix = [&](uint8_t channel) { return static_cast<uint8_t>(((channel * (16 - level)) >> 4) + grey); };
	return {mix(colour.r), mix(colour.g), mix(colour.b)};
}

} // namespace openblack::snow_cover
