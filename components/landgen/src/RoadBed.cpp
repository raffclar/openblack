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
/// The least rise over run by which the change from the land becomes less away from the bed, so that each change stops
constexpr float k_LeastTaper = 0.03f;

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

/// The height of the bed at a point in corners: the mean of the covered corners round the point, each with the weight of
/// its nearness. Where no corner round the point is covered, the height of the nearest corner.
float BedHeightAt(const Bed& bed, float x, float z)
{
	const int x0 = static_cast<int>(std::floor(x));
	const int z0 = static_cast<int>(std::floor(z));
	float sum = 0.0f;
	float weights = 0.0f;
	for (int dx = 0; dx <= 1; ++dx)
	{
		for (int dz = 0; dz <= 1; ++dz)
		{
			const int cx = x0 + dx;
			const int cz = z0 + dz;
			if (!bed.covered.Contains(cx, cz) || bed.covered.At(cx, cz) == 0)
			{
				continue;
			}
			const float weight =
			    (1.0f - std::fabs(x - static_cast<float>(cx))) * (1.0f - std::fabs(z - static_cast<float>(cz)));
			sum += weight * bed.height.At(cx, cz);
			weights += weight;
		}
	}
	if (weights <= 0.0f)
	{
		return bed.height.At(Rounded(x), Rounded(z));
	}
	return sum / weights;
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

	// The bed comes back to the land at each fixed sample and at the ends. Its grade from a ring is no more than the ring
	// grade, and from the other fixed samples and the ends no more than the fixed grade.
	const auto kindOf = [&](size_t i) -> uint8_t {
		if (i < fixed.size() && fixed[i] == k_Ring)
		{
			return k_Ring;
		}
		return i == 0 || i + 1 == count || (i < fixed.size() && fixed[i] != 0) ? k_Fixed : 0;
	};
	const auto isFixed = [&](size_t i) { return kindOf(i) != 0; };
	std::vector<float> lower(count, -k_Infinity);
	std::vector<float> upper(count, k_Infinity);
	for (const auto& [kind, grade] : {std::pair {k_Fixed, settings.fixedGrade}, std::pair {k_Ring, settings.ringGrade}})
	{
		const float rise = grade * settings.step;
		std::vector<float> low(count, -k_Infinity);
		std::vector<float> high(count, k_Infinity);
		for (size_t i = 0; i < count; ++i)
		{
			if (kindOf(i) == kind)
			{
				low[i] = land[i];
				high[i] = land[i];
			}
			if (i > 0)
			{
				low[i] = std::max(low[i], low[i - 1] - rise);
				high[i] = std::min(high[i], high[i - 1] + rise);
			}
		}
		for (size_t i = count - 1; i-- > 0;)
		{
			low[i] = std::max(low[i], low[i + 1] - rise);
			high[i] = std::min(high[i], high[i + 1] + rise);
		}
		for (size_t i = 0; i < count; ++i)
		{
			lower[i] = std::max(lower[i], low[i]);
			upper[i] = std::min(upper[i], high[i]);
		}
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

std::vector<float> roads::CutThinWalls(std::span<const float> land, std::span<const uint8_t> fixed, const Settings& settings)
{
	const auto count = static_cast<int>(land.size());
	const int radius = std::max(0, Rounded(settings.thinWall / (2.0f * settings.step)));
	if (count == 0 || radius == 0)
	{
		return {land.begin(), land.end()};
	}
	// The lowest land near each sample, then the highest of these values near each sample. A hump that is narrower than
	// the window has no sample where the window is all on the hump, so the hump goes. A wider hill stays.
	const auto near = [count, radius](int i) { return std::pair {std::max(0, i - radius), std::min(count - 1, i + radius)}; };
	std::vector<float> lowest(land.size());
	for (int i = 0; i < count; ++i)
	{
		const auto [first, last] = near(i);
		lowest[static_cast<size_t>(i)] =
		    *std::ranges::min_element(land.subspan(static_cast<size_t>(first), static_cast<size_t>(last - first + 1)));
	}
	std::vector<float> cut(land.begin(), land.end());
	for (int i = 0; i < count; ++i)
	{
		const auto at = static_cast<size_t>(i);
		if (at < fixed.size() && fixed[at] != 0)
		{
			continue;
		}
		const auto [first, last] = near(i);
		const float opened = *std::ranges::max_element(
		    std::span(lowest).subspan(static_cast<size_t>(first), static_cast<size_t>(last - first + 1)));
		cut[at] = std::max(opened, land[at] - settings.deepestWallCut);
	}
	return cut;
}

Bed roads::BedOf(std::span<const paths::Path> paths, const Ground& ground, const Settings& settings)
{
	const int side = ground.metres.side;
	Bed bed {.height = Grid<float>(side, 0.0f), .covered = Grid<uint8_t>(side, 0)};
	Grid<float> nearest(side, k_Infinity);
	const float reach = settings.halfWidth;
	const int box = static_cast<int>(std::ceil(reach));
	const float widest = (2.0f * reach) - settings.narrowEdge;
	const int wideBox = static_cast<int>(std::ceil(widest));
	for (const auto& path : paths)
	{
		const auto samples = SamplesOf(path.points, settings.step / k_MetresPerCorner);
		// The corners of the beds of the paths before this path keep their height
		const auto earlier = bed.covered;
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
			fixed[i] = !ground.fixed.Contains(x, z) ? k_Fixed : ground.fixed.At(x, z);
			// Where the path shares the way of a path before it, it keeps the bed of that path and joins it with the
			// grade of a ring. Thus two paths on one way make one bed, with no steps between their profiles.
			if (fixed[i] == 0 && bed.covered.At(x, z) != 0)
			{
				land[i] = BedHeightAt(bed, samples[i].x, samples[i].z);
				fixed[i] = k_Ring;
			}
		}
		const auto cut = CutThinWalls(land, fixed, settings);
		const auto profile = Profile(cut, fixed, settings);
		for (size_t i = 0; i < samples.size(); ++i)
		{
			// The direction across the path, from the samples before and after this sample
			const auto& before = samples[i > 0 ? i - 1 : i];
			const auto& after = samples[i + 1 < samples.size() ? i + 1 : i];
			float ax = after.x - before.x;
			float az = after.z - before.z;
			const float length = std::sqrt((ax * ax) + (az * az));
			if (length <= 0.0f)
			{
				ax = 1.0f;
				az = 0.0f;
			}
			else
			{
				ax /= length;
				az /= length;
			}
			const float nx = -az;
			const float nz = ax;

			// The bed is level from the low edge to the high edge across the path. In a valley where one side is too steep
			// for a bank, the bed moves away from that side and cuts into the other side. The bed also moves away from a
			// side that rises or falls as steeply as a cliff.
			float low = -reach;
			float high = reach;
			const float middle = HeightAt(ground.metres, samples[i].x, samples[i].z);
			const auto riseOf = [&](float direction) {
				constexpr float k_Run = 2.0f;
				const float far =
				    HeightAt(ground.metres, samples[i].x + (nx * direction * k_Run), samples[i].z + (nz * direction * k_Run));
				return (far - middle) / (k_Run * k_MetresPerCorner);
			};
			const float risePlus = riseOf(1.0f);
			const float riseMinus = riseOf(-1.0f);
			const auto tooSteep = [&](float rise, float other) {
				if (std::fabs(rise) >= settings.cliffSlope)
				{
					return std::fabs(rise) > std::fabs(other);
				}
				return rise >= settings.narrowSide && rise > other && other >= 0.0f;
			};
			if (tooSteep(risePlus, riseMinus))
			{
				low = -widest;
				high = settings.narrowEdge;
			}
			else if (tooSteep(riseMinus, risePlus))
			{
				low = -settings.narrowEdge;
				high = widest;
			}
			const float centre = (low + high) / 2.0f;
			const int cx = Rounded(samples[i].x);
			const int cz = Rounded(samples[i].z);
			for (int x = cx - wideBox; x <= cx + wideBox; ++x)
			{
				for (int z = cz - wideBox; z <= cz + wideBox; ++z)
				{
					if (!ground.fixed.Contains(x, z) || ground.fixed.At(x, z) != 0)
					{
						continue;
					}
					const float dx = static_cast<float>(x) - samples[i].x;
					const float dz = static_cast<float>(z) - samples[i].z;
					const float along = (dx * ax) + (dz * az);
					const float across = (dx * nx) + (dz * nz);
					if (across < low || across > high || std::fabs(along) > reach)
					{
						continue;
					}
					const float distance = (along * along) + ((across - centre) * (across - centre));
					// Where the bed cuts through a thin wall, the corners of the wall are part of the bed
					const float deepest = settings.deepestCorner + (land[i] - cut[i]);
					if (earlier.At(x, z) == 0 && distance < nearest.At(x, z) &&
					    std::fabs(profile[i] - ground.metres.At(x, z)) <= deepest)
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
	// the land that the bed lets it have, and the largest change that the nearest fixed corner and the nearest ring let it
	// have
	Grid<float> lowest(side, -k_Infinity);
	Grid<float> highest(side, k_Infinity);
	Grid<float> change(side, 0.0f);
	Grid<float> fromFixed(side, k_Infinity);
	Grid<float> fromRing(side, k_Infinity);
	Grid<float> toBed(side, k_Infinity);
	for (size_t i = 0; i < bed.covered.values.size(); ++i)
	{
		if (bed.covered.values[i] != 0)
		{
			toBed.values[i] = 0.0f;
			lowest.values[i] = bed.height.values[i];
			highest.values[i] = bed.height.values[i];
			change.values[i] = std::fabs(bed.height.values[i] - ground.metres.values[i]);
		}
		if (ground.fixed.values[i] == k_Ring)
		{
			fromRing.values[i] = 0.0f;
		}
		else if (ground.fixed.values[i] != 0)
		{
			fromFixed.values[i] = 0.0f;
		}
	}
	const float cutRise = settings.cutSlope * k_MetresPerCorner;
	const float fillRise = settings.fillSlope * k_MetresPerCorner;
	const float taperRise = settings.taper * k_MetresPerCorner;
	ChamferPasses(side, [&](int x, int z, int nx, int nz, float length) {
		if (!ground.metres.Contains(nx, nz))
		{
			return;
		}
		const auto here = ground.metres.Index(x, z);
		const auto there = ground.metres.Index(nx, nz);
		lowest.values[here] = std::max(lowest.values[here], lowest.values[there] - (fillRise * length));
		highest.values[here] = std::min(highest.values[here], highest.values[there] + (cutRise * length));
		toBed.values[here] = std::min(toBed.values[here], toBed.values[there] + length);
	});

	// A steep side next to the bed that is not too high, such as a low cliff or the side of a small hill, gets a bank of
	// the fixed slope back to where the bank meets the land. A corner gets the full bank when the bank meets the land
	// within the reach of the deepest bank and the cut or the fill at the corner is no more than the deepest bank.
	const float reachOfBank = settings.deepestBank / std::min(cutRise, fillRise);
	Grid<float> toMetCut(side, k_Infinity);
	Grid<float> toMetFill(side, k_Infinity);
	for (size_t i = 0; i < toBed.values.size(); ++i)
	{
		if (bed.covered.values[i] != 0 || toBed.values[i] > 2.0f * reachOfBank)
		{
			continue;
		}
		const float land = ground.metres.values[i];
		const float bedCut = highest.values[i] - (cutRise * toBed.values[i]);
		const float bedFill = lowest.values[i] + (fillRise * toBed.values[i]);
		if (land <= highest.values[i] && land >= bedCut)
		{
			toMetCut.values[i] = 0.0f;
		}
		if (land >= lowest.values[i] && land <= bedFill)
		{
			toMetFill.values[i] = 0.0f;
		}
	}
	// A bank also starts where the land between the bed and the bank meets a steep side, such as the foot of a cliff
	Grid<float> bankHigh(side, k_Infinity);
	Grid<float> bankLow(side, -k_Infinity);
	for (size_t i = 0; i < toBed.values.size(); ++i)
	{
		if (toMetCut.values[i] == 0.0f)
		{
			bankHigh.values[i] = ground.metres.values[i];
		}
		if (toMetFill.values[i] == 0.0f)
		{
			bankLow.values[i] = ground.metres.values[i];
		}
	}
	ChamferPasses(side, [&](int x, int z, int nx, int nz, float length) {
		if (!ground.metres.Contains(nx, nz))
		{
			return;
		}
		const auto here = ground.metres.Index(x, z);
		const auto there = ground.metres.Index(nx, nz);
		toMetCut.values[here] = std::min(toMetCut.values[here], toMetCut.values[there] + length);
		toMetFill.values[here] = std::min(toMetFill.values[here], toMetFill.values[there] + length);
		bankHigh.values[here] = std::min(bankHigh.values[here], bankHigh.values[there] + (cutRise * length));
		bankLow.values[here] = std::max(bankLow.values[here], bankLow.values[there] - (fillRise * length));
	});
	for (size_t i = 0; i < toBed.values.size(); ++i)
	{
		if (bed.covered.values[i] == 0)
		{
			highest.values[i] = std::min(highest.values[i], bankHigh.values[i]);
			lowest.values[i] = std::max(lowest.values[i], bankLow.values[i]);
		}
	}
	for (size_t i = 0; i < change.values.size(); ++i)
	{
		if (bed.covered.values[i] != 0 || ground.fixed.values[i] != 0 || toBed.values[i] > reachOfBank)
		{
			continue;
		}
		const float land = ground.metres.values[i];
		const float cut = land - highest.values[i];
		const float fill = lowest.values[i] - land;
		if (cut > 0.0f && cut <= settings.deepestBank && toMetCut.values[i] <= reachOfBank)
		{
			change.values[i] = std::max(change.values[i], cut);
		}
		else if (fill > 0.0f && fill <= settings.deepestBank && toMetFill.values[i] <= reachOfBank)
		{
			change.values[i] = std::max(change.values[i], fill);
		}
	}

	// The change from the land becomes less away from the bed. Where the land is less steep than the bank, it becomes
	// less by the slope of the bank minus the slope of the land, so that the bank keeps its fixed slope until it meets
	// the land. Where the land is steeper, it becomes less by the taper, or by less where the taper makes a cliff.
	const float steepestFace = settings.cliffSlope - (2.0f * k_CliffMargin);
	// The largest taper at a corner that keeps the new ground less steep than a cliff, where the land is not a cliff
	const auto taperOf = [&](float slope, float taper) {
		return slope < settings.cliffSlope ? std::clamp(steepestFace - slope, k_LeastTaper, taper) : taper;
	};
	Grid<float> rate(side, taperRise);
	Grid<float> fixedRate(side, 0.0f);
	Grid<float> ringRate(side, 0.0f);
	for (size_t i = 0; i < rate.values.size(); ++i)
	{
		const float land = ground.metres.values[i];
		float bank = 0.0f;
		if (land > highest.values[i])
		{
			bank = settings.cutSlope;
		}
		else if (land < lowest.values[i])
		{
			bank = settings.fillSlope;
		}
		const float slope = SlopeAt(ground.metres, rate.XOf(i), rate.ZOf(i));
		rate.values[i] = std::max(taperOf(slope, settings.taper), bank - slope) * k_MetresPerCorner;
		fixedRate.values[i] = taperOf(slope, settings.fixedTaper) * k_MetresPerCorner;
		ringRate.values[i] = taperOf(slope, settings.ringTaper) * k_MetresPerCorner;
	}
	constexpr int k_ChangePasses = 2;
	for (int pass = 0; pass < k_ChangePasses; ++pass)
	{
		ChamferPasses(side, [&](int x, int z, int nx, int nz, float length) {
			if (!ground.metres.Contains(nx, nz))
			{
				return;
			}
			const auto here = ground.metres.Index(x, z);
			const auto there = ground.metres.Index(nx, nz);
			change.values[here] = std::max(change.values[here], change.values[there] - (rate.values[here] * length));
			fromFixed.values[here] =
			    std::min(fromFixed.values[here], fromFixed.values[there] + (fixedRate.values[here] * length));
			fromRing.values[here] = std::min(fromRing.values[here], fromRing.values[there] + (ringRate.values[here] * length));
		});
	}

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
		// The bed itself is level up to the shores and the rivers. Only the banks become less near them.
		const float most = bed.covered.values[i] != 0 ? std::min(change.values[i], fromRing.values[i])
		                                              : std::min({change.values[i], fromFixed.values[i], fromRing.values[i]});
		float height = std::clamp(target, land - most, land + most);
		height = std::max(height, std::min(land, ground.lowest.values[i]));
		graded.values[i] = height;
	}

	// The bed and the banks make no new cliff. The change at a new cliff and next to it becomes half as large in each
	// pass. The first passes change only the banks, so that the bed stays level where it can. After the last of these
	// passes, each pass puts the land back at each new cliff that stays and next to it. The land that comes back can make a
	// new cliff next to it, so these passes continue until no new cliff stays.
	constexpr int k_CliffPasses = 12;
	constexpr int k_BedPasses = 4;
	std::vector<size_t> changed;
	for (size_t i = 0; i < graded.values.size(); ++i)
	{
		if (graded.values[i] != ground.metres.values[i])
		{
			changed.push_back(i);
		}
	}
	std::vector<size_t> cliffs;
	for (int pass = 0;; ++pass)
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
				// The game rounds the heights, so the check keeps a margin on each side of the cliff slope. A corner that
				// was in the margin can become a cliff when the game rounds its new heights. The passes that put the land
				// back thus also give such a corner its old slope.
				const float now = SlopeAt(graded, x, z);
				const float was = SlopeAt(ground.metres, x, z);
				const bool inMargin = pass >= k_CliffPasses && was >= settings.cliffSlope - k_CliffMargin;
				if (now >= settings.cliffSlope - k_CliffMargin && was < settings.cliffSlope + k_CliffMargin &&
				    (now > was || (inMargin && now != was)))
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
				if (pass < k_BedPasses && bed.covered.At(x, z) != 0)
				{
					continue;
				}
				const float land = ground.metres.At(x, z);
				auto& height = graded.At(x, z);
				height = pass >= k_CliffPasses ? land : land + ((height - land) / 2.0f);
			}
		}
	}
	return graded;
}
