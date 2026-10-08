/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CoastAlpha.h"

#include <cmath>

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Graphics/Argb4444.h"

using namespace openblack;

namespace
{
/// The alpha table (the same in the normal and SSE paths): 16 dwords already shifted to the ARGB4444 alpha (0x1000 per
/// step), kept here as the nibble
constexpr std::array<uint8_t, 16> k_AlphaTable = {0, 0, 0, 1, 2, 4, 6, 8, 9, 10, 10, 11, 12, 13, 14, 15};

/// Bit 1 of the cell's flags byte: an open sea cell, not drawn and its texels cleared
constexpr uint8_t k_NotDrawnFlag = 0x02;

coast::ConeWeights BuildConeWeights()
{
	coast::ConeWeights weights {};
	for (int i = 0; i < coast::k_TexelsPerCell; ++i)
	{
		for (int j = 0; j < coast::k_TexelsPerCell; ++j)
		{
			const auto distance = [](int a, int b) { return std::sqrt(static_cast<double>(a * a + b * b)); };
			const std::array<double, 4> d = {distance(i, j), distance(i, 16 - j), distance(16 - i, 16 - j),
			                                 distance(16 - i, j)};
			std::array<double, 4> w {};
			double sum = 0.0;
			for (size_t k = 0; k < w.size(); ++k)
			{
				w[k] = d[k] > 14.0 ? 0.0 : 14.0 - d[k];
				sum += w[k];
			}
			// 255 / sum, truncated; then +1 to the largest until they add up to 255, the *last* of equals
			// (the original's table, read back from an emulated run: e.g. texel (8, 8) = 63, 63, 63, 66)
			const double scale = 255.0 / sum;
			std::array<int, 4> q {};
			int total = 0;
			for (size_t k = 0; k < q.size(); ++k)
			{
				q[k] = static_cast<int>(scale * w[k]);
				total += q[k];
			}
			while (total < 255)
			{
				size_t largest = 0;
				for (size_t k = 1; k < q.size(); ++k)
				{
					if (q[k] >= q[largest])
					{
						largest = k;
					}
				}
				++q[largest];
				++total;
			}
			auto& out = weights.at(static_cast<size_t>(i * coast::k_TexelsPerCell + j));
			for (size_t k = 0; k < q.size(); ++k)
			{
				out.at(k) = static_cast<uint8_t>(q[k]);
			}
		}
	}
	return weights;
}
} // namespace

const coast::ConeWeights& coast::GetConeWeights()
{
	static const ConeWeights k_Weights = BuildConeWeights();
	return k_Weights;
}

uint8_t coast::CoastAlphaNibble(int32_t h, uint8_t noise)
{
	if (h < 0x100)
	{
		return 0; // the whole texel is 0 (black and transparent; SSE: the whole group below 0x100)
	}
	if (h >= 0x400)
	{
		return 15; // alpha 0xF000 (SSE: h > 0x400, the same nibble at 0x400 through the table)
	}
	const int32_t e = h + 4 * static_cast<int8_t>(noise);
	if (e < 0x200)
	{
		return k_AlphaTable.front();
	}
	if (e > 0x3B6)
	{
		return k_AlphaTable.back();
	}
	// C division truncates toward zero, like the original's idiv
	const int32_t index = std::min(15, 15 + (15 * e - 14250) / 438);
	return k_AlphaTable.at(static_cast<size_t>(std::max(0, index)));
}

uint8_t coast::CellTexelNibble(const std::array<uint16_t, 4>& corners, int i, int j, uint8_t noise)
{
	const auto& w = GetConeWeights().at(static_cast<size_t>(i * k_TexelsPerCell + j));
	int32_t h = 0;
	for (size_t k = 0; k < corners.size(); ++k)
	{
		h += static_cast<int32_t>(w.at(k)) * corners.at(k);
	}
	return CoastAlphaNibble(h, noise);
}

std::vector<uint8_t> coast::BuildIslandCoastAlpha(const LandIslandInterface& island, glm::u16vec2 extentIndexMin,
                                                  glm::u16vec2 indexSize, uint16_t texelsPerBlock,
                                                  std::span<const uint8_t> noise)
{
	const size_t width = static_cast<size_t>(indexSize.x) * texelsPerBlock;
	const size_t height = static_cast<size_t>(indexSize.y) * texelsPerBlock;
	std::vector<uint8_t> texels(width * height, 0);
	constexpr size_t k_NoiseSize = static_cast<size_t>(k_TexelsPerBlock) * k_TexelsPerBlock;
	for (const auto& block : island.GetBlocks())
	{
		const auto* cells = block.GetCells();
		const auto position = block.GetBlockPosition() - glm::ivec2(extentIndexMin);
		if (cells == nullptr || position.x < 0 || position.y < 0 || position.x >= indexSize.x || position.y >= indexSize.y)
		{
			continue;
		}
		for (int tx = 0; tx < texelsPerBlock; ++tx)
		{
			// the original's texel under this one (the same one with 256 texels per block)
			const int ox = tx * k_TexelsPerBlock / texelsPerBlock;
			const int cx = ox / k_TexelsPerCell;
			const int i = ox % k_TexelsPerCell;
			for (int tz = 0; tz < texelsPerBlock; ++tz)
			{
				const int oz = tz * k_TexelsPerBlock / texelsPerBlock;
				const int cz = oz / k_TexelsPerCell;
				const int j = oz % k_TexelsPerCell;
				// the block's own 17 x 17 cells: +1 along z, +17 along x
				const auto* base = &cells[cx * 17 + cz];
				uint8_t nibble = 0;
				if ((base[0].flags & k_NotDrawnFlag) == 0)
				{
					const std::array<uint16_t, 4> corners = {island.GetCellAltitude(base[0]), island.GetCellAltitude(base[1]),
					                                         island.GetCellAltitude(base[18]),
					                                         island.GetCellAltitude(base[17])};
					const size_t noiseIndex = static_cast<size_t>(ox) * k_TexelsPerBlock + static_cast<size_t>(oz);
					nibble = CellTexelNibble(corners, i, j, noise.size() >= k_NoiseSize ? noise[noiseIndex] : uint8_t {0});
				}
				const size_t row = static_cast<size_t>(position.y) * texelsPerBlock + static_cast<size_t>(tz);
				const size_t column = static_cast<size_t>(position.x) * texelsPerBlock + static_cast<size_t>(tx);
				texels[row * width + column] = graphics::argb4444::Expand(nibble);
			}
		}
	}
	return texels;
}
