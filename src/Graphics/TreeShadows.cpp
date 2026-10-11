/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TreeShadows.h"

#include <cmath>

#include <algorithm>
#include <bit>

namespace openblack::graphics::tree_shadows
{

namespace
{

/// The texels per unit, as the game works them out: the texels along a block times a 160th
constexpr float k_TexelsPerBlock = static_cast<float>(k_BlockTexels);
constexpr float k_PerBlockSize = 1.0f / k_BlockSize;
constexpr int k_LastTexel = k_BlockTexels - 1;

/// What one row of texels a triangle covers: its first and last texel along z, with the texture's coordinates there
struct Row
{
	bool used {false};
	int first {0};
	int last {0};
	glm::vec2 firstUv {0.0f};
	glm::vec2 lastUv {0.0f};
};
using Rows = std::array<Row, k_BlockTexels>;

/// Notes a point of an edge in its row: the row's ends are pushed out to it, keeping the texture's coordinates there
void Mark(Rows& rows, int x, int z, glm::vec2 uv)
{
	if (x < 0 || x > k_LastTexel)
	{
		return;
	}
	auto& row = rows[static_cast<size_t>(x)];
	if (!row.used)
	{
		row = {.used = true, .first = z, .last = z, .firstUv = uv, .lastUv = uv};
	}
	else if (z < row.first)
	{
		row.first = z;
		row.firstUv = uv;
	}
	else if (z > row.last)
	{
		row.last = z;
		row.lastUv = uv;
	}
}

/// Steps along an edge a texel at a time from the corner further back along x, between the corners taken down to whole
/// texels, the texture's coordinates moving on evenly with each step
void Edge(Rows& rows, const Corner& a, const Corner& b)
{
	const auto& start = b.texel.x < a.texel.x ? b : a;
	const auto& end = b.texel.x < a.texel.x ? a : b;
	int x = static_cast<int>(start.texel.x);
	int z = static_cast<int>(start.texel.y);
	int alongX = static_cast<int>(end.texel.x) - x;
	int alongZ = static_cast<int>(end.texel.y) - z;
	const int stepX = alongX < 0 ? -1 : 1;
	const int stepZ = alongZ < 0 ? -1 : 1;
	alongX = std::abs(alongX);
	alongZ = std::abs(alongZ);
	auto uv = start.uv;
	Mark(rows, x, z, uv);
	int error = 0;
	if (alongX < alongZ)
	{
		const auto steps = static_cast<float>(alongZ);
		for (int i = 0; i < alongZ; ++i)
		{
			uv.y = (end.uv.y - start.uv.y) / steps + uv.y;
			uv.x = (end.uv.x - start.uv.x) / steps + uv.x;
			z += stepZ;
			error += alongX;
			if (error > alongZ)
			{
				x += stepX;
				error -= alongZ;
			}
			Mark(rows, x, z, uv);
		}
	}
	else if (alongX != 0)
	{
		const auto steps = static_cast<float>(alongX);
		for (int i = 0; i < alongX; ++i)
		{
			uv.x = (end.uv.x - start.uv.x) / steps + uv.x;
			uv.y = (end.uv.y - start.uv.y) / steps + uv.y;
			x += stepX;
			error += alongZ;
			if (error > alongX)
			{
				z += stepZ;
				error -= alongX;
			}
			Mark(rows, x, z, uv);
		}
	}
}

/// The mask's place along a side for a texture coordinate spread over it: rounded to the nearest place after a further
/// half, so a whole place on, wrapping round
int Place(float spread)
{
	const auto onward = spread + 0.5f;
	return static_cast<int>(std::nearbyint(onward)) & (k_MaskSide - 1);
}

} // namespace

AlphaMask MaskOf(std::span<const uint16_t> texels, uint32_t width, uint32_t height)
{
	AlphaMask mask {};
	if (texels.size() < static_cast<size_t>(width) * height)
	{
		return mask;
	}
	const float acrossStep = static_cast<float>(width) * (1.0f / k_MaskSide);
	const float downStep = static_cast<float>(height) * (1.0f / k_MaskSide);
	for (int row = 0; row < k_MaskSide; ++row)
	{
		const auto y = static_cast<uint32_t>(static_cast<float>(row) * downStep);
		for (int column = 0; column < k_MaskSide; ++column)
		{
			const auto x = static_cast<uint32_t>(static_cast<float>(column) * acrossStep);
			mask[static_cast<size_t>(row * k_MaskSide + column)] = static_cast<uint8_t>((texels[y * width + x] >> 8) & 0xF0);
		}
	}
	return mask;
}

AlphaMask MaskOfCompressed(std::span<const uint8_t> blocks, uint32_t width, uint32_t height, Compression compression)
{
	AlphaMask mask {};
	const uint32_t blockBytes = compression == Compression::Dxt1 ? 8 : 16;
	const uint32_t blocksAcross = std::max(1u, (width + 3) / 4);
	const uint32_t blocksDown = std::max(1u, (height + 3) / 4);
	if (blocks.size() < static_cast<size_t>(blocksAcross) * blocksDown * blockBytes)
	{
		return mask;
	}
	const auto read16 = [&blocks](size_t at) {
		return static_cast<uint16_t>(blocks[at] | (static_cast<uint16_t>(blocks[at + 1]) << 8));
	};
	const auto alphaAt = [&](uint32_t x, uint32_t y) -> uint8_t {
		const size_t block = (static_cast<size_t>(y / 4) * blocksAcross + x / 4) * blockBytes;
		const uint32_t pixel = (y % 4) * 4 + (x % 4);
		switch (compression)
		{
		case Compression::Dxt1:
		{
			// Only the fourth colour of a block whose first colour isn't above its second is a hole
			const auto first = read16(block);
			const auto second = read16(block + 2);
			const auto index = (blocks[block + 4 + pixel / 4] >> ((pixel % 4) * 2)) & 3u;
			return first <= second && index == 3 ? 0 : 0xF;
		}
		case Compression::Dxt3:
			return static_cast<uint8_t>((blocks[block + pixel / 2] >> ((pixel % 2) * 4)) & 0xFu);
		case Compression::Dxt5:
		{
			const uint32_t first = blocks[block];
			const uint32_t second = blocks[block + 1];
			uint64_t bits = 0;
			for (size_t i = 0; i < 6; ++i)
			{
				bits |= static_cast<uint64_t>(blocks[block + 2 + i]) << (8 * i);
			}
			const auto index = static_cast<uint32_t>((bits >> (3 * pixel)) & 7u);
			uint32_t alpha = 0;
			if (index == 0)
			{
				alpha = first;
			}
			else if (index == 1)
			{
				alpha = second;
			}
			else if (first > second)
			{
				alpha = ((8 - index) * first + (index - 1) * second) / 7;
			}
			else if (index == 6)
			{
				alpha = 0;
			}
			else if (index == 7)
			{
				alpha = 255;
			}
			else
			{
				alpha = ((6 - index) * first + (index - 1) * second) / 5;
			}
			return static_cast<uint8_t>(alpha >> 4);
		}
		}
		return 0;
	};
	const float acrossStep = static_cast<float>(width) * (1.0f / k_MaskSide);
	const float downStep = static_cast<float>(height) * (1.0f / k_MaskSide);
	for (int row = 0; row < k_MaskSide; ++row)
	{
		const auto y = static_cast<uint32_t>(static_cast<float>(row) * downStep);
		for (int column = 0; column < k_MaskSide; ++column)
		{
			const auto x = static_cast<uint32_t>(static_cast<float>(column) * acrossStep);
			mask[static_cast<size_t>(row * k_MaskSide + column)] = static_cast<uint8_t>(alphaAt(x, y) << 4);
		}
	}
	return mask;
}

uint8_t Shade(uint8_t coverage)
{
	// An eighth of 255 for each bit, halved
	const auto bits = static_cast<unsigned>(std::popcount(coverage));
	const auto darkness = ((bits * 255u) >> 3u) >> 1u;
	return static_cast<uint8_t>(255u - darkness);
}

Corner Flatten(glm::vec3 offset, glm::vec2 foot, glm::vec2 block, glm::vec2 uv)
{
	// The block's corner is taken as the origin, so the far sun is placed from it too
	const auto footInBlock = foot - block;
	const auto sun = (footInBlock - foot) + glm::vec2(k_Sun.x, k_Sun.z);
	const auto point = glm::vec2(offset.x, offset.z) + footInBlock;
	const float height = std::max(offset.y, 0.0f);
	const float along = k_Sun.y / (k_Sun.y - height);
	const auto flat = (point - sun) * along + sun;
	return {
	    .texel = flat * k_TexelsPerBlock * k_PerBlockSize,
	    .uv = uv * static_cast<float>(k_MaskSide - 1) + 0.5f,
	};
}

void Fill(Coverage& coverage, const std::array<Corner, 3>& corners, const AlphaMask& mask)
{
	Rows rows {};
	Edge(rows, corners[0], corners[1]);
	Edge(rows, corners[1], corners[2]);
	Edge(rows, corners[2], corners[0]);
	for (int x = 0; x < k_BlockTexels; ++x)
	{
		const auto& row = rows[static_cast<size_t>(x)];
		if (!row.used)
		{
			continue;
		}
		const int span = row.last - row.first;
		glm::vec2 step {0.0f};
		if (span != 0)
		{
			const float perTexel = 1.0f / static_cast<float>(span);
			step = {(row.lastUv.x - row.firstUv.x) * perTexel, (row.lastUv.y - row.firstUv.y) * perTexel};
		}
		auto uv = row.firstUv;
		const int last = std::min(row.last, k_LastTexel);
		for (int z = row.first; z <= last; ++z)
		{
			if (z >= 0)
			{
				const auto place = Place(uv.y) * k_MaskSide + Place(uv.x);
				coverage[static_cast<size_t>(x * k_BlockTexels + z)] |= mask[static_cast<size_t>(place)];
			}
			uv += step;
		}
	}
}

bool Reaches(const std::array<Corner, 3>& corners)
{
	const auto whole = [](float value) { return static_cast<int>(value); };
	int lowX = whole(corners[0].texel.x);
	int highX = lowX;
	int lowZ = whole(corners[0].texel.y);
	int highZ = lowZ;
	for (const auto& corner : std::span(corners).subspan(1))
	{
		lowX = std::min(lowX, whole(corner.texel.x));
		highX = std::max(highX, whole(corner.texel.x));
		lowZ = std::min(lowZ, whole(corner.texel.y));
		highZ = std::max(highZ, whole(corner.texel.y));
	}
	return highX >= 0 && lowX <= k_LastTexel && highZ >= 0 && lowZ <= k_LastTexel;
}

bool TakesShadowFrom(glm::ivec2 block, glm::ivec2 cell)
{
	const auto first = block * k_BlockCells - k_CellsBefore;
	const auto last = block * k_BlockCells + k_BlockCells - 1 + k_CellsAfter;
	return cell.x >= first.x && cell.x <= last.x && cell.y >= first.y && cell.y <= last.y;
}

glm::ivec2 BlocksTakingShadowFrom(int cell)
{
	const auto floorDivide = [](int value, int by) { return value >= 0 ? value / by : -((-value + by - 1) / by); };
	// The first block whose last cell taken reaches the cell, up to the last whose first cell taken does
	const int first = -floorDivide(-(cell - (k_BlockCells - 1 + k_CellsAfter)), k_BlockCells);
	const int last = floorDivide(cell + k_CellsBefore, k_BlockCells);
	return {first, last};
}

} // namespace openblack::graphics::tree_shadows
