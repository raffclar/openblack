/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BlockTexture.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <vector>

#include <LNDFile.h>

#include "Common/WorkerPool.h"

using namespace openblack;
using namespace openblack::block_texture;

namespace
{
/// The coast alpha's 16 steps
constexpr std::array<uint8_t, 16> k_CoastAlphaSteps = {0, 0, 0, 1, 2, 4, 6, 8, 9, 10, 10, 11, 12, 13, 14, 15};

/// A cell of open sea, which isn't drawn
constexpr uint8_t k_OpenSeaFlag = 0x02;

constexpr size_t k_MapTexels = static_cast<size_t>(k_Side) * k_Side;

uint16_t MaterialTexel(std::span<const uint16_t> materials, uint32_t material, size_t texelIndex)
{
	const size_t index = (material * k_MapTexels) + texelIndex;
	return index < materials.size() ? materials[index] : uint16_t {0};
}
} // namespace

ConeWeights block_texture::ComputeConeWeights()
{
	ConeWeights weights {};
	for (int i = 0; i < k_TexelsPerCell; ++i)
	{
		for (int j = 0; j < k_TexelsPerCell; ++j)
		{
			const auto distance = [](int a, int b) { return std::sqrt(static_cast<float>((a * a) + (b * b))); };
			const std::array<float, 4> distances = {distance(i, j), distance(i, k_TexelsPerCell - j),
			                                        distance(k_TexelsPerCell - i, k_TexelsPerCell - j),
			                                        distance(k_TexelsPerCell - i, j)};
			std::array<float, 4> cone {};
			std::ranges::transform(distances, cone.begin(), [](float d) { return d <= 14.0f ? 14.0f - d : 0.0f; });
			// The sum starts from the last corner
			const float sum = ((cone[3] + cone[2]) + cone[1]) + cone[0];
			const float scale = 255.0f / sum;
			// Scaled to 255 and truncated, then the largest weight, the last of equal ones, takes what is left one by one
			std::array<int, 4> scaled {};
			int total = 0;
			for (size_t k = 0; k < scaled.size(); ++k)
			{
				scaled.at(k) = static_cast<int>(scale * cone.at(k));
				total += scaled.at(k);
			}
			for (; total < 255; ++total)
			{
				size_t largest = 0;
				for (size_t k = 1; k < scaled.size(); ++k)
				{
					if (scaled.at(k) >= scaled.at(largest))
					{
						largest = k;
					}
				}
				++scaled.at(largest);
			}
			std::ranges::transform(scaled, weights.at(static_cast<size_t>((i * k_TexelsPerCell) + j)).begin(),
			                       [](int w) { return static_cast<uint8_t>(w); });
		}
	}
	return weights;
}

uint8_t block_texture::CoastAlpha(int32_t h, uint8_t noise)
{
	if (h < 0x100)
	{
		return 0;
	}
	if (h >= 0x400)
	{
		return 15;
	}
	const int32_t e = h + (4 * static_cast<int8_t>(noise));
	if (e < 0x200)
	{
		return k_CoastAlphaSteps.front();
	}
	if (e > 0x3B6)
	{
		return k_CoastAlphaSteps.back();
	}
	// Divided rounding towards zero
	const int32_t step = 15 + (((15 * e) - 14250) / 438);
	return k_CoastAlphaSteps.at(static_cast<size_t>(std::clamp(step, 0, 15)));
}

uint16_t block_texture::CountryTexel(const lnd::LNDCountry& country, int32_t h, uint8_t noise, uint8_t bump,
                                     std::span<const uint16_t> materials, size_t texelIndex)
{
	if (h < 0x100)
	{
		return 0;
	}
	// The country's materials for the height, lifted by the noise
	const auto level = static_cast<size_t>(std::min((h >> 8) + static_cast<int32_t>(noise), 255));
	const auto& entry = country.materials[level];
	const uint32_t b = bump;
	const uint32_t first = MaterialTexel(materials, entry.indices[0], texelIndex);
	uint32_t red = 0;
	uint32_t green = 0;
	uint32_t blue = 0;
	if (entry.indices[0] == entry.indices[1])
	{
		// The 5-bit channels shaded by the bump map into the top 4 bits of their nibbles
		red = ((first & 0x7C00u) * b) >> 10;
		green = ((first & 0x3E0u) * b) >> 9;
		blue = ((first & 0x1Fu) * b) >> 8;
	}
	else
	{
		// The first material weighs the coefficient out of 256, the second the rest
		const uint32_t second = MaterialTexel(materials, entry.indices[1], texelIndex);
		const uint32_t k0 = entry.coefficient;
		const uint32_t k1 = 0x100u - k0;
		red = ((((first & 0x7C00u) * k0) + ((second & 0x7C00u) * k1)) * b) >> 18;
		green = ((((first & 0x3E0u) * k0) + ((second & 0x3E0u) * k1)) * b) >> 17;
		blue = ((((first & 0x1Fu) * k0) + ((second & 0x1Fu) * k1)) * b) >> 16;
	}
	// A bright bump saturates each channel
	red = std::min(red, 0xF00u) & 0xF00u;
	green = std::min(green, 0xF0u) & 0xF0u;
	blue = std::min(blue, 0xFu) & 0xFu;
	const auto alpha = static_cast<uint32_t>(CoastAlpha(h, noise)) << 12;
	return static_cast<uint16_t>(alpha | red | green | blue);
}

uint16_t block_texture::BlendCorners(const std::array<uint16_t, 4>& texels, const std::array<uint8_t, 4>& weights)
{
	uint32_t out = 0;
	for (const uint32_t mask : {0xF00u, 0xF0u, 0xFu, 0xF000u})
	{
		uint32_t sum = 0;
		for (size_t k = 0; k < texels.size(); ++k)
		{
			sum += (texels.at(k) & mask) * weights.at(k);
		}
		out |= (sum / 255u) & mask;
	}
	return static_cast<uint16_t>(out);
}

void block_texture::BuildBlock(std::span<const lnd::LNDCell> cells, const Sources& sources, std::span<uint8_t> rgba)
{
	PaintCells(cells, sources, {0, 0}, {k_BlockCells, k_BlockCells}, rgba);
}

void block_texture::PaintCells(std::span<const lnd::LNDCell> cells, const Sources& sources, glm::ivec2 firstCell,
                               glm::ivec2 cellCount, std::span<uint8_t> rgba)
{
	const auto perCell = TexelsPerCellOf(sources.resolution);
	std::vector<uint16_t> texels(static_cast<size_t>(std::max(cellCount.x, 0) * std::max(cellCount.y, 0)) * perCell * perCell);
	PaintCellTexels(cells, sources, firstCell, cellCount, texels);
	// The texels fill the start of rgba, and what is left after them is clear
	const auto painted = std::min(rgba.size(), texels.size() * 4);
	ToRgba(texels, rgba.first(painted));
	std::ranges::fill(rgba.subspan(painted), uint8_t {0});
}

void block_texture::BuildBlockTexels(std::span<const lnd::LNDCell> cells, const Sources& sources, std::span<uint16_t> texels)
{
	PaintCellTexels(cells, sources, {0, 0}, {k_BlockCells, k_BlockCells}, texels);
}

void block_texture::ToRgba(std::span<const uint16_t> texels, std::span<uint8_t> rgba)
{
	const auto count = std::min(texels.size(), rgba.size() / 4);
	auto* out = rgba.data();
	for (const auto texel : texels.first(count))
	{
		out[0] = static_cast<uint8_t>(((texel >> 8) & 0xF) * 17);
		out[1] = static_cast<uint8_t>(((texel >> 4) & 0xF) * 17);
		out[2] = static_cast<uint8_t>((texel & 0xF) * 17);
		out[3] = static_cast<uint8_t>(((texel >> 12) & 0xF) * 17);
		out += 4;
	}
}

void block_texture::PaintCellTexels(std::span<const lnd::LNDCell> cells, const Sources& sources, glm::ivec2 firstCell,
                                    glm::ivec2 cellCount, std::span<uint16_t> texels)
{
	std::ranges::fill(texels, uint16_t {0});
	// The low resolution paints every second texel of the full texture along each axis
	const auto perCell = TexelsPerCellOf(sources.resolution);
	const auto step = k_TexelsPerCell / perCell;
	const auto rows = cellCount.x * perCell;
	const auto columns = cellCount.y * perCell;
	if (cells.size() < static_cast<size_t>(k_CellsPerSide * k_CellsPerSide) || cellCount.x <= 0 || cellCount.y <= 0 ||
	    firstCell.x < 0 || firstCell.y < 0 || firstCell.x + cellCount.x > k_BlockCells ||
	    firstCell.y + cellCount.y > k_BlockCells || texels.size() < static_cast<size_t>(rows) * columns ||
	    sources.countries.empty())
	{
		return;
	}
	// The weights are the same for every cell of every block
	static const ConeWeights k_ConeWeights = ComputeConeWeights();
	const auto& coneWeights = k_ConeWeights;
	const auto countryOf = [&sources](const lnd::LNDCell& cell) -> const lnd::LNDCountry& {
		return sources.countries[std::min<size_t>(cell.properties.country, sources.countries.size() - 1)];
	};
	// Cell by cell: the corners and their countries are the same for all the texels of a cell
	for (int cellX = firstCell.x; cellX < firstCell.x + cellCount.x; ++cellX)
	{
		for (int cellZ = firstCell.y; cellZ < firstCell.y + cellCount.y; ++cellZ)
		{
			const auto cellIndex = static_cast<size_t>((cellX * k_CellsPerSide) + cellZ);
			const auto& cell = cells[cellIndex];
			if ((cell.flags & k_OpenSeaFlag) != 0)
			{
				continue;
			}
			const std::array<const lnd::LNDCell*, 4> corners = {
			    &cell, &cells[cellIndex + 1], &cells[cellIndex + k_CellsPerSide + 1], &cells[cellIndex + k_CellsPerSide]};
			const auto country = cell.properties.country;
			const bool oneCountry =
			    std::ranges::all_of(corners, [country](const auto* c) { return c->properties.country == country; });
			const std::array<const lnd::LNDCountry*, 4> countries = {&countryOf(*corners[0]), &countryOf(*corners[1]),
			                                                         &countryOf(*corners[2]), &countryOf(*corners[3])};
			const std::array<int32_t, 4> altitudes = {corners[0]->altitude, corners[1]->altitude, corners[2]->altitude,
			                                          corners[3]->altitude};
			for (int i = 0; i < k_TexelsPerCell; i += step)
			{
				const int x = (cellX * k_TexelsPerCell) + i;
				for (int j = 0; j < k_TexelsPerCell; j += step)
				{
					const int z = (cellZ * k_TexelsPerCell) + j;
					const auto& weights = coneWeights[static_cast<size_t>((i * k_TexelsPerCell) + j)];
					const int32_t h =
					    (static_cast<int32_t>(weights[0]) * altitudes[0]) + (static_cast<int32_t>(weights[1]) * altitudes[1]) +
					    (static_cast<int32_t>(weights[2]) * altitudes[2]) + (static_cast<int32_t>(weights[3]) * altitudes[3]);
					const auto index = static_cast<size_t>((x * k_Side) + z);
					const uint8_t noise = index < sources.noise.size() ? sources.noise[index] : uint8_t {0};
					const uint8_t bump = index < sources.bump.size() ? sources.bump[index] : uint8_t {0x80};

					uint16_t texel = 0;
					if (oneCountry)
					{
						texel = CountryTexel(*countries[0], h, noise, bump, sources.materials, index);
					}
					else
					{
						const std::array<uint16_t, 4> painted = {
						    CountryTexel(*countries[0], h, noise, bump, sources.materials, index),
						    CountryTexel(*countries[1], h, noise, bump, sources.materials, index),
						    CountryTexel(*countries[2], h, noise, bump, sources.materials, index),
						    CountryTexel(*countries[3], h, noise, bump, sources.materials, index)};
						texel = BlendCorners(painted, weights);
					}

					const auto row = ((cellX - firstCell.x) * perCell) + (i / step);
					const auto column = ((cellZ - firstCell.y) * perCell) + (j / step);
					texels[static_cast<size_t>((row * columns) + column)] = texel;
				}
			}
		}
	}
}

void block_texture::BuildBlocks(size_t count, const CellsOfBlock& cellsOf, const Sources& sources, WorkerPool& pool,
                                const MemoryOfBlock& into)
{
	// Each lane paints whole blocks straight into their own memory, so the function copies nothing after
	pool.ForEach(count, [&](size_t block, size_t /*lane*/) { BuildBlock(cellsOf(block), sources, into(block)); });
}
