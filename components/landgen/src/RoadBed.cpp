/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/RoadBed.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>

using namespace openblack::landgen;
using namespace openblack::landgen::roads;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_Infinity = std::numeric_limits<float>::infinity();
/// The largest change of a slope that the rounding of the heights to whole height units can make
constexpr float k_CliffMargin = 0.05f;

int Rounded(float value)
{
	return static_cast<int>(std::floor(value + 0.5f));
}

/// The mean of each value and the values next to it, up to the radius on each side. Near an end, the radius becomes
/// smaller, so that each end keeps its value.
std::vector<float> Mean(std::span<const float> values, int radius)
{
	const auto count = static_cast<int>(values.size());
	std::vector<float> mean(values.size());
	for (int i = 0; i < count; ++i)
	{
		const int reach = std::min({radius, i, count - 1 - i});
		float sum = 0.0f;
		for (int j = i - reach; j <= i + reach; ++j)
		{
			sum += values[static_cast<size_t>(j)];
		}
		mean[static_cast<size_t>(i)] = sum / static_cast<float>((2 * reach) + 1);
	}
	return mean;
}

/// The values with no step larger than the rise. One pass goes forwards and one pass goes backwards, and the result is
/// their mean, so that the direction of the path has no effect.
std::vector<float> LimitRise(std::span<const float> values, float rise)
{
	const size_t count = values.size();
	std::vector<float> forwards(values.begin(), values.end());
	std::vector<float> backwards(values.begin(), values.end());
	for (size_t i = 1; i < count; ++i)
	{
		forwards[i] = std::clamp(forwards[i], forwards[i - 1] - rise, forwards[i - 1] + rise);
	}
	for (size_t i = count - 1; i-- > 0;)
	{
		backwards[i] = std::clamp(backwards[i], backwards[i + 1] - rise, backwards[i + 1] + rise);
	}
	for (size_t i = 0; i < count; ++i)
	{
		forwards[i] = (forwards[i] + backwards[i]) / 2.0f;
	}
	return forwards;
}

/// The height of the land at a point in corners, between the four corners round it
float HeightAt(const Grid<float>& metres, float x, float z)
{
	const auto last = static_cast<float>(metres.side - 1);
	x = std::clamp(x, 0.0f, last);
	z = std::clamp(z, 0.0f, last);
	const int x0 = std::min(static_cast<int>(std::floor(x)), metres.side - 2);
	const int z0 = std::min(static_cast<int>(std::floor(z)), metres.side - 2);
	const float fx = x - static_cast<float>(x0);
	const float fz = z - static_cast<float>(z0);
	const float near = metres.At(x0, z0) + ((metres.At(x0 + 1, z0) - metres.At(x0, z0)) * fx);
	const float far = metres.At(x0, z0 + 1) + ((metres.At(x0 + 1, z0 + 1) - metres.At(x0, z0 + 1)) * fx);
	return near + ((far - near) * fz);
}

/// The middle of a drawn path, which covers the corner of each point and the corners after it on each axis
constexpr float k_DrawnMiddle = 0.5f;

/// The points along a path a step apart, in corners
std::vector<paths::Point> SamplesOf(std::span<const paths::Point> points, float step)
{
	std::vector<paths::Point> samples;
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const auto& a = points[i];
		const auto& b = points[i + 1];
		const float length = std::sqrt(((b.x - a.x) * (b.x - a.x)) + ((b.z - a.z) * (b.z - a.z)));
		if (length <= 0.0f)
		{
			continue;
		}
		const int steps = std::max(1, static_cast<int>(std::ceil(length / step)));
		for (int s = 0; s < steps; ++s)
		{
			const float t = static_cast<float>(s) / static_cast<float>(steps);
			samples.push_back({.x = a.x + ((b.x - a.x) * t) + k_DrawnMiddle, .z = a.z + ((b.z - a.z) * t) + k_DrawnMiddle});
		}
	}
	if (!points.empty())
	{
		samples.push_back({.x = points.back().x + k_DrawnMiddle, .z = points.back().z + k_DrawnMiddle});
	}
	return samples;
}

/// The slope of a corner, from the heights of the corners next to it on each axis, as the measure of the lands has it
float SlopeAt(const Grid<float>& metres, int x, int z)
{
	const int side = metres.side;
	const auto along = [&](int x0, int z0, int x1, int z1) {
		const float run = static_cast<float>((x1 - x0) + (z1 - z0)) * k_MetresPerCorner;
		return (metres.At(x1, z1) - metres.At(x0, z0)) / run;
	};
	const float gx = along(std::max(x - 1, 0), z, std::min(x + 1, side - 1), z);
	const float gz = along(x, std::max(z - 1, 0), x, std::min(z + 1, side - 1));
	return std::sqrt((gx * gx) + (gz * gz));
}

/// The two passes of a chamfer distance over the grid. The visit function gets each corner and each of its neighbours
/// that the pass has already done, with the length of the step in corners.
template <typename Visit>
void ChamferPasses(int side, Visit&& visit)
{
	constexpr float k_Diagonal = 1.41421356f;
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			visit(x, z, x - 1, z - 1, k_Diagonal);
			visit(x, z, x - 1, z, 1.0f);
			visit(x, z, x - 1, z + 1, k_Diagonal);
			visit(x, z, x, z - 1, 1.0f);
		}
	}
	for (int x = side - 1; x >= 0; --x)
	{
		for (int z = side - 1; z >= 0; --z)
		{
			visit(x, z, x + 1, z + 1, k_Diagonal);
			visit(x, z, x + 1, z, 1.0f);
			visit(x, z, x + 1, z - 1, k_Diagonal);
			visit(x, z, x, z + 1, 1.0f);
		}
	}
}
} // namespace

std::vector<float> roads::Profile(std::span<const float> land, std::span<const uint8_t> fixed, const Settings& settings)
{
	const size_t count = land.size();
	if (count == 0)
	{
		return {};
	}
	const int radius = std::max(0, Rounded(settings.smoothing / settings.step));
	std::vector<float> bed(land.begin(), land.end());
	for (int pass = 0; pass < settings.smoothPasses; ++pass)
	{
		bed = Mean(bed, radius);
	}
	bed = LimitRise(bed, settings.steepestGrade * settings.step);
	for (int pass = 0; pass < settings.smoothPasses; ++pass)
	{
		bed = Mean(bed, radius);
	}

	// The bed comes back to the land at each fixed sample and at the ends, with a grade of no more than the fixed grade
	const auto isFixed = [&](size_t i) { return i == 0 || i + 1 == count || (i < fixed.size() && fixed[i] != 0); };
	const float rise = settings.fixedGrade * settings.step;
	std::vector<float> lower(count, -k_Infinity);
	std::vector<float> upper(count, k_Infinity);
	for (size_t i = 0; i < count; ++i)
	{
		if (isFixed(i))
		{
			lower[i] = land[i];
			upper[i] = land[i];
		}
		if (i > 0)
		{
			lower[i] = std::max(lower[i], lower[i - 1] - rise);
			upper[i] = std::min(upper[i], upper[i - 1] + rise);
		}
	}
	for (size_t i = count - 1; i-- > 0;)
	{
		lower[i] = std::max(lower[i], lower[i + 1] - rise);
		upper[i] = std::min(upper[i], upper[i + 1] + rise);
	}
	for (size_t i = 0; i < count; ++i)
	{
		if (isFixed(i))
		{
			bed[i] = land[i];
			continue;
		}
		// Two fixed samples that are too steep for the fixed grade give no room. The bed is then between their limits.
		const float low = std::min(lower[i], upper[i]);
		const float high = std::max(lower[i], upper[i]);
		bed[i] = std::clamp(bed[i], low, high);
		bed[i] = std::clamp(bed[i], land[i] - settings.deepest, land[i] + settings.deepest);
	}
	return bed;
}

Bed roads::BedOf(std::span<const paths::Path> paths, const Ground& ground, const Settings& settings)
{
	const int side = ground.metres.side;
	Bed bed {.height = Grid<float>(side, 0.0f), .covered = Grid<uint8_t>(side, 0)};
	Grid<float> nearest(side, k_Infinity);
	const float reach = settings.halfWidth;
	const int box = static_cast<int>(std::ceil(reach));
	for (const auto& path : paths)
	{
		const auto samples = SamplesOf(path.points, settings.step / k_MetresPerCorner);
		std::vector<float> land(samples.size());
		std::vector<uint8_t> fixed(samples.size());
		std::vector<float> near;
		for (size_t i = 0; i < samples.size(); ++i)
		{
			near.clear();
			const int sx = Rounded(samples[i].x);
			const int sz = Rounded(samples[i].z);
			for (int x = sx - box; x <= sx + box; ++x)
			{
				for (int z = sz - box; z <= sz + box; ++z)
				{
					const float dx = static_cast<float>(x) - samples[i].x;
					const float dz = static_cast<float>(z) - samples[i].z;
					if (ground.metres.Contains(x, z) && (dx * dx) + (dz * dz) <= reach * reach)
					{
						near.push_back(ground.metres.At(x, z));
					}
				}
			}
			if (near.empty())
			{
				land[i] = HeightAt(ground.metres, samples[i].x, samples[i].z);
			}
			else
			{
				std::ranges::sort(near);
				const size_t half = near.size() / 2;
				land[i] = (near.size() % 2) != 0 ? near[half] : (near[half - 1] + near[half]) / 2.0f;
			}
			const int x = Rounded(samples[i].x);
			const int z = Rounded(samples[i].z);
			fixed[i] = !ground.fixed.Contains(x, z) || ground.fixed.At(x, z) != 0 ? 1 : 0;
		}
		const auto profile = Profile(land, fixed, settings);
		for (size_t i = 0; i < samples.size(); ++i)
		{
			const int cx = Rounded(samples[i].x);
			const int cz = Rounded(samples[i].z);
			for (int x = cx - box; x <= cx + box; ++x)
			{
				for (int z = cz - box; z <= cz + box; ++z)
				{
					if (!ground.fixed.Contains(x, z) || ground.fixed.At(x, z) != 0)
					{
						continue;
					}
					const float dx = static_cast<float>(x) - samples[i].x;
					const float dz = static_cast<float>(z) - samples[i].z;
					const float distance = (dx * dx) + (dz * dz);
					if (distance <= reach * reach && distance < nearest.At(x, z) &&
					    std::fabs(profile[i] - ground.metres.At(x, z)) <= settings.deepestCorner)
					{
						nearest.At(x, z) = distance;
						bed.height.At(x, z) = profile[i];
						bed.covered.At(x, z) = 1;
					}
				}
			}
		}
	}
	return bed;
}

Grid<float> roads::Grade(const Bed& bed, const Ground& ground, const Settings& settings)
{
	const int side = ground.metres.side;
	// For each corner: the lowest and the highest height that the banks of the bed let it have, the largest change from
	// the land that the bed lets it have, and the distance to the nearest fixed corner
	Grid<float> lowest(side, -k_Infinity);
	Grid<float> highest(side, k_Infinity);
	Grid<float> change(side, 0.0f);
	Grid<float> toFixed(side, k_Infinity);
	for (size_t i = 0; i < bed.covered.values.size(); ++i)
	{
		if (bed.covered.values[i] != 0)
		{
			lowest.values[i] = bed.height.values[i];
			highest.values[i] = bed.height.values[i];
			change.values[i] = std::fabs(bed.height.values[i] - ground.metres.values[i]);
		}
		if (ground.fixed.values[i] != 0)
		{
			toFixed.values[i] = 0.0f;
		}
	}
	const float bankRise = settings.bankSlope * k_MetresPerCorner;
	const float taperRise = settings.taper * k_MetresPerCorner;
	ChamferPasses(side, [&](int x, int z, int nx, int nz, float length) {
		if (!ground.metres.Contains(nx, nz))
		{
			return;
		}
		const auto here = ground.metres.Index(x, z);
		const auto there = ground.metres.Index(nx, nz);
		lowest.values[here] = std::max(lowest.values[here], lowest.values[there] - (bankRise * length));
		highest.values[here] = std::min(highest.values[here], highest.values[there] + (bankRise * length));
		change.values[here] = std::max(change.values[here], change.values[there] - (taperRise * length));
		toFixed.values[here] = std::min(toFixed.values[here], toFixed.values[there] + length);
	});

	Grid<float> graded = ground.metres;
	for (size_t i = 0; i < graded.values.size(); ++i)
	{
		if (ground.fixed.values[i] != 0 || change.values[i] <= 0.0f)
		{
			continue;
		}
		const float land = ground.metres.values[i];
		float target = land;
		if (bed.covered.values[i] != 0)
		{
			target = bed.height.values[i];
		}
		else if (lowest.values[i] > highest.values[i])
		{
			// Two beds of different heights are near each other. The corner is between their banks.
			target = (lowest.values[i] + highest.values[i]) / 2.0f;
		}
		else
		{
			target = std::clamp(land, lowest.values[i], highest.values[i]);
		}
		const float most = std::min(change.values[i], taperRise * toFixed.values[i]);
		float height = std::clamp(target, land - most, land + most);
		height = std::max(height, std::min(land, ground.lowest.values[i]));
		graded.values[i] = height;
	}

	// The bed and the banks make no new cliff. The change at a new cliff and at the changed corners next to it becomes
	// half as large in each pass. The last pass puts the land back at each new cliff that stays.
	constexpr int k_CliffPasses = 8;
	std::vector<size_t> changed;
	for (size_t i = 0; i < graded.values.size(); ++i)
	{
		if (graded.values[i] != ground.metres.values[i])
		{
			changed.push_back(i);
		}
	}
	std::vector<size_t> cliffs;
	for (int pass = 0; pass <= k_CliffPasses; ++pass)
	{
		cliffs.clear();
		for (const auto i : changed)
		{
			const int cx = graded.XOf(i);
			const int cz = graded.ZOf(i);
			for (const auto& step : std::array<Step, 5> {{{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
			{
				const int x = cx + step.dx;
				const int z = cz + step.dz;
				if (!graded.Contains(x, z))
				{
					continue;
				}
				// The game rounds the heights, so the check keeps a margin on each side of the cliff slope
				const float now = SlopeAt(graded, x, z);
				const float was = SlopeAt(ground.metres, x, z);
				if (now >= settings.cliffSlope - k_CliffMargin && was < settings.cliffSlope + k_CliffMargin && now > was)
				{
					cliffs.push_back(graded.Index(x, z));
				}
			}
		}
		if (cliffs.empty())
		{
			break;
		}
		for (const auto i : cliffs)
		{
			const int cx = graded.XOf(i);
			const int cz = graded.ZOf(i);
			for (const auto& step : std::array<Step, 5> {{{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
			{
				const int x = cx + step.dx;
				const int z = cz + step.dz;
				if (!graded.Contains(x, z))
				{
					continue;
				}
				const float land = ground.metres.At(x, z);
				auto& height = graded.At(x, z);
				height = pass == k_CliffPasses ? land : land + ((height - land) / 2.0f);
			}
		}
	}
	return graded;
}
