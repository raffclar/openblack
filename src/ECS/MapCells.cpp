/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapCells.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack::ecs::map_cells;
using openblack::map_coords::FtoL;

namespace
{
constexpr size_t k_Blocks = static_cast<size_t>(k_BlocksPerSide) * k_BlocksPerSide;

std::optional<size_t> BlockIndex(glm::ivec2 block)
{
	if (block.x < 0 || block.y < 0 || block.x >= k_BlocksPerSide || block.y >= k_BlocksPerSide)
	{
		return std::nullopt;
	}
	return static_cast<size_t>(block.x) * k_BlocksPerSide + static_cast<size_t>(block.y);
}
} // namespace

BlockLists::BlockLists()
    : _blocks(k_Blocks)
    , _onLand(k_Blocks, true)
{
}

void BlockLists::SetLandBlocks(std::span<const glm::ivec2> blocks)
{
	std::ranges::fill(_onLand, false);
	for (const auto block : blocks)
	{
		if (const auto index = BlockIndex(block))
		{
			_onLand.at(*index) = true;
		}
	}
	_landBlocksKnown = true;
}

BlockLists::List& BlockLists::ListOf(glm::ivec2 cell)
{
	const auto index = BlockIndex({cell.x >= 0 ? cell.x / k_BlockCells : -1, cell.y >= 0 ? cell.y / k_BlockCells : -1});
	return index.has_value() && _onLand.at(*index) ? _blocks.at(*index) : _off;
}

void BlockLists::Insert(glm::ivec2 cell, entt::entity entity)
{
	auto& list = ListOf(cell);
	list.lastOut = entt::null;
	if (list.lastIn != entity)
	{
		list.things.push_back(entity);
		list.lastIn = entity;
	}
}

void BlockLists::Remove(glm::ivec2 cell, entt::entity entity)
{
	auto& list = ListOf(cell);
	list.lastIn = entt::null;
	if (list.lastOut == entity)
	{
		return;
	}
	list.lastOut = entity;
	if (list.things.empty())
	{
		return;
	}
	// The last takes the place of the one taken out; one not found drops the last all the same
	if (const auto found = std::ranges::find(list.things, entity); found != list.things.end())
	{
		*found = list.things.back();
	}
	list.things.pop_back();
}

std::span<const entt::entity> BlockLists::OnBlock(glm::ivec2 block) const
{
	const auto index = BlockIndex(block);
	if (!index.has_value() || !_onLand.at(*index))
	{
		return {};
	}
	return _blocks.at(*index).things;
}

void BlockLists::Clear()
{
	for (auto& list : _blocks)
	{
		list = {};
	}
	_off = {};
}

CellLists::CellLists(uint32_t cellsPerSide)
    : _cellsPerSide(cellsPerSide)
    , _cells(static_cast<size_t>(cellsPerSide) * cellsPerSide)
{
}

glm::ivec2 CellLists::CellAt(uint32_t cell) const
{
	return {static_cast<int32_t>(cell / _cellsPerSide), static_cast<int32_t>(cell % _cellsPerSide)};
}

std::optional<uint32_t> CellLists::IndexOf(glm::ivec2 cell) const
{
	if (static_cast<uint32_t>(cell.x) >= _cellsPerSide || static_cast<uint32_t>(cell.y) >= _cellsPerSide)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(cell.x) * _cellsPerSide + static_cast<uint32_t>(cell.y);
}

void CellLists::Insert(uint32_t cell, entt::entity entity, Placement placement)
{
	auto& lists = _cells.at(cell);
	if (lists.fixed.empty() && lists.mobile.empty())
	{
		_used.push_back(cell);
	}
	switch (placement)
	{
	case Placement::FixedFront:
		lists.fixed.insert(lists.fixed.begin(), entity);
		break;
	case Placement::FixedBack:
		lists.fixed.push_back(entity);
		break;
	case Placement::MobileFront:
		lists.mobile.insert(lists.mobile.begin(), entity);
		break;
	}
	_blocks.Insert(CellAt(cell), entity);
}

void CellLists::Remove(uint32_t cell, entt::entity entity)
{
	_blocks.Remove(CellAt(cell), entity);
	auto& lists = _cells.at(cell);
	if (const auto found = std::ranges::find(lists.fixed, entity); found != lists.fixed.end())
	{
		lists.fixed.erase(found);
		return;
	}
	if (const auto found = std::ranges::find(lists.mobile, entity); found != lists.mobile.end())
	{
		lists.mobile.erase(found);
	}
}

std::span<const entt::entity> CellLists::Fixed(uint32_t cell) const
{
	return _cells.at(cell).fixed;
}

std::span<const entt::entity> CellLists::Mobile(uint32_t cell) const
{
	return _cells.at(cell).mobile;
}

std::vector<entt::entity> CellLists::All(uint32_t cell) const
{
	const auto& lists = _cells.at(cell);
	std::vector<entt::entity> all;
	all.reserve(lists.fixed.size() + lists.mobile.size());
	all.insert(all.end(), lists.fixed.begin(), lists.fixed.end());
	all.insert(all.end(), lists.mobile.begin(), lists.mobile.end());
	return all;
}

void CellLists::Clear()
{
	for (const auto cell : _used)
	{
		_cells.at(cell).fixed.clear();
		_cells.at(cell).mobile.clear();
	}
	_used.clear();
	_blocks.Clear();
}

Outline openblack::ecs::map_cells::OutlineOfBox(glm::vec3 position, const glm::mat3& rotation, float scale, glm::vec3 boxCentre,
                                                glm::vec3 boxHalfSize)
{
	// The game keeps positions as map positions
	const glm::vec3 placed {openblack::map_coords::Quantise(position.x), position.y,
	                        openblack::map_coords::Quantise(position.z)};
	const glm::vec3 centre = placed + rotation * (boxCentre * scale);
	glm::vec2 axis {(rotation * glm::vec3(1.0f, 0.0f, 0.0f)).x, (rotation * glm::vec3(1.0f, 0.0f, 0.0f)).z};
	axis = glm::length(axis) > 0.0f ? glm::normalize(axis) : glm::vec2(1.0f, 0.0f);
	return {.centre = {centre.x, centre.z},
	        .halfSize = {boxHalfSize.x * scale, boxHalfSize.z * scale},
	        .halfDiagonal = scale * glm::length(boxHalfSize),
	        .axis = axis};
}

OutlineCircles openblack::ecs::map_cells::CirclesOf(const Outline& outline)
{
	// Neither side is taken as less than a unit
	const float x = std::max(outline.halfSize.x, 1.0f);
	const float z = std::max(outline.halfSize.y, 1.0f);
	const bool longAlongZ = x <= z;
	const float longSide = longAlongZ ? z : x;
	const float shortSide = longAlongZ ? x : z;
	OutlineCircles circles;
	if (longSide / shortSide <= k_RoundBoxRatio)
	{
		circles.bounds = {.centre = outline.centre, .radius = longSide};
		return circles;
	}
	circles.bounds = {.centre = outline.centre, .radius = std::sqrt(z * z + x * x)};
	// A row of circles the box's width across, spread evenly along its length
	const int32_t count = FtoL(longSide / shortSide) + 1;
	const float length = longSide + longSide;
	const float step = length / static_cast<float>(count);
	const float cosine = outline.axis.x;
	const float sine = outline.axis.y;
	circles.row.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; ++i)
	{
		const float along = (static_cast<float>(i) + 0.5f) * step - length * 0.5f;
		const float localX = longAlongZ ? 0.0f : along;
		const float localZ = longAlongZ ? along : 0.0f;
		const glm::vec2 centre {cosine * localX + -sine * localZ + outline.centre.x,
		                        sine * localX + cosine * localZ + outline.centre.y};
		circles.row.push_back({.centre = centre, .radius = shortSide});
	}
	return circles;
}

namespace
{
/// Whether two circles meet, touching counts, as (a + b)^2 summed term by term
bool Meet(const openblack::ecs::map_cells::Circle& first, const openblack::ecs::map_cells::Circle& second)
{
	const float dx = first.centre.x - second.centre.x;
	const float dz = first.centre.y - second.centre.y;
	const float both = second.radius * first.radius;
	return dx * dx + dz * dz <= both + both + second.radius * second.radius + first.radius * first.radius;
}
} // namespace

bool openblack::ecs::map_cells::Touches(const OutlineCircles& circles, glm::ivec2 cell)
{
	const Circle test {.centre = {static_cast<float>(cell.x * 10 + 5), static_cast<float>(cell.y * 10 + 5)},
	                   .radius = k_CellTestRadius};
	if (!Meet(test, circles.bounds))
	{
		return false;
	}
	return circles.row.empty() || std::ranges::any_of(circles.row, [&test](const Circle& c) { return Meet(c, test); });
}

std::vector<glm::ivec2> openblack::ecs::map_cells::CellsCovered(const Outline& outline, uint32_t cellsPerSide)
{
	const float reach = outline.halfDiagonal + 1.0f;
	int32_t minX = FtoL((outline.centre.x - reach) * 0.1f);
	int32_t maxX = FtoL((outline.centre.x + reach) * 0.1f);
	int32_t minZ = FtoL((outline.centre.y - reach) * 0.1f);
	int32_t maxZ = FtoL((outline.centre.y + reach) * 0.1f);
	if (minX < 0)
	{
		minX = 0;
		maxX = std::max(maxX, 0);
	}
	if (minZ < 0)
	{
		minZ = 0;
		maxZ = std::max(maxZ, 0);
	}
	const auto circles = CirclesOf(outline);
	std::vector<glm::ivec2> covered;
	for (int32_t x = minX; x <= maxX; ++x)
	{
		for (int32_t z = minZ; z <= maxZ; ++z)
		{
			if (static_cast<uint32_t>(x) < cellsPerSide && static_cast<uint32_t>(z) < cellsPerSide && Touches(circles, {x, z}))
			{
				covered.emplace_back(x, z);
			}
		}
	}
	if (covered.empty())
	{
		// Touching nothing, it is put in the middle cell of the square, if that is on the map
		const int32_t width = maxX - minX + 1;
		const int32_t depth = maxZ - minZ + 1;
		const glm::ivec2 middle {minX + width / 2, minZ + depth / 2};
		if (static_cast<uint32_t>(middle.x) < cellsPerSide && static_cast<uint32_t>(middle.y) < cellsPerSide)
		{
			covered.push_back(middle);
		}
	}
	return covered;
}
