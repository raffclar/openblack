/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CoarseEdges.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace openblack::graphics::coarse_edges
{

std::vector<size_t> PaintOrder(std::span<const PaintRequest> requests, glm::vec3 camera)
{
	// A request for a block that already waits keeps the first place. A new request goes before the requests that came
	// before it.
	std::vector<PaintRequest> waiting;
	for (const auto& request : requests)
	{
		if (std::ranges::none_of(waiting, [&](const PaintRequest& kept) { return kept.block == request.block; }))
		{
			waiting.insert(waiting.begin(), request);
		}
	}
	// The distance in floats, in the game's order of the sums
	const auto distance = [&camera](const PaintRequest& request) {
		const float dx = request.centre.x - camera.x;
		const float dy = 0.0f - camera.y;
		const float dz = request.centre.y - camera.z;
		const float squared = ((dz * dz) + (dy * dy)) + (dx * dx);
		return std::sqrt(squared);
	};
	std::vector<std::pair<float, size_t>> order;
	order.reserve(waiting.size());
	std::ranges::transform(waiting, std::back_inserter(order),
	                       [&](const PaintRequest& request) { return std::pair {distance(request), request.block}; });
	// Only a farther block moves behind a nearer one, thus blocks at the same distance keep their order
	std::ranges::stable_sort(order, {}, &std::pair<float, size_t>::first);
	std::vector<size_t> blocks;
	blocks.reserve(order.size());
	std::ranges::transform(order, std::back_inserter(blocks), &std::pair<float, size_t>::second);
	return blocks;
}

std::vector<size_t> PaintsDue(const std::vector<bool>& waiting, std::span<const size_t> drawn,
                              const std::function<bool(size_t)>& painted, const std::function<bool(size_t)>& busy)
{
	std::vector<size_t> due;
	std::vector<bool> taken(waiting.size(), false);
	const auto take = [&](size_t block) {
		if (block < waiting.size() && waiting[block] && !taken[block] && !busy(block))
		{
			taken[block] = true;
			due.push_back(block);
		}
	};
	for (size_t block = 0; block < waiting.size(); ++block)
	{
		if (waiting[block] && !painted(block))
		{
			take(block);
		}
	}
	for (const auto block : drawn)
	{
		take(block);
	}
	return due;
}

FramePaints MakeFramePaints(std::span<const glm::ivec2> places, std::span<const size_t> order,
                            const std::bitset<k_Blocks>& paintedBefore)
{
	FramePaints paints {};
	for (size_t block = 0; block < places.size(); ++block)
	{
		if (OnMap(places[block]))
		{
			auto& paint = paints.at(PlaceOf(places[block]));
			paint.block = static_cast<uint16_t>(block);
			paint.paintedBefore = paintedBefore.test(PlaceOf(places[block]));
		}
	}
	uint16_t rank = 0;
	for (const auto block : order)
	{
		if (block < places.size() && OnMap(places[block]))
		{
			auto& paint = paints.at(PlaceOf(places[block]));
			if (paint.rank == 0)
			{
				paint.rank = ++rank;
			}
		}
	}
	return paints;
}

std::vector<float> FramePaintTexels(const FramePaints& paints)
{
	std::vector<float> texels(paints.size() * 4, 0.0f);
	for (size_t place = 0; place < paints.size(); ++place)
	{
		const auto& paint = paints.at(place);
		texels[(place * 4) + 0] = static_cast<float>(paint.rank);
		texels[(place * 4) + 1] = paint.paintedBefore ? 1.0f : 0.0f;
		texels[(place * 4) + 2] = paint.block.has_value() ? static_cast<float>(*paint.block + 1) : 0.0f;
	}
	return texels;
}

namespace
{

FramePaint PaintAt(const FramePaints& paints, glm::ivec2 block)
{
	return OnMap(block) ? paints.at(PlaceOf(block)) : FramePaint {};
}

/// A cell that is not at a corner, shared by the block and the neighbour across its edge
Levels EdgeCellAfter(const FramePaints& paints, glm::ivec2 block, Cell cell, glm::ivec2 neighbour, Cell neighbourCell,
                     const CellLevels& kept, const CellLevels& fresh)
{
	auto own = PaintAt(paints, block);
	auto other = PaintAt(paints, neighbour);
	auto ownCell = kept(block, cell);
	if (own.rank == 0 && other.rank == 0)
	{
		return ownCell;
	}
	auto otherCell = other.paintedBefore ? kept(neighbour, neighbourCell) : Levels {0};
	const bool ownFirst = own.rank != 0 && (other.rank == 0 || own.rank < other.rank);
	for (const bool ownTurn : {ownFirst, !ownFirst})
	{
		if (ownTurn && own.rank != 0)
		{
			ownCell = fresh(block, cell);
			if (other.paintedBefore)
			{
				ownCell = Mix(ownCell, otherCell);
				otherCell = ownCell;
			}
			own.paintedBefore = true;
		}
		else if (!ownTurn && other.rank != 0)
		{
			otherCell = fresh(neighbour, neighbourCell);
			if (own.paintedBefore)
			{
				otherCell = Mix(otherCell, ownCell);
				ownCell = otherCell;
			}
			other.paintedBefore = true;
		}
	}
	return ownCell;
}

/// A cell at a corner. The four blocks round the corner are i along x and j along z from the lowest, member 2i + j. A
/// block's cell at the corner is at its high edge along each axis where its index is 0.
Levels CornerCellAfter(const FramePaints& paints, int32_t samples, glm::ivec2 block, Cell cell, const CellLevels& kept,
                       const CellLevels& fresh)
{
	const auto last = samples - 1;
	const glm::ivec2 ownIndex {cell.y == last ? 0 : 1, cell.x == last ? 0 : 1};
	const auto lowest = block - ownIndex;
	const auto indexOf = [](size_t member) { return glm::ivec2(static_cast<int>(member / 2), static_cast<int>(member % 2)); };
	const auto cellOf = [last](glm::ivec2 index) { return Cell(index.y == 0 ? last : 0, index.x == 0 ? last : 0); };
	std::array<FramePaint, 4> members {};
	std::array<Levels, 4> cells {};
	for (size_t m = 0; m < members.size(); ++m)
	{
		const auto index = indexOf(m);
		members.at(m) = PaintAt(paints, lowest + index);
		cells.at(m) = members.at(m).paintedBefore ? kept(lowest + index, cellOf(index)) : Levels {0};
	}
	const auto own = static_cast<size_t>((ownIndex.x * 2) + ownIndex.y);
	if (std::ranges::all_of(members, [](const FramePaint& member) { return member.rank == 0; }))
	{
		return kept(block, cell);
	}
	std::array<size_t, 4> order {0, 1, 2, 3};
	std::ranges::sort(order, {}, [&members](size_t m) { return members.at(m).rank; });
	for (const auto next : order)
	{
		if (members.at(next).rank == 0)
		{
			continue;
		}
		const auto index = indexOf(next);
		auto mixed = fresh(lowest + index, cellOf(index));
		// The member across z, then the member across x
		for (const auto across : {next ^ 1u, next ^ 2u})
		{
			if (members.at(across).paintedBefore)
			{
				mixed = Mix(mixed, cells.at(across));
				cells.at(across) = mixed;
			}
		}
		cells.at(next) = mixed;
		members.at(next).paintedBefore = true;
	}
	return members.at(own).paintedBefore ? cells.at(own) : kept(block, cell);
}

} // namespace

Levels CellAfterFrame(const FramePaints& paints, int32_t samples, glm::ivec2 block, Cell cell, const CellLevels& kept,
                      const CellLevels& fresh)
{
	const auto last = samples - 1;
	const bool atZEdge = cell.x == 0 || cell.x == last;
	const bool atXEdge = cell.y == 0 || cell.y == last;
	if (atZEdge && atXEdge)
	{
		return CornerCellAfter(paints, samples, block, cell, kept, fresh);
	}
	if (cell.x == 0)
	{
		return EdgeCellAfter(paints, block, cell, block - glm::ivec2(0, 1), Cell(last, cell.y), kept, fresh);
	}
	if (cell.x == last)
	{
		return EdgeCellAfter(paints, block, cell, block + glm::ivec2(0, 1), Cell(0, cell.y), kept, fresh);
	}
	if (cell.y == 0)
	{
		return EdgeCellAfter(paints, block, cell, block - glm::ivec2(1, 0), Cell(cell.x, last), kept, fresh);
	}
	return EdgeCellAfter(paints, block, cell, block + glm::ivec2(1, 0), Cell(cell.x, 0), kept, fresh);
}

glm::ivec2 EdgeTexel(glm::ivec2 block, Cell cell, int32_t samples)
{
	const auto row = static_cast<int>(PlaceOf(block));
	const auto last = samples - 1;
	constexpr int k_Side = k_SideTexels;
	if (cell.x == 0)
	{
		return {cell.y, row};
	}
	if (cell.x == last)
	{
		return {k_Side + cell.y, row};
	}
	if (cell.y == 0)
	{
		return {(2 * k_Side) + cell.x, row};
	}
	return {(3 * k_Side) + cell.x, row};
}

} // namespace openblack::graphics::coarse_edges
