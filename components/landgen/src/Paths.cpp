/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Paths.h"

#include <cmath>

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>

#include "landgen/Noise.h"
#include "landgen/Random.h"

using namespace openblack::landgen;
using namespace openblack::landgen::paths;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
/// The step along a segment of a path when the corners under it are found, in corners
constexpr float k_DrawStep = 0.25f;

/// The metres of each corner, and the wander noise of each corner from 0 to 1
struct Costs
{
	Grid<float> metres;
	Grid<float> wander;
};

Costs CostsOf(const Ground& ground, uint64_t seed, const Settings& settings)
{
	const int side = ground.heights.side;
	Costs costs {.metres = Grid<float>(side, 0.0f), .wander = Grid<float>(side, 0.0f)};
	const auto wanderSeed = StageSeed(seed, 80);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			costs.metres.At(x, z) = ground.curve.MetresOf(ground.heights.At(x, z));
			const float noise = noise::Fractal(wanderSeed, static_cast<float>(x) / settings.wanderScale,
			                                   static_cast<float>(z) / settings.wanderScale, 3);
			costs.wander.At(x, z) = std::clamp(0.5f + (0.5f * noise), 0.0f, 1.0f);
		}
	}
	return costs;
}

/// A corner that waits in the search, with the cost of the way through it and the guess of the cost to the end
struct Open
{
	float estimate;
	uint32_t index;

	bool operator>(const Open& other) const
	{
		return estimate > other.estimate || (estimate == other.estimate && index > other.index);
	}
};

int Rounded(float value)
{
	return static_cast<int>(std::floor(value + 0.5f));
}

std::optional<std::vector<Point>> Search(const Ground& ground, const Costs& costs, const Grid<uint8_t>& laid, Point from,
                                         Point to, const Settings& settings)
{
	const auto& grid = ground.heights;
	const int side = grid.side;
	const int startX = std::clamp(Rounded(from.x), 0, side - 1);
	const int startZ = std::clamp(Rounded(from.z), 0, side - 1);
	const int endX = std::clamp(Rounded(to.x), 0, side - 1);
	const int endZ = std::clamp(Rounded(to.z), 0, side - 1);
	const auto start = grid.Index(startX, startZ);
	const auto end = grid.Index(endX, endZ);

	// The guess is the shortest way with every step at the least cost, so the search finds the way of least cost
	const auto guess = [&](int x, int z) {
		const auto dx = static_cast<float>(std::abs(x - endX));
		const auto dz = static_cast<float>(std::abs(z - endZ));
		const float straight = std::max(dx, dz) - std::min(dx, dz);
		return ((std::min(dx, dz) * k_NeighbourDistances[4]) + straight) * settings.reuse;
	};

	std::vector<float> cost(grid.values.size(), std::numeric_limits<float>::max());
	std::vector<int32_t> before(grid.values.size(), -1);
	std::vector<uint8_t> done(grid.values.size(), 0);
	std::priority_queue<Open, std::vector<Open>, std::greater<>> open;
	cost[start] = 0.0f;
	open.push({.estimate = guess(startX, startZ), .index = static_cast<uint32_t>(start)});
	while (!open.empty())
	{
		const auto here = open.top().index;
		open.pop();
		if (done[here] != 0)
		{
			continue;
		}
		done[here] = 1;
		if (here == end)
		{
			break;
		}
		const int x = grid.XOf(here);
		const int z = grid.ZOf(here);
		for (size_t n = 0; n < k_Neighbours.size(); ++n)
		{
			const int nx = x + k_Neighbours[n].dx;
			const int nz = z + k_Neighbours[n].dz;
			if (!grid.Contains(nx, nz))
			{
				continue;
			}
			const auto next = grid.Index(nx, nz);
			if (done[next] != 0 || ground.water.values[next] != 0)
			{
				continue;
			}
			const float length = k_NeighbourDistances[n];
			const float slope = std::abs(costs.metres.values[next] - costs.metres.values[here]) / (length * k_MetresPerCorner);
			if (slope > settings.steepest)
			{
				continue;
			}
			float step = length * (1.0f + (settings.slopeWeight * slope * slope)) *
			             (1.0f + (settings.wander * costs.wander.values[next]));
			if (ground.rivers.values[next] != 0)
			{
				step += settings.riverCost;
			}
			if (laid.values[next] != 0)
			{
				step *= settings.reuse;
			}
			const float total = cost[here] + step;
			if (total < cost[next])
			{
				cost[next] = total;
				before[next] = static_cast<int32_t>(here);
				open.push({.estimate = total + guess(nx, nz), .index = static_cast<uint32_t>(next)});
			}
		}
	}
	if (done[end] == 0)
	{
		return std::nullopt;
	}
	std::vector<Point> way;
	for (auto index = static_cast<int32_t>(end); index >= 0; index = before[static_cast<size_t>(index)])
	{
		const auto at = static_cast<size_t>(index);
		way.push_back({.x = static_cast<float>(grid.XOf(at)), .z = static_cast<float>(grid.ZOf(at))});
	}
	std::ranges::reverse(way);
	return way;
}

bool InClear(float x, float z, std::span<const Clear> clear)
{
	return std::ranges::any_of(clear, [x, z](const Clear& circle) {
		const float dx = x - circle.x;
		const float dz = z - circle.z;
		return (dx * dx) + (dz * dz) < circle.radius * circle.radius;
	});
}

bool InWater(float x, float z, const Grid<uint8_t>& water)
{
	const int cx = Rounded(x);
	const int cz = Rounded(z);
	return !water.Contains(cx, cz) || water.At(cx, cz) != 0;
}

/// The height of the land at a point in corners, between the four corners round it
float HeightAt(const Grid<float>& metres, float x, float z)
{
	const int last = metres.side - 1;
	x = std::clamp(x, 0.0f, static_cast<float>(last));
	z = std::clamp(z, 0.0f, static_cast<float>(last));
	const int x0 = std::min(static_cast<int>(std::floor(x)), last);
	const int z0 = std::min(static_cast<int>(std::floor(z)), last);
	const int x1 = std::min(x0 + 1, last);
	const int z1 = std::min(z0 + 1, last);
	const float fx = x - static_cast<float>(x0);
	const float fz = z - static_cast<float>(z0);
	const float near = metres.At(x0, z0) + ((metres.At(x1, z0) - metres.At(x0, z0)) * fx);
	const float far = metres.At(x0, z1) + ((metres.At(x1, z1) - metres.At(x0, z1)) * fx);
	return near + ((far - near) * fz);
}

/// The steepest rise over run of a step between two points, from the heights at the two ends and at the middle
float SlopeOf(const Grid<float>& metres, Point a, Point b)
{
	const float run = std::sqrt(((b.x - a.x) * (b.x - a.x)) + ((b.z - a.z) * (b.z - a.z))) * k_MetresPerCorner;
	if (run <= 0.0f)
	{
		return 0.0f;
	}
	const float start = HeightAt(metres, a.x, a.z);
	const float middle = HeightAt(metres, (a.x + b.x) / 2.0f, (a.z + b.z) / 2.0f);
	const float end = HeightAt(metres, b.x, b.z);
	return std::max(std::fabs(middle - start), std::fabs(end - middle)) / (run / 2.0f);
}
} // namespace

std::vector<Link> paths::ChainLinks(std::span<const Place> places)
{
	std::vector<Link> links;
	for (size_t i = 1; i < places.size(); ++i)
	{
		size_t nearest = 0;
		float best = std::numeric_limits<float>::max();
		for (size_t j = 0; j < i; ++j)
		{
			const float dx = places[i].x - places[j].x;
			const float dz = places[i].z - places[j].z;
			const float distance = (dx * dx) + (dz * dz);
			if (distance < best)
			{
				best = distance;
				nearest = j;
			}
		}
		links.push_back({.from = static_cast<uint32_t>(nearest), .to = static_cast<uint32_t>(i)});
	}
	return links;
}

std::optional<std::vector<Point>> paths::FindWay(const Ground& ground, const Grid<uint8_t>& laid, Point from, Point to,
                                                 uint64_t seed, const Settings& settings)
{
	return Search(ground, CostsOf(ground, seed, settings), laid, from, to, settings);
}

std::vector<Point> paths::Smooth(std::span<const Point> points, int passes)
{
	std::vector<Point> smooth(points.begin(), points.end());
	if (smooth.size() < 3)
	{
		return smooth;
	}
	std::vector<Point> next = smooth;
	for (int pass = 0; pass < passes; ++pass)
	{
		for (size_t i = 1; i + 1 < smooth.size(); ++i)
		{
			next[i] = {.x = (smooth[i - 1].x + smooth[i].x + smooth[i + 1].x) / 3.0f,
			           .z = (smooth[i - 1].z + smooth[i].z + smooth[i + 1].z) / 3.0f};
		}
		smooth.swap(next);
		next = smooth;
	}
	return smooth;
}

std::vector<Point> paths::Smooth(std::span<const Point> points, int passes, const Grid<float>& metres, float steepest)
{
	std::vector<Point> smooth(points.begin(), points.end());
	if (smooth.size() < 3)
	{
		return smooth;
	}
	std::vector<Point> next = smooth;
	const auto same = [](Point a, Point b) { return a.x == b.x && a.z == b.z; };
	for (int pass = 0; pass < passes; ++pass)
	{
		for (size_t i = 1; i + 1 < smooth.size(); ++i)
		{
			next[i] = {.x = (smooth[i - 1].x + smooth[i].x + smooth[i + 1].x) / 3.0f,
			           .z = (smooth[i - 1].z + smooth[i].z + smooth[i + 1].z) / 3.0f};
		}
		// A step that the pass makes too steep gets back the two points that it had before the pass. This can make the
		// steps next to it too steep, so the check continues until no point changes. A point goes back once at most.
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (size_t i = 0; i + 1 < next.size(); ++i)
			{
				if (same(next[i], smooth[i]) && same(next[i + 1], smooth[i + 1]))
				{
					continue;
				}
				const float limit = std::max(steepest, SlopeOf(metres, smooth[i], smooth[i + 1]));
				if (SlopeOf(metres, next[i], next[i + 1]) > limit)
				{
					next[i] = smooth[i];
					next[i + 1] = smooth[i + 1];
					changed = true;
				}
			}
		}
		smooth.swap(next);
		next = smooth;
	}
	return smooth;
}

std::vector<Path> paths::Lay(const Ground& ground, std::span<const Place> places, std::span<const Link> links, uint64_t seed,
                             const Settings& settings)
{
	const auto costs = CostsOf(ground, seed, settings);
	Grid<uint8_t> laid(ground.heights.side, 0);
	Random random(StageSeed(seed, 81));
	std::vector<Path> paths;
	for (const auto& link : links)
	{
		// Each link takes a random number, so that a link without a way does not change the lanterns of the others
		const bool lit = random.NextUnit() < settings.litShare;
		if (link.from >= places.size() || link.to >= places.size())
		{
			continue;
		}
		const auto& from = places[link.from];
		const auto& to = places[link.to];
		const auto way = Search(ground, costs, laid, {.x = from.x, .z = from.z}, {.x = to.x, .z = to.z}, settings);
		if (!way.has_value())
		{
			continue;
		}
		for (const auto& point : *way)
		{
			laid.At(static_cast<int>(point.x), static_cast<int>(point.z)) = 1;
		}
		paths.push_back(
		    {.link = link, .points = Smooth(*way, settings.smoothPasses, costs.metres, settings.steepest), .lit = lit});
	}
	return paths;
}

namespace
{
Grid<uint8_t> DrawOn(std::span<const Path> paths, const Grid<uint8_t>& water, std::span<const Clear> clear,
                     const Grid<float>* metres, float cliffSlope)
{
	Grid<uint8_t> drawn(water.side, 0);
	// A corner of the square is not covered when a cliff is between it and the corner at the point of the path
	const auto level = [&](int x, int z, int ax, int az) {
		if (metres == nullptr || !metres->Contains(x, z) || !metres->Contains(ax, az))
		{
			return true;
		}
		const auto dx = static_cast<float>(x - ax);
		const auto dz = static_cast<float>(z - az);
		const float rise = cliffSlope * k_MetresPerCorner * std::sqrt((dx * dx) + (dz * dz));
		return std::fabs(metres->At(x, z) - metres->At(ax, az)) <= rise;
	};
	const auto mark = [&](int x, int z, int ax, int az) {
		const auto fx = static_cast<float>(x);
		const auto fz = static_cast<float>(z);
		if (water.Contains(x, z) && water.At(x, z) == 0 && !InClear(fx, fz, clear) && level(x, z, ax, az))
		{
			drawn.At(x, z) = 1;
		}
	};
	// The path covers a square of four corners at each point. The cells between the corners are then all earth, and
	// the path shows as a clear track with soft edges.
	const auto cover = [&mark](int x, int z, int ax, int az) {
		mark(x, z, ax, az);
		mark(x + 1, z, ax, az);
		mark(x, z + 1, ax, az);
		mark(x + 1, z + 1, ax, az);
	};
	for (const auto& path : paths)
	{
		std::optional<std::pair<int, int>> last;
		const auto visit = [&](int x, int z) {
			if (last.has_value())
			{
				if (last->first == x && last->second == z)
				{
					return;
				}
				// A step along a diagonal also covers a corner along an axis, so that the line has no gaps
				if (last->first != x && last->second != z)
				{
					cover(x, last->second, x, z);
				}
			}
			cover(x, z, x, z);
			last = {x, z};
		};
		for (size_t i = 0; i + 1 < path.points.size(); ++i)
		{
			const auto& a = path.points[i];
			const auto& b = path.points[i + 1];
			const float length = std::sqrt(((b.x - a.x) * (b.x - a.x)) + ((b.z - a.z) * (b.z - a.z)));
			const int steps = std::max(1, static_cast<int>(std::ceil(length / k_DrawStep)));
			for (int s = 0; s < steps; ++s)
			{
				const float t = static_cast<float>(s) / static_cast<float>(steps);
				visit(Rounded(a.x + ((b.x - a.x) * t)), Rounded(a.z + ((b.z - a.z) * t)));
			}
		}
		if (!path.points.empty())
		{
			visit(Rounded(path.points.back().x), Rounded(path.points.back().z));
		}
	}
	return drawn;
}
} // namespace

Grid<uint8_t> paths::Draw(std::span<const Path> paths, const Grid<uint8_t>& water, std::span<const Clear> clear)
{
	return DrawOn(paths, water, clear, nullptr, 0.0f);
}

Grid<uint8_t> paths::Draw(std::span<const Path> paths, const Grid<uint8_t>& water, std::span<const Clear> clear,
                          const Grid<float>& metres, float cliffSlope)
{
	return DrawOn(paths, water, clear, &metres, cliffSlope);
}

std::vector<Lantern> paths::PlaceLanterns(std::span<const Path> paths, std::span<const Place> places,
                                          const Grid<uint8_t>& water, std::span<const Clear> clear, uint64_t seed,
                                          const Settings& settings)
{
	Random random(StageSeed(seed, 82));
	std::vector<Lantern> lanterns;
	const auto kindAt = [&](float x, float z) {
		const float reach = settings.streetReach / k_MetresPerCorner;
		const bool near = std::ranges::any_of(places, [&](const Place& place) {
			const float dx = x - place.x;
			const float dz = z - place.z;
			return (dx * dx) + (dz * dz) <= reach * reach;
		});
		return near ? LanternKind::Street : LanternKind::Country;
	};
	const auto spacingOf = [&](LanternKind kind) {
		return kind == LanternKind::Street ? random.NextRange(settings.streetSpacingLow, settings.streetSpacingHigh)
		                                   : random.NextRange(settings.countrySpacingLow, settings.countrySpacingHigh);
	};
	// A lantern keeps at least the shortest spacing of its kind from the other lanterns, also where two paths share a way
	const auto crowded = [&](float x, float z, LanternKind kind) {
		const float least = kind == LanternKind::Street ? settings.streetSpacingLow : settings.countrySpacingLow;
		return std::ranges::any_of(lanterns, [&](const Lantern& other) {
			const float dx = x - other.x;
			const float dz = z - other.z;
			return (dx * dx) + (dz * dz) < least * least;
		});
	};
	for (const auto& path : paths)
	{
		if (!path.lit || path.points.size() < 2)
		{
			continue;
		}
		// The distance in metres along the path to the next lantern
		float next = spacingOf(kindAt(path.points.front().x, path.points.front().z)) * random.NextUnit();
		float along = 0.0f;
		for (size_t i = 0; i + 1 < path.points.size() && lanterns.size() < settings.mostLanterns; ++i)
		{
			const float ax = path.points[i].x * k_MetresPerCorner;
			const float az = path.points[i].z * k_MetresPerCorner;
			const float dx = (path.points[i + 1].x * k_MetresPerCorner) - ax;
			const float dz = (path.points[i + 1].z * k_MetresPerCorner) - az;
			const float length = std::sqrt((dx * dx) + (dz * dz));
			if (length <= 0.0f)
			{
				continue;
			}
			while (next <= along + length && lanterns.size() < settings.mostLanterns)
			{
				const float t = (next - along) / length;
				const bool left = (random.Next() & 1u) != 0;
				const float side = random.NextRange(settings.nearestSide, settings.farthestSide) * (left ? 1.0f : -1.0f);
				// The lantern stands to one side, square to the way of the path
				const float x = ax + (dx * t) - (dz / length * side);
				const float z = az + (dz * t) + (dx / length * side);
				const float cx = x / k_MetresPerCorner;
				const float cz = z / k_MetresPerCorner;
				const auto kind = kindAt(cx, cz);
				if (!InWater(cx, cz, water) && !InClear(cx, cz, clear) && !crowded(x, z, kind))
				{
					lanterns.push_back({.kind = kind, .x = x, .z = z});
				}
				next += spacingOf(kind);
			}
			along += length;
		}
	}
	return lanterns;
}

float paths::LengthOf(std::span<const Point> points)
{
	float length = 0.0f;
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const float dx = points[i + 1].x - points[i].x;
		const float dz = points[i + 1].z - points[i].z;
		length += std::sqrt((dx * dx) + (dz * dz));
	}
	return length;
}

float paths::WindingOf(std::span<const Point> points)
{
	if (points.size() < 2)
	{
		return 1.0f;
	}
	const float dx = points.back().x - points.front().x;
	const float dz = points.back().z - points.front().z;
	const float straight = std::sqrt((dx * dx) + (dz * dz));
	return straight > 0.0f ? LengthOf(points) / straight : 1.0f;
}
