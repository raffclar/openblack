/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Hydrology.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <queue>

using namespace openblack::landgen;
using namespace openblack::landgen::hydrology;

namespace
{
/// The rise from a corner to the next on flat filled ground, so that water always has a way down
constexpr float k_FlatRise = 0.001f;
/// The channel from a lake to the sea is three corners wide at its floor, and its banks rise this much a corner
constexpr int k_ChannelBank = 4;
constexpr float k_BankRise = 4.0f;

struct Entry
{
	float height;
	uint32_t index;
	/// The lower height first, then the lower index, so that the order is the same on every platform
	bool operator>(const Entry& other) const { return height != other.height ? height > other.height : index > other.index; }
};
} // namespace

Drainage hydrology::Drain(const Grid<float>& heights, float seaLevel)
{
	const int side = heights.side;
	Drainage drainage {.filled = heights, .receivers = std::vector<int32_t>(heights.values.size(), -1), .order = {}};
	drainage.order.reserve(heights.values.size());
	std::vector<uint8_t> done(heights.values.size(), 0);
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			if (x == 0 || z == 0 || x == side - 1 || z == side - 1)
			{
				const auto index = heights.Index(x, z);
				done[index] = 1;
				open.push({.height = heights.values[index], .index = static_cast<uint32_t>(index)});
			}
		}
	}
	while (!open.empty())
	{
		const auto entry = open.top();
		open.pop();
		drainage.order.push_back(entry.index);
		const int x = heights.XOf(entry.index);
		const int z = heights.ZOf(entry.index);
		for (const auto& step : k_Neighbours)
		{
			const int nx = x + step.dx;
			const int nz = z + step.dz;
			if (!heights.Contains(nx, nz))
			{
				continue;
			}
			const auto next = heights.Index(nx, nz);
			if (done[next] != 0)
			{
				continue;
			}
			done[next] = 1;
			auto& filled = drainage.filled.values[next];
			// Ground at or below the sea level that the sea reaches stays as it is: it is the sea
			if (!(filled <= seaLevel && entry.height <= seaLevel))
			{
				filled = std::max(filled, entry.height + k_FlatRise);
			}
			open.push({.height = filled, .index = static_cast<uint32_t>(next)});
		}
	}

	// The steepest way down on the filled heights. Each filled corner above the sea has a lower neighbour.
	for (size_t index = 0; index < heights.values.size(); ++index)
	{
		const float height = drainage.filled.values[index];
		if (height <= seaLevel)
		{
			continue;
		}
		const int x = heights.XOf(index);
		const int z = heights.ZOf(index);
		float steepest = 0.0f;
		for (size_t i = 0; i < k_Neighbours.size(); ++i)
		{
			const int nx = x + k_Neighbours[i].dx;
			const int nz = z + k_Neighbours[i].dz;
			if (!heights.Contains(nx, nz))
			{
				continue;
			}
			const auto next = heights.Index(nx, nz);
			const float slope = (height - drainage.filled.values[next]) / k_NeighbourDistances[i];
			if (slope > steepest)
			{
				steepest = slope;
				drainage.receivers[index] = static_cast<int32_t>(next);
			}
		}
	}
	return drainage;
}

Grid<float> hydrology::Accumulate(const Drainage& drainage, const Grid<float>& rain)
{
	Grid<float> flow = rain;
	for (auto it = drainage.order.rbegin(); it != drainage.order.rend(); ++it)
	{
		const auto receiver = drainage.receivers[*it];
		if (receiver >= 0)
		{
			flow.values[static_cast<size_t>(receiver)] += flow.values[*it];
		}
	}
	return flow;
}

Lakes hydrology::FormLakes(Grid<float>& heights, const Drainage& drainage, const LakeSettings& settings)
{
	const auto count = heights.values.size();
	Lakes lakes {.seaLakes = {}, .wet = Grid<uint8_t>(heights.side, 0)};
	std::vector<uint8_t> inBasin(count, 0);
	for (size_t index = 0; index < count; ++index)
	{
		inBasin[index] = drainage.filled.values[index] - heights.values[index] > settings.minDepth ? 1 : 0;
	}
	std::vector<uint8_t> seen(count, 0);
	std::vector<uint32_t> basin;
	std::vector<uint32_t> stack;
	for (size_t start = 0; start < count; ++start)
	{
		if (inBasin[start] == 0 || seen[start] != 0)
		{
			continue;
		}
		// The corners of the basin, joined along the axes and the diagonals
		basin.clear();
		stack.assign(1, static_cast<uint32_t>(start));
		seen[start] = 1;
		float spill = 0.0f;
		while (!stack.empty())
		{
			const auto index = stack.back();
			stack.pop_back();
			basin.push_back(index);
			spill = std::max(spill, drainage.filled.values[index]);
			const int x = heights.XOf(index);
			const int z = heights.ZOf(index);
			for (const auto& step : k_Neighbours)
			{
				if (!heights.Contains(x + step.dx, z + step.dz))
				{
					continue;
				}
				const auto next = heights.Index(x + step.dx, z + step.dz);
				if (inBasin[next] != 0 && seen[next] == 0)
				{
					seen[next] = 1;
					stack.push_back(static_cast<uint32_t>(next));
				}
			}
		}
		std::ranges::sort(basin);

		if (static_cast<int>(basin.size()) >= settings.minCorners && spill <= settings.seaLakeSpill)
		{
			for (const auto index : basin)
			{
				heights.values[index] = std::min(heights.values[index], settings.bed);
			}
			// The channel follows the water from the lowest corner of the lake down to the sea, with banks that slope up
			// from it
			auto index = static_cast<int32_t>(
			    *std::ranges::min_element(basin, {}, [&heights](uint32_t corner) { return heights.values[corner]; }));
			while (index >= 0)
			{
				const int x = heights.XOf(static_cast<size_t>(index));
				const int z = heights.ZOf(static_cast<size_t>(index));
				for (int dx = -k_ChannelBank; dx <= k_ChannelBank; ++dx)
				{
					for (int dz = -k_ChannelBank; dz <= k_ChannelBank; ++dz)
					{
						if (!heights.Contains(x + dx, z + dz))
						{
							continue;
						}
						const float distance = std::sqrt(static_cast<float>((dx * dx) + (dz * dz)));
						auto& height = heights.At(x + dx, z + dz);
						height = std::min(height, settings.channel + (std::max(distance - 1.0f, 0.0f) * k_BankRise));
					}
				}
				index = drainage.receivers[static_cast<size_t>(index)];
			}
			lakes.seaLakes.push_back(basin);
			continue;
		}
		const bool wet = static_cast<int>(basin.size()) >= settings.minCorners;
		for (const auto index : basin)
		{
			heights.values[index] = drainage.filled.values[index];
			if (wet)
			{
				lakes.wet.values[index] = 1;
			}
		}
	}
	return lakes;
}

std::vector<River> hydrology::TraceRivers(const Drainage& drainage, const Grid<float>& flow, const RiverSettings& settings)
{
	const auto count = flow.values.size();
	const auto onRiver = [&](size_t index) {
		return flow.values[index] >= settings.sourceFlow && drainage.receivers[index] >= 0;
	};
	std::vector<uint8_t> riverAbove(count, 0);
	for (size_t index = 0; index < count; ++index)
	{
		if (onRiver(index))
		{
			riverAbove[static_cast<size_t>(drainage.receivers[index])] = 1;
		}
	}

	std::vector<River> rivers;
	std::vector<uint8_t> taken(count, 0);
	for (size_t source = 0; source < count; ++source)
	{
		if (!onRiver(source) || riverAbove[source] != 0)
		{
			continue;
		}
		River river;
		auto index = static_cast<int32_t>(source);
		bool joins = false;
		while (index >= 0)
		{
			const auto corner = static_cast<size_t>(index);
			river.corners.push_back(static_cast<uint32_t>(corner));
			if (taken[corner] != 0)
			{
				joins = true;
				break;
			}
			taken[corner] = 1;
			const auto next = drainage.receivers[corner];
			if (next < 0)
			{
				break;
			}
			index = next;
		}
		if (!joins && static_cast<int>(river.corners.size()) < settings.minCorners)
		{
			continue;
		}
		const auto spacing = static_cast<size_t>(std::max(settings.pointSpacing, 1));
		for (size_t i = 0; i < river.corners.size(); ++i)
		{
			if (i % spacing == 0 || i + 1 == river.corners.size())
			{
				const auto corner = river.corners[i];
				river.points.push_back({.x = static_cast<float>(flow.XOf(corner)), .z = static_cast<float>(flow.ZOf(corner))});
			}
		}
		if (river.points.size() >= 2)
		{
			rivers.push_back(std::move(river));
		}
	}
	return rivers;
}

void hydrology::CarveRivers(Grid<float>& heights, const std::vector<River>& rivers, const Grid<float>& flow,
                            const RiverSettings& settings)
{
	for (const auto& river : rivers)
	{
		float bed = heights.values[river.corners.front()];
		for (const auto corner : river.corners)
		{
			const float original = heights.values[corner];
			const float depth =
			    std::min(settings.depth * std::sqrt(flow.values[corner] / settings.sourceFlow), settings.maxDepth);
			// The bed never rises downstream, and stays above the sea where the ground is above it
			float target = std::min(bed, original - depth);
			if (original > settings.lowestBed)
			{
				target = std::max(target, std::min(settings.lowestBed, original));
			}
			bed = target;
			const int x = heights.XOf(corner);
			const int z = heights.ZOf(corner);
			// The banks slope up from the bed, two corners out
			for (int dx = -2; dx <= 2; ++dx)
			{
				for (int dz = -2; dz <= 2; ++dz)
				{
					if (!heights.Contains(x + dx, z + dz))
					{
						continue;
					}
					const float distance = std::sqrt(static_cast<float>((dx * dx) + (dz * dz)));
					auto& height = heights.At(x + dx, z + dz);
					height = std::min(height, bed + (distance * depth * 0.7f));
				}
			}
		}
	}
}
