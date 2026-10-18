/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandShade.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include "3D/VillageLights.h"

namespace openblack::land_shade
{
namespace
{
/// A cell is 10 units across
constexpr float k_CellsPerUnit = std::bit_cast<float>(0x3DCCCCCDu);
constexpr float k_Full = 255.0f;
constexpr float k_HalfCell = 5.0f;
constexpr float k_InverseByte = std::bit_cast<float>(0x3B808081u);
/// A light's image texel is brought down by this factor and rounded, to at most the last warm level
constexpr float k_WarmScale = std::bit_cast<float>(0x3E40C0C1u);
constexpr float k_WarmCap = 48.0f;
constexpr float k_LastWarm = 47.0f;
constexpr uint32_t k_LastWarmLevel = 47;

/// One step of the blend between two texels by a weight of 256: the difference times the weight, shifted down, added to
/// the first texel. Only the low byte counts.
uint32_t Blend(uint32_t from, uint32_t to, uint32_t weight)
{
	return ((((to - from) * weight) >> 8u) + from) & 0xFFu;
}

/// The blend of the four texels from a texel of an image: along z by the first weight, then along x by the second
uint32_t Bilinear(std::span<const uint8_t> image, size_t at, size_t side, glm::uvec2 weight)
{
	const auto near = Blend(image[at], image[at + 1], weight.y);
	const auto far = Blend(image[at + side], image[at + side + 1], weight.y);
	return Blend(near, far, weight.x);
}

/// The block that holds a cell and the cell's place in it
glm::ivec2 BlockOf(int32_t x, int32_t z)
{
	return {x >> 4, z >> 4};
}
size_t CellIn(int32_t x, int32_t z)
{
	return static_cast<size_t>(((x & 0xF) * k_BlockSide) + (z & 0xF));
}

/// A light sets a cell to its level when the cell is above the warm levels, or when the level is brighter
void LightCell(uint8_t& cell, uint8_t level)
{
	if (cell > k_LastWarmLevel || cell < level)
	{
		cell = level;
	}
}
} // namespace

void ShadedLand::Lay(std::span<const LaidBlock> blocks)
{
	_slots.fill(-1);
	_places.clear();
	_laid.clear();
	_shaded.clear();
	_changed.clear();
	for (const auto& block : blocks)
	{
		if (block.at.x < 0 || block.at.y < 0 || block.at.x >= k_BlocksPerSide || block.at.y >= k_BlocksPerSide)
		{
			continue;
		}
		_slots.at(static_cast<size_t>((block.at.x * k_BlocksPerSide) + block.at.y)) = static_cast<int16_t>(_places.size());
		_places.push_back(block.at);
		_laid.push_back(block.luminosity);
		_shaded.push_back(block.luminosity);
	}
	_inView.assign(_places.size(), 0);
	_isChanged.assign(_places.size(), 0);
}

void ShadedLand::Restore()
{
	for (const auto place : _changed)
	{
		const auto slot = static_cast<size_t>(SlotOf(place));
		_shaded[slot] = _laid[slot];
		_isChanged[slot] = 0;
	}
	_changed.clear();
}

int32_t ShadedLand::SlotOf(glm::ivec2 block) const
{
	if (block.x < 0 || block.y < 0 || block.x >= k_BlocksPerSide || block.y >= k_BlocksPerSide)
	{
		return -1;
	}
	return _slots[static_cast<size_t>((block.x * k_BlocksPerSide) + block.y)];
}

void ShadedLand::SetInView(glm::ivec2 block, bool inView)
{
	if (const auto slot = SlotOf(block); slot >= 0)
	{
		_inView[static_cast<size_t>(slot)] = inView ? 1 : 0;
	}
}

bool ShadedLand::InView(glm::ivec2 block) const
{
	const auto slot = SlotOf(block);
	return slot >= 0 && _inView[static_cast<size_t>(slot)] != 0;
}

const BlockCells* ShadedLand::Find(glm::ivec2 block) const
{
	const auto slot = SlotOf(block);
	return slot >= 0 ? &_shaded[static_cast<size_t>(slot)] : nullptr;
}

BlockCells* ShadedLand::Change(glm::ivec2 block)
{
	const auto slot = SlotOf(block);
	if (slot < 0)
	{
		return nullptr;
	}
	const auto at = static_cast<size_t>(slot);
	if (_isChanged[at] == 0)
	{
		_isChanged[at] = 1;
		_changed.push_back(block);
	}
	return &_shaded[at];
}

std::optional<land_point_light::Corners> ShadedLand::CornersAt(int32_t x, int32_t z) const
{
	if (x < 0 || z < 0 || x >= k_MapCells || z >= k_MapCells)
	{
		return std::nullopt;
	}
	const auto* cells = Find(BlockOf(x, z));
	if (cells == nullptr)
	{
		return std::nullopt;
	}
	const auto at = CellIn(x, z);
	return land_point_light::Corners {(*cells)[at], (*cells)[at + 1], (*cells)[at + k_BlockSide],
	                                  (*cells)[at + k_BlockSide + 1]};
}

uint8_t Alpha(float strength)
{
	float scaled = strength * k_Full;
	if (scaled < 0.0f)
	{
		scaled = 0.0f;
	}
	else if (scaled > k_Full)
	{
		scaled = k_Full;
	}
	return static_cast<uint8_t>(static_cast<int32_t>(scaled));
}

float CloudStrength(uint32_t alpha)
{
	return static_cast<float>(alpha) * k_InverseByte;
}

glm::vec2 Centred(glm::vec2 centre, int32_t side)
{
	const float back = static_cast<float>(side - 1) * k_HalfCell;
	return {centre.x - back, centre.y - back};
}

ShadowPlace PlaceShadow(glm::vec2 corner, int32_t side)
{
	// Each axis gives its cell, cut towards zero and then down for a point before the map, and the weight of the next
	// texel, rounded to the nearest
	const auto axis = [](float position, int32_t& cell, int32_t& weight) {
		const float cells = position * k_CellsPerUnit;
		cell = static_cast<int32_t>(cells);
		float share = 0.0f;
		if (cells < 0.0f)
		{
			share = (static_cast<float>(cell) - cells) * k_Full;
			--cell;
		}
		else
		{
			share = k_Full - (cells - static_cast<float>(cell)) * k_Full;
		}
		weight = static_cast<int32_t>(std::nearbyint(share)) & 0xFF;
	};
	ShadowPlace place {.cell = {}, .weight = {}, .span = {side - 1, side - 1}, .first = 0, .onMap = true};
	axis(corner.x, place.cell.x, place.weight.x);
	axis(corner.y, place.cell.y, place.weight.y);
	// Cut at the map's edges: rows of the image along x, columns along z
	const auto clip = [side, &place](int32_t& cell, int32_t& span, int32_t texelsPerCell) {
		if (cell > k_MapCells - side)
		{
			if (cell >= k_MapCells)
			{
				place.onMap = false;
			}
			span = k_MapCells - cell;
		}
		else if (cell < 0)
		{
			if (span + cell < 0)
			{
				place.onMap = false;
			}
			place.first -= cell * texelsPerCell;
			span += cell;
			cell = 0;
		}
	};
	clip(place.cell.x, place.span.x, side);
	clip(place.cell.y, place.span.y, 1);
	return place;
}

void CastShadow(ShadedLand& land, const Shadow& shadow)
{
	if (shadow.side < 2 || shadow.image.size() < static_cast<size_t>(shadow.side) * static_cast<size_t>(shadow.side))
	{
		return;
	}
	const auto place = PlaceShadow(shadow.corner, shadow.side);
	if (!place.onMap)
	{
		return;
	}
	const auto side = static_cast<size_t>(shadow.side);
	const glm::uvec2 weight {static_cast<uint32_t>(place.weight.x), static_cast<uint32_t>(place.weight.y)};
	const auto alpha = static_cast<uint32_t>(shadow.alpha);
	const auto last = place.cell + place.span;
	for (int32_t bx = place.cell.x >> 4; bx <= last.x >> 4; ++bx)
	{
		for (int32_t bz = place.cell.y >> 4; bz <= last.y >> 4; ++bz)
		{
			const glm::ivec2 block {bx, bz};
			if (land.Find(block) == nullptr || !land.InView(block))
			{
				continue;
			}
			auto& cells = *land.Change(block);
			// The block's own cells under the shadow, its shared last row and column too
			const glm::ivec2 origin = block * k_BlockStep;
			const auto from = glm::max(place.cell - origin, glm::ivec2(0));
			const auto to = glm::min(last - origin, glm::ivec2(k_BlockSide));
			for (int32_t x = from.x; x < to.x; ++x)
			{
				const auto row = static_cast<size_t>(place.first) + (static_cast<size_t>(origin.x + x - place.cell.x) * side);
				for (int32_t z = from.y; z < to.y; ++z)
				{
					const auto blend =
					    Bilinear(shadow.image, row + static_cast<size_t>(origin.y + z - place.cell.y), side, weight);
					auto value = 255u - (((255u - blend) * alpha) / 255u);
					value = std::max<uint32_t>(value, k_DarkestShadow);
					auto& cell = cells[static_cast<size_t>((x * k_BlockSide) + z)];
					if (value < cell)
					{
						cell = static_cast<uint8_t>(value);
					}
				}
			}
		}
	}
}

uint8_t WarmLevel(uint8_t texel)
{
	float level = static_cast<float>(texel) * k_WarmScale;
	level = level + 0.5f;
	if (!(level < k_WarmCap))
	{
		level = k_LastWarm;
	}
	return static_cast<uint8_t>(static_cast<int32_t>(level));
}

std::vector<uint8_t> WarmImage(std::span<const uint8_t> image)
{
	std::vector<uint8_t> warm(image.size());
	std::ranges::transform(image, warm.begin(), WarmLevel);
	return warm;
}

void CastLight(ShadedLand& land, const Light& light, uint8_t fullLightGreen)
{
	if (!(light.strength > 0.0f) || light.side < 2 ||
	    light.image.size() < static_cast<size_t>(light.side) * static_cast<size_t>(light.side))
	{
		return;
	}
	const auto placement = village_lights::Place(light.corner);
	glm::ivec2 cell = placement.cell;
	const glm::uvec2 weight {static_cast<uint32_t>(placement.weight.x), static_cast<uint32_t>(placement.weight.y)};
	// Cut at the map's edges: rows of the image along x, columns along z
	glm::ivec2 count {light.side, light.side};
	int32_t first = 0;
	if (cell.x > k_MapCells - light.side)
	{
		if (cell.x >= k_MapCells)
		{
			return;
		}
		count.x = k_MapCells - cell.x;
	}
	else if (cell.x < 0)
	{
		count.x = cell.x + light.side;
		first -= cell.x * light.side;
		cell.x = 0;
	}
	if (cell.y > k_MapCells - light.side)
	{
		if (cell.y >= k_MapCells)
		{
			return;
		}
		count.y = k_MapCells - cell.y;
	}
	else if (cell.y < 0)
	{
		count.y = cell.y + light.side;
		first -= cell.y;
		cell.y = 0;
	}
	const auto threshold = static_cast<uint32_t>(village_lights::Threshold(fullLightGreen));
	const auto strength = static_cast<uint32_t>(static_cast<int32_t>(light.strength * k_Full));
	const auto side = static_cast<size_t>(light.side);
	for (int32_t i = 0; i < count.x - 1; ++i)
	{
		const int32_t x = cell.x + i;
		for (int32_t j = 0; j < count.y - 1; ++j)
		{
			const int32_t z = cell.y + j;
			// Up to the second to last cell of the map
			if (x > k_MapCells - 1 || z > k_MapCells - 1)
			{
				continue;
			}
			auto* cells = land.Change(BlockOf(x, z));
			if (cells == nullptr)
			{
				continue;
			}
			auto& own = (*cells)[CellIn(x, z)];
			const auto at = static_cast<size_t>(first) + (static_cast<size_t>(i) * side) + static_cast<size_t>(j);
			auto value = (Bilinear(light.image, at, side, weight) * strength) / 255u;
			if (value <= ((static_cast<uint32_t>(own) * threshold) >> 8u))
			{
				value = own;
			}
			const auto level = static_cast<uint8_t>(value);
			LightCell(own, level);
			// The copies of the cell kept as the last column of the block before along z, the last row and column of the
			// block before along both, and the last row of the block before along x
			if ((z & 0xF) == 0 && z >= 1)
			{
				if (auto* before = land.Change(BlockOf(x, z - 1)); before != nullptr)
				{
					LightCell((*before)[static_cast<size_t>(((x & 0xF) * k_BlockSide) + k_BlockStep)], level);
					if ((x & 0xF) == 0 && x >= 1)
					{
						if (auto* corner = land.Change(BlockOf(x - 1, z - 1)); corner != nullptr)
						{
							LightCell((*corner)[static_cast<size_t>(k_BlockCells - 1)], level);
						}
					}
				}
			}
			if ((x & 0xF) == 0 && x >= 1)
			{
				if (auto* before = land.Change(BlockOf(x - 1, z)); before != nullptr)
				{
					LightCell((*before)[static_cast<size_t>((k_BlockStep * k_BlockSide) + (z & 0xF))], level);
				}
			}
		}
	}
}

} // namespace openblack::land_shade
