/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Lakes.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <numeric>

#include "landgen/Hydrology.h"
#include "landgen/Random.h"

using namespace openblack::landgen;
using namespace openblack::landgen::lakes;

namespace
{
struct Candidate
{
	float level;
	/// Corners under the water of the lake, from which its water fills again after a change
	std::vector<uint32_t> seeds;
	std::vector<uint32_t> water;
	std::vector<uint32_t> region;
	/// A lake that a dry circle made too small or too shallow. Its basin stays dry.
	bool dry {false};
};

/// The part of the line from a corner of a lake to a corner outside it that the lake's water covers. This agrees with
/// the surface that the game draws. A corner outside the lake counts as land at least shoreReach above the level.
float Cover(float from, float to, float level, float shoreReach)
{
	if (from >= level)
	{
		return 0.0f;
	}
	const float cutTo = std::max(to, level + shoreReach);
	return std::clamp((level - from) / (cutTo - from), 0.0f, 1.0f);
}

/// The corners next to a corner, along the axes and the diagonals, that are on the map
template <typename Visit>
void EachNeighbour(const Grid<float>& heights, uint32_t index, Visit&& visit)
{
	const int x = heights.XOf(index);
	const int z = heights.ZOf(index);
	for (const auto& step : k_Neighbours)
	{
		if (heights.Contains(x + step.dx, z + step.dz))
		{
			visit(static_cast<uint32_t>(heights.Index(x + step.dx, z + step.dz)));
		}
	}
}

/// The water of a lake: the corners below its level that join its seeds. The level is below the height where the basin
/// spills over, so the water stays in the basin. Then its shore: the corners next to the water, at most shoreSteps
/// corners from it, that are less than shoreReach above the level.
void FillCandidate(Candidate& lake, const Grid<float>& heights, const Settings& settings, std::vector<int32_t>& mark,
                   int32_t stamp)
{
	lake.water.clear();
	lake.region.clear();
	if (lake.dry)
	{
		return;
	}
	std::vector<uint32_t> stack;
	for (const auto seed : lake.seeds)
	{
		if (heights.values[seed] < lake.level && mark[seed] != stamp)
		{
			mark[seed] = stamp;
			stack.push_back(seed);
		}
	}
	while (!stack.empty())
	{
		const auto index = stack.back();
		stack.pop_back();
		lake.water.push_back(index);
		EachNeighbour(heights, index, [&](uint32_t next) {
			if (mark[next] != stamp && heights.values[next] < lake.level)
			{
				mark[next] = stamp;
				stack.push_back(next);
			}
		});
	}
	lake.region = lake.water;
	std::vector<uint32_t> frontier = lake.water;
	std::vector<uint32_t> nextFrontier;
	for (int step = 0; step < settings.shoreSteps && !frontier.empty(); ++step)
	{
		nextFrontier.clear();
		for (const auto index : frontier)
		{
			EachNeighbour(heights, index, [&](uint32_t next) {
				const float height = heights.values[next];
				if (mark[next] != stamp && height >= lake.level && height < lake.level + settings.shoreReach)
				{
					mark[next] = stamp;
					nextFrontier.push_back(next);
				}
			});
		}
		lake.region.insert(lake.region.end(), nextFrontier.begin(), nextFrontier.end());
		std::swap(frontier, nextFrontier);
	}
	std::ranges::sort(lake.water);
	std::ranges::sort(lake.region);
}

/// Lowers the level of a lake until its water and its shore cover no corner in a dry circle. A corner of the shore is
/// less than shoreReach above the level, so the level goes to shoreReach and the freeboard under the lowest such corner.
/// When the lower lake has fewer corners than a lake needs, or is not deep enough, the lake becomes dry.
void KeepDry(Candidate& lake, const Grid<float>& heights, std::span<const Keep> dry, const Settings& settings,
             std::vector<int32_t>& mark, int32_t& stamp)
{
	if (dry.empty() || lake.dry)
	{
		return;
	}
	const auto inDry = [&](uint32_t corner) {
		const auto x = static_cast<float>(heights.XOf(corner));
		const auto z = static_cast<float>(heights.ZOf(corner));
		return std::ranges::any_of(dry, [x, z](const Keep& keep) {
			const float dx = x - keep.x;
			const float dz = z - keep.z;
			return (dx * dx) + (dz * dz) <= keep.radius * keep.radius;
		});
	};
	bool lowered = false;
	for (bool again = true; again;)
	{
		again = false;
		float cap = lake.level;
		for (const auto corner : lake.region)
		{
			if (inDry(corner))
			{
				cap = std::min(cap, heights.values[corner] - settings.shoreReach - settings.freeboard);
			}
		}
		if (cap < lake.level)
		{
			lake.level = cap;
			FillCandidate(lake, heights, settings, mark, stamp++);
			lowered = true;
			again = true;
		}
	}
	if (!lowered)
	{
		return;
	}
	float lowest = std::numeric_limits<float>::max();
	for (const auto corner : lake.water)
	{
		lowest = std::min(lowest, heights.values[corner]);
	}
	if (static_cast<int>(lake.water.size()) < settings.minCorners || lake.level - lowest < settings.minDeepest)
	{
		lake.dry = true;
		lake.water.clear();
		lake.region.clear();
	}
}

size_t Root(std::vector<size_t>& parents, size_t index)
{
	while (parents[index] != index)
	{
		parents[index] = parents[parents[index]];
		index = parents[index];
	}
	return index;
}
} // namespace

std::vector<Lake> lakes::Find(Grid<float>& heights, float seaLevel, const Settings& settings, std::span<const Keep> dry)
{
	const auto count = heights.values.size();
	const auto drainage = hydrology::Drain(heights, seaLevel);

	// The basins: the corners that the filled heights cover, joined along the axes and the diagonals
	std::vector<Candidate> candidates;
	std::vector<uint8_t> seen(count, 0);
	std::vector<uint32_t> stack;
	const auto inBasin = [&](size_t index) {
		return drainage.filled.values[index] > seaLevel &&
		       drainage.filled.values[index] - heights.values[index] > settings.minDepth;
	};
	for (size_t start = 0; start < count; ++start)
	{
		if (seen[start] != 0 || !inBasin(start))
		{
			continue;
		}
		Candidate candidate {.level = 0.0f, .seeds = {}, .water = {}, .region = {}, .dry = false};
		float spill = drainage.filled.values[start];
		float lowest = heights.values[start];
		stack.assign(1, static_cast<uint32_t>(start));
		seen[start] = 1;
		while (!stack.empty())
		{
			const auto index = stack.back();
			stack.pop_back();
			candidate.seeds.push_back(index);
			spill = std::min(spill, drainage.filled.values[index]);
			lowest = std::min(lowest, heights.values[index]);
			EachNeighbour(heights, index, [&](uint32_t next) {
				if (seen[next] == 0 && inBasin(next))
				{
					seen[next] = 1;
					stack.push_back(next);
				}
			});
		}
		candidate.level = spill - settings.freeboard;
		if (static_cast<int>(candidate.seeds.size()) >= settings.minCorners &&
		    candidate.level - lowest >= settings.minDeepest && candidate.level >= seaLevel + settings.lowestAboveSea)
		{
			std::ranges::sort(candidate.seeds);
			candidates.push_back(std::move(candidate));
		}
	}

	// Two lakes whose water or shore share a corner, or whose surfaces meet between corners next to each other, become
	// one lake. Its level
	// is the lower level, where the water of the two spills over, and the bed of the higher lake goes down under it.
	std::vector<int32_t> mark(count, -1);
	std::vector<int32_t> owner(count, -1);
	int32_t stamp = 0;
	for (size_t round = 0; round <= candidates.size(); ++round)
	{
		for (auto& candidate : candidates)
		{
			FillCandidate(candidate, heights, settings, mark, stamp++);
			KeepDry(candidate, heights, dry, settings, mark, stamp);
		}
		std::vector<size_t> parents(candidates.size());
		std::iota(parents.begin(), parents.end(), size_t {0});
		bool joined = false;
		const auto join = [&](size_t a, size_t b) {
			const auto rootA = Root(parents, a);
			const auto rootB = Root(parents, b);
			if (rootA != rootB)
			{
				parents[std::max(rootA, rootB)] = std::min(rootA, rootB);
				joined = true;
			}
		};
		std::ranges::fill(owner, -1);
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			for (const auto corner : candidates[i].region)
			{
				if (owner[corner] >= 0)
				{
					join(static_cast<size_t>(owner[corner]), i);
				}
				else
				{
					owner[corner] = static_cast<int32_t>(i);
				}
			}
		}
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			for (const auto corner : candidates[i].region)
			{
				EachNeighbour(heights, corner, [&](uint32_t next) {
					if (owner[next] < 0 || static_cast<size_t>(owner[next]) == i)
					{
						return;
					}
					// The surfaces of the two lakes meet when together they cover the line between the two corners
					const auto& other = candidates[static_cast<size_t>(owner[next])];
					const float from = heights.values[corner];
					const float to = heights.values[next];
					if (Cover(from, to, candidates[i].level, settings.shoreReach) +
					        Cover(to, from, other.level, settings.shoreReach) >=
					    1.0f)
					{
						join(static_cast<size_t>(owner[next]), i);
					}
				});
			}
		}
		if (!joined)
		{
			break;
		}

		std::vector<Candidate> merged;
		std::vector<int32_t> slot(candidates.size(), -1);
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			const auto root = Root(parents, i);
			if (slot[root] < 0)
			{
				slot[root] = static_cast<int32_t>(merged.size());
				merged.push_back(
				    {.level = candidates[i].level, .seeds = {}, .water = {}, .region = {}, .dry = candidates[i].dry});
			}
			auto& target = merged[static_cast<size_t>(slot[root])];
			target.level = std::min(target.level, candidates[i].level);
		}
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			auto& target = merged[static_cast<size_t>(slot[Root(parents, i)])];
			// The bed of each joined lake goes down under the level of the joined lake, so that its water stays
			const float bed = std::floor(target.level - settings.bedBelow);
			for (const auto corner : candidates[i].water)
			{
				auto& height = heights.values[corner];
				if (height > target.level - settings.bedBelow)
				{
					height = bed;
				}
				target.seeds.push_back(corner);
			}
		}
		for (auto& lake : merged)
		{
			std::ranges::sort(lake.seeds);
		}
		candidates = std::move(merged);
	}

	std::vector<Lake> lakes;
	lakes.reserve(candidates.size());
	for (auto& candidate : candidates)
	{
		if (!candidate.water.empty())
		{
			lakes.push_back({.level = candidate.level, .corners = std::move(candidate.region)});
		}
	}
	std::ranges::sort(lakes, {}, [](const Lake& lake) { return lake.corners.front(); });
	return lakes;
}

void lakes::DigHollows(Grid<float>& heights, const Grid<uint8_t>& keepAway, const std::vector<Keep>& keep, uint64_t seed,
                       const DigSettings& settings)
{
	const int side = heights.side;
	Random random(seed);
	std::vector<Keep> dug;
	for (int tries = 0; tries < settings.count * 60 && static_cast<int>(dug.size()) < settings.count; ++tries)
	{
		const float radius = random.NextRange(settings.smallest, settings.largest);
		const float outer = radius + 2.0f;
		const float reach = radius + settings.clearance;
		const float cx = random.NextRange(reach, static_cast<float>(side - 1) - reach);
		const float cz = random.NextRange(reach, static_cast<float>(side - 1) - reach);
		if (cx >= static_cast<float>(side - 1) - reach || cz >= static_cast<float>(side - 1) - reach)
		{
			continue;
		}
		const auto apart = [&](const Keep& other) {
			const float dx = other.x - cx;
			const float dz = other.z - cz;
			const float distance = other.radius + reach;
			return (dx * dx) + (dz * dz) > distance * distance;
		};
		if (!std::ranges::all_of(keep, apart) || !std::ranges::all_of(dug, apart))
		{
			continue;
		}
		const int x0 = static_cast<int>(std::floor(cx - reach));
		const int x1 = static_cast<int>(std::ceil(cx + reach));
		const int z0 = static_cast<int>(std::floor(cz - reach));
		const int z1 = static_cast<int>(std::ceil(cz + reach));
		bool fits = true;
		float low = std::numeric_limits<float>::max();
		float high = 0.0f;
		float rim = std::numeric_limits<float>::max();
		for (int x = x0; x <= x1 && fits; ++x)
		{
			for (int z = z0; z <= z1; ++z)
			{
				const float dx = static_cast<float>(x) - cx;
				const float dz = static_cast<float>(z) - cz;
				const float distance = std::sqrt((dx * dx) + (dz * dz));
				if (distance > reach)
				{
					continue;
				}
				if (!heights.Contains(x, z) || keepAway.At(x, z) != 0)
				{
					fits = false;
					break;
				}
				if (distance <= outer)
				{
					const float height = heights.At(x, z);
					low = std::min(low, height);
					high = std::max(high, height);
					if (distance >= radius)
					{
						rim = std::min(rim, height);
					}
				}
			}
		}
		if (!fits || low < settings.lowest || high - low > settings.roughest)
		{
			continue;
		}
		// The ground goes down from the lowest point of the rim to the depth at the middle, as a bowl
		for (int x = x0; x <= x1; ++x)
		{
			for (int z = z0; z <= z1; ++z)
			{
				const float dx = static_cast<float>(x) - cx;
				const float dz = static_cast<float>(z) - cz;
				const float part = ((dx * dx) + (dz * dz)) / (radius * radius);
				if (part < 1.0f)
				{
					auto& height = heights.At(x, z);
					height = std::min(height, rim - (settings.depth * (1.0f - part)));
				}
			}
		}
		dug.push_back({.x = cx, .z = cz, .radius = radius});
	}
}
