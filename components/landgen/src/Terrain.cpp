/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Terrain.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "landgen/Climate.h"
#include "landgen/Noise.h"
#include "landgen/Random.h"

using namespace openblack::landgen;

namespace
{
struct Direction
{
	float x;
	float z;
};
/// Eight directions over half a turn, for the lines of the mountain ranges
constexpr std::array<Direction, 8> k_RangeDirections {{{1.0f, 0.0f},
                                                       {0.9238795f, 0.3826834f},
                                                       {0.7071068f, 0.7071068f},
                                                       {0.3826834f, 0.9238795f},
                                                       {0.0f, 1.0f},
                                                       {-0.3826834f, 0.9238795f},
                                                       {-0.7071068f, 0.7071068f},
                                                       {-0.9238795f, 0.3826834f}}};

struct Range
{
	float centreU;
	float centreV;
	Direction direction;
	/// Half the length of the line and the width of the range, in parts of the map
	float halfLength;
	float width;
	/// The part of the full height that this range reaches
	float height;
};

/// How much of a range's height a point has: full on the line, less to the sides and towards the ends
float RangeProfile(const Range& range, float u, float v)
{
	const float du = u - range.centreU;
	const float dv = v - range.centreV;
	const float along = (du * range.direction.x) + (dv * range.direction.z);
	const float across = (du * -range.direction.z) + (dv * range.direction.x);
	const float t = std::clamp(along / range.halfLength, -1.0f, 1.0f);
	// The distance from the line, which stops at the ends
	const float beyond = std::fabs(along) - range.halfLength;
	const float endDistance = beyond > 0.0f ? beyond : 0.0f;
	const float distance = std::sqrt((across * across) + (endDistance * endDistance));
	const float side = noise::SmoothStep(range.width, 0.0f, distance);
	const float taper = 1.0f - (t * t * 0.6f);
	return side * taper * range.height;
}

struct Sample
{
	float height;
	float gradientX;
	float gradientZ;
};

Sample SampleAt(const Grid<float>& heights, float x, float z)
{
	const auto ix = static_cast<int>(x);
	const auto iz = static_cast<int>(z);
	const float fx = x - static_cast<float>(ix);
	const float fz = z - static_cast<float>(iz);
	const float h00 = heights.At(ix, iz);
	const float h10 = heights.At(ix + 1, iz);
	const float h01 = heights.At(ix, iz + 1);
	const float h11 = heights.At(ix + 1, iz + 1);
	return {
	    .height = (h00 * (1.0f - fx) * (1.0f - fz)) + (h10 * fx * (1.0f - fz)) + (h01 * (1.0f - fx) * fz) + (h11 * fx * fz),
	    .gradientX = ((h10 - h00) * (1.0f - fz)) + ((h11 - h01) * fz),
	    .gradientZ = ((h01 - h00) * (1.0f - fx)) + ((h11 - h10) * fx),
	};
}

/// Puts sediment down on the four corners of the cell of a point, each by how near the point is
void Deposit(Grid<float>& heights, int ix, int iz, float fx, float fz, float amount)
{
	heights.At(ix, iz) += amount * (1.0f - fx) * (1.0f - fz);
	heights.At(ix + 1, iz) += amount * fx * (1.0f - fz);
	heights.At(ix, iz + 1) += amount * (1.0f - fx) * fz;
	heights.At(ix + 1, iz + 1) += amount * fx * fz;
}

struct BrushPoint
{
	int dx;
	int dz;
	float weight;
};

/// The rest to the power of the profile, between the powers 0.5, 1, 1.5 and 2 that need only a square root. Between
/// two of those powers the result changes in a straight line.
float ProfileOf(float rest, float profile)
{
	const float root = std::sqrt(rest);
	const std::array<float, 4> powers {root, rest, rest * root, rest * rest};
	const float position = std::clamp((profile - 0.5f) * 2.0f, 0.0f, 3.0f);
	const auto low = std::min(static_cast<size_t>(position), size_t {2});
	const float part = position - static_cast<float>(low);
	return powers[low] + ((powers[low + 1] - powers[low]) * part);
}

std::vector<BrushPoint> MakeBrush(int radius)
{
	std::vector<BrushPoint> brush;
	for (int dx = -radius; dx <= radius; ++dx)
	{
		for (int dz = -radius; dz <= radius; ++dz)
		{
			const float distance = std::sqrt(static_cast<float>((dx * dx) + (dz * dz)));
			if (distance < static_cast<float>(radius))
			{
				brush.push_back({.dx = dx, .dz = dz, .weight = static_cast<float>(radius) - distance});
			}
		}
	}
	return brush;
}
} // namespace

Grid<float> terrain::BaseHeights(int side, uint64_t seed, const BaseSettings& settings)
{
	Random random(StageSeed(seed, 1));
	std::vector<Range> ranges;
	for (int i = 0; i < settings.ranges; ++i)
	{
		ranges.push_back({
		    .centreU = random.NextRange(0.3f, 0.7f),
		    .centreV = random.NextRange(0.3f, 0.7f),
		    .direction = k_RangeDirections[random.NextBelow(static_cast<uint32_t>(k_RangeDirections.size()))],
		    .halfLength = random.NextRange(0.1f, 0.22f),
		    .width = random.NextRange(0.07f, 0.11f),
		    .height = random.NextRange(0.75f, 1.0f),
		});
	}
	const auto warpU = StageSeed(seed, 2);
	const auto warpV = StageSeed(seed, 3);
	const auto outline = StageSeed(seed, 4);
	const auto hills = StageSeed(seed, 5);
	const auto crests = StageSeed(seed, 6);

	Grid<float> heights(side, 0.0f);
	const float last = static_cast<float>(side - 1);
	const float margin = static_cast<float>(std::max(settings.seaMargin, 1));
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const float u = static_cast<float>(x) / last;
			const float v = static_cast<float>(z) / last;
			// The warp bends the outline and the ranges, so that no line is straight
			const float wu = u + (0.1f * noise::Fractal(warpU, u * 3.0f, v * 3.0f, 4));
			const float wv = v + (0.1f * noise::Fractal(warpV, (u * 3.0f) + 5.2f, (v * 3.0f) + 1.3f, 4));
			const float du = wu - 0.5f;
			const float dv = wv - 0.5f;
			const float radius = std::sqrt((du * du) + (dv * dv)) * 2.0f;
			float land =
			    0.62f + settings.landAmount - (radius * 0.75f) + (0.35f * noise::Fractal(outline, wu * 2.2f, wv * 2.2f, 5));

			// Near the edges of the map the land goes under the sea
			const int toEdge = std::min({x, z, side - 1 - x, side - 1 - z});
			const float edge = noise::SmoothStep(0.0f, 1.0f, static_cast<float>(toEdge) / margin);
			land = -0.3f + ((land + 0.3f) * edge);

			float height = 0.0f;
			if (land < 0.0f)
			{
				// The sea bed goes down gently from the coast
				height = land * 80.0f;
			}
			else
			{
				height = (settings.plainHeight * noise::SmoothStep(0.0f, settings.plainRise, land)) + 0.5f;
				height += settings.hillHeight * noise::Fractal(hills, wu * 9.0f, wv * 9.0f, 4) *
				          noise::SmoothStep(0.03f, 0.25f, land);
			}
			float mountain = 0.0f;
			for (const auto& range : ranges)
			{
				mountain = std::max(mountain, RangeProfile(range, wu, wv));
			}
			const float crest = 0.35f + (0.65f * noise::Ridged(crests, wu * 7.0f, wv * 7.0f, 5));
			height += settings.mountainHeight * mountain * crest * noise::SmoothStep(0.0f, 0.15f, land);
			heights.At(x, z) = height;
		}
	}

	// The hollows: bowls in the low land, away from each other
	std::vector<std::array<float, 2>> hollows;
	const float radius = settings.hollowRadius;
	for (int tries = 0; tries < settings.hollows * 40 && static_cast<int>(hollows.size()) < settings.hollows; ++tries)
	{
		const float cx = random.NextRange(0.15f, 0.85f) * last;
		const float cz = random.NextRange(0.15f, 0.85f) * last;
		const float ground = heights.At(static_cast<int>(cx), static_cast<int>(cz));
		const bool apart = std::ranges::none_of(hollows, [cx, cz, radius](const auto& other) {
			const float dx = other[0] - cx;
			const float dz = other[1] - cz;
			return (dx * dx) + (dz * dz) < 9.0f * radius * radius;
		});
		if (ground < k_SeaLevel + 1.0f || ground > settings.hollowGround || !apart)
		{
			continue;
		}
		hollows.push_back({cx, cz});
		const auto reach = static_cast<int>(radius * 1.5f) + 1;
		for (int x = static_cast<int>(cx) - reach; x <= static_cast<int>(cx) + reach; ++x)
		{
			for (int z = static_cast<int>(cz) - reach; z <= static_cast<int>(cz) + reach; ++z)
			{
				if (!heights.Contains(x, z))
				{
					continue;
				}
				const float dx = static_cast<float>(x) - cx;
				const float dz = static_cast<float>(z) - cz;
				// The noise bends the shore of the hollow, so that it is not round
				const float bend = 0.75f + (0.5f * noise::Fractal(outline, static_cast<float>(x) / last * 24.0f,
				                                                  static_cast<float>(z) / last * 24.0f, 2));
				const float edge = noise::SmoothStep(0.45f, 1.0f, std::sqrt((dx * dx) + (dz * dz)) / (radius * bend));
				auto& height = heights.At(x, z);
				height = std::min(height, settings.hollowFloor + ((height - settings.hollowFloor) * edge));
			}
		}
	}
	return heights;
}

void terrain::ErodeHydraulic(Grid<float>& heights, uint64_t seed, const HydraulicSettings& settings)
{
	if (heights.side < 3)
	{
		return;
	}
	// The drops work on heights in the units of their slope
	Grid<float> scaled = heights;
	for (auto& height : scaled.values)
	{
		height *= settings.heightScale;
	}
	const float sea = k_SeaLevel * settings.heightScale;
	const auto brush = MakeBrush(settings.radius);
	const float limit = static_cast<float>(heights.side - 1);

	Random random(StageSeed(seed, 10));
	for (int drop = 0; drop < settings.drops; ++drop)
	{
		float x = random.NextUnit() * limit;
		float z = random.NextUnit() * limit;
		if (SampleAt(scaled, x, z).height < sea)
		{
			continue;
		}
		float directionX = 0.0f;
		float directionZ = 0.0f;
		float speed = 1.0f;
		float water = 1.0f;
		float sediment = 0.0f;
		for (int step = 0; step < settings.steps; ++step)
		{
			const auto ix = static_cast<int>(x);
			const auto iz = static_cast<int>(z);
			const float fx = x - static_cast<float>(ix);
			const float fz = z - static_cast<float>(iz);
			const auto here = SampleAt(scaled, x, z);

			directionX = (directionX * settings.inertia) - (here.gradientX * (1.0f - settings.inertia));
			directionZ = (directionZ * settings.inertia) - (here.gradientZ * (1.0f - settings.inertia));
			const float length = std::sqrt((directionX * directionX) + (directionZ * directionZ));
			if (length < 1e-6f)
			{
				break;
			}
			directionX /= length;
			directionZ /= length;
			x += directionX;
			z += directionZ;
			if (x < 0.0f || z < 0.0f || x >= limit || z >= limit)
			{
				break;
			}

			const float next = SampleAt(scaled, x, z).height;
			const float fall = next - here.height;
			if (next < sea)
			{
				// The drop goes into the sea and puts all its sediment down at the coast
				Deposit(scaled, ix, iz, fx, fz, sediment);
				break;
			}
			const float capacity = std::max(-fall * speed * water * settings.capacity, settings.minCapacity);
			if (sediment > capacity || fall > 0.0f)
			{
				const float amount = fall > 0.0f ? std::min(fall, sediment) : (sediment - capacity) * settings.deposition;
				sediment -= amount;
				Deposit(scaled, ix, iz, fx, fz, amount);
			}
			else
			{
				const float amount = std::min((capacity - sediment) * settings.erosion, -fall);
				float total = 0.0f;
				for (const auto& point : brush)
				{
					if (scaled.Contains(ix + point.dx, iz + point.dz))
					{
						total += point.weight;
					}
				}
				for (const auto& point : brush)
				{
					if (scaled.Contains(ix + point.dx, iz + point.dz))
					{
						scaled.At(ix + point.dx, iz + point.dz) -= amount * point.weight / total;
					}
				}
				sediment += amount;
			}
			speed = std::sqrt(std::max(0.0f, (speed * speed) - (fall * settings.gravity)));
			water *= 1.0f - settings.evaporation;
		}
	}
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		heights.values[i] = scaled.values[i] / settings.heightScale;
	}
}

void terrain::ErodeThermal(Grid<float>& heights, const ThermalSettings& settings, std::span<const uint8_t> fixed)
{
	const auto isFixed = [fixed](size_t index) { return !fixed.empty() && fixed[index] != 0; };
	Grid<float> change(heights.side, 0.0f);
	for (int pass = 0; pass < settings.passes; ++pass)
	{
		std::ranges::fill(change.values, 0.0f);
		for (int x = 0; x < heights.side; ++x)
		{
			for (int z = 0; z < heights.side; ++z)
			{
				if (isFixed(heights.Index(x, z)))
				{
					continue;
				}
				const float height = heights.At(x, z);
				const float talus = settings.plainTalus +
				                    ((settings.mountainTalus - settings.plainTalus) * noise::SmoothStep(60.0f, 180.0f, height));
				for (size_t i = 0; i < 4; ++i)
				{
					const int nx = x + k_Neighbours[i].dx;
					const int nz = z + k_Neighbours[i].dz;
					if (!heights.Contains(nx, nz) || isFixed(heights.Index(nx, nz)))
					{
						continue;
					}
					const float difference = height - heights.At(nx, nz);
					if (difference > talus)
					{
						const float move = settings.rate * (difference - talus) * 0.25f;
						change.At(x, z) -= move;
						change.At(nx, nz) += move;
					}
				}
			}
		}
		for (size_t i = 0; i < heights.values.size(); ++i)
		{
			heights.values[i] += change.values[i];
		}
	}
}

std::vector<float> terrain::TerraceLevels(uint64_t seed, const TerraceSettings& settings)
{
	Random random(StageSeed(seed, 11));
	std::vector<float> levels;
	float level = random.NextRange(settings.firstLevelLow, settings.firstLevelHigh);
	while (level <= settings.highestLevel)
	{
		levels.push_back(level);
		level += random.NextRange(settings.spacingLow, settings.spacingHigh);
	}
	return levels;
}

Grid<uint8_t> terrain::TerraceSteps(const Grid<float>& heights, std::span<const float> levels, uint64_t seed,
                                    const TerraceSettings& settings, std::span<const float> highestEdge)
{
	const int side = heights.side;
	const float last = static_cast<float>(std::max(side - 1, 1));
	const auto edgeSeed = StageSeed(seed, 12);
	const auto warpSeed = StageSeed(seed, 16);
	const auto ridgeSeed = StageSeed(seed, 17);
	const auto roughSeed = StageSeed(seed, 18);
	Grid<uint8_t> steps(side, 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const float height = heights.At(x, z);
			if (height <= k_SeaLevel)
			{
				continue;
			}
			// The warp bends the edges, and the ridged noise makes long ridges and gorges rather than round hills
			const float u0 = static_cast<float>(x) / last * settings.edgeFrequency;
			const float v0 = static_cast<float>(z) / last * settings.edgeFrequency;
			const float u = u0 + (settings.edgeWarp * noise::Fractal(warpSeed, u0 * 0.5f, v0 * 0.5f, 3));
			const float v = v0 + (settings.edgeWarp * noise::Fractal(warpSeed, (u0 * 0.5f) + 7.3f, (v0 * 0.5f) + 2.9f, 3));
			const float ridged = (2.0f * noise::Ridged(ridgeSeed, u, v, 4)) - 1.0f;
			const float shape =
			    ((1.0f - settings.edgeRidge) * noise::Fractal(edgeSeed, u, v, 4)) + (settings.edgeRidge * ridged);
			// Some parts of the land are calm, with wide terraces, and other parts are rough, with many cliffs
			const float rough =
			    noise::SmoothStep(-0.35f, 0.35f,
			                      noise::Fractal(roughSeed, u0 * settings.roughFrequency / settings.edgeFrequency,
			                                     v0 * settings.roughFrequency / settings.edgeFrequency, 2));
			const float amount = settings.calmNoise + ((settings.edgeNoise - settings.calmNoise) * rough);
			float moved = height + (amount * shape);
			if (!highestEdge.empty())
			{
				moved = std::min(moved, highestEdge[steps.Index(x, z)]);
			}
			const auto above = std::ranges::upper_bound(levels, moved);
			steps.At(x, z) = static_cast<uint8_t>(std::distance(levels.begin(), above));
		}
	}

	// A step of one corner makes a spike or a pit. The corner takes the nearest step of its neighbours.
	for (int pass = 0; pass < 2; ++pass)
	{
		const Grid<uint8_t> before = steps;
		for (int x = 1; x < side - 1; ++x)
		{
			for (int z = 1; z < side - 1; ++z)
			{
				uint8_t lowest = 255;
				uint8_t highest = 0;
				for (size_t i = 0; i < 4; ++i)
				{
					const auto step = before.At(x + k_Neighbours[i].dx, z + k_Neighbours[i].dz);
					lowest = std::min(lowest, step);
					highest = std::max(highest, step);
				}
				steps.At(x, z) = std::clamp(before.At(x, z), lowest, highest);
			}
		}
	}

	// A small area of a high step makes a spike, because its cliffs meet at the top. Each small area goes down one
	// step, from the highest step down.
	std::vector<uint8_t> seen(steps.values.size(), 0);
	std::vector<uint32_t> area;
	std::vector<uint32_t> stack;
	for (auto level = static_cast<uint8_t>(levels.size()); level >= 1; --level)
	{
		std::ranges::fill(seen, uint8_t {0});
		for (size_t start = 0; start < steps.values.size(); ++start)
		{
			if (steps.values[start] < level || seen[start] != 0)
			{
				continue;
			}
			area.clear();
			stack.assign(1, static_cast<uint32_t>(start));
			seen[start] = 1;
			while (!stack.empty())
			{
				const auto index = stack.back();
				stack.pop_back();
				area.push_back(index);
				const int x = steps.XOf(index);
				const int z = steps.ZOf(index);
				for (size_t i = 0; i < 4; ++i)
				{
					const int nx = x + k_Neighbours[i].dx;
					const int nz = z + k_Neighbours[i].dz;
					if (!steps.Contains(nx, nz))
					{
						continue;
					}
					const auto next = steps.Index(nx, nz);
					if (steps.values[next] >= level && seen[next] == 0)
					{
						seen[next] = 1;
						stack.push_back(static_cast<uint32_t>(next));
					}
				}
			}
			if (static_cast<int>(area.size()) < settings.smallestArea)
			{
				for (const auto index : area)
				{
					steps.values[index] = static_cast<uint8_t>(level - 1);
				}
			}
		}
	}

	// A low step that higher steps close in all round would hold water as a lake above the sea, which the lands of the
	// story do not have. A flood from the sea and the edges of the map lifts each closed step to the step of its rim.
	std::vector<std::vector<uint32_t>> queues(levels.size() + 1);
	std::vector<uint8_t> reached(steps.values.size(), 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = steps.Index(x, z);
			if (heights.values[index] <= k_SeaLevel || x == 0 || z == 0 || x == side - 1 || z == side - 1)
			{
				reached[index] = 1;
				queues[steps.values[index]].push_back(static_cast<uint32_t>(index));
			}
		}
	}
	for (size_t level = 0; level < queues.size(); ++level)
	{
		// The queue of a level can grow while the flood goes through it
		for (size_t next = 0; next < queues[level].size(); ++next)
		{
			const auto index = queues[level][next];
			const int x = steps.XOf(index);
			const int z = steps.ZOf(index);
			for (const auto& neighbour : k_Neighbours)
			{
				if (!steps.Contains(x + neighbour.dx, z + neighbour.dz))
				{
					continue;
				}
				const auto other = steps.Index(x + neighbour.dx, z + neighbour.dz);
				if (reached[other] != 0)
				{
					continue;
				}
				reached[other] = 1;
				auto& step = steps.values[other];
				step = std::max(step, static_cast<uint8_t>(level));
				queues[step].push_back(static_cast<uint32_t>(other));
			}
		}
	}
	return steps;
}

Grid<uint8_t> terrain::Terrace(Grid<float>& heights, uint64_t seed, const TerraceSettings& settings,
                               std::span<const float> highestEdge)
{
	const int side = heights.side;
	Grid<uint8_t> cliffs(side, 0);
	const auto levels = TerraceLevels(seed, settings);
	if (levels.empty())
	{
		return cliffs;
	}
	const auto steps = TerraceSteps(heights, levels, seed, settings, highestEdge);
	const float last = static_cast<float>(std::max(side - 1, 1));
	const auto widthSeed = StageSeed(seed, 13);

	// The flat terraces: each corner goes down towards the edge of its terrace
	Grid<float> terraced = heights;
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		const auto step = steps.values[i];
		const float height = heights.values[i];
		if (height <= k_SeaLevel)
		{
			continue;
		}
		if (step == 0)
		{
			// The low land keeps its shape, but the edge noise can put high ground on it. That ground rises slower and
			// slower above the knee and stays below the first edge, so that it does not become one flat top.
			const float first = levels.front();
			const float knee = first * std::clamp(settings.lowKnee, 0.0f, 0.99f);
			if (height > knee)
			{
				const float over = (height - knee) / (first - knee);
				terraced.values[i] = knee + ((first - knee) * over / (1.0f + over));
			}
			continue;
		}
		const float edge = levels[step - 1U];
		const float top = step < levels.size() ? levels[step] : k_HighestAltitude;
		const float keep = settings.keepLow + ((settings.keepHigh - settings.keepLow) *
		                                       noise::SmoothStep(settings.keepLowAltitude, settings.keepHighAltitude, height));
		// The terrace keeps a part of the rise of its ground, less and less as the ground goes up. It never gets to the
		// next edge, so that the cliff is always a rise, and high ground does not become one flat top.
		const float span = top - edge;
		// Ground below the edge is on a closed hollow that the steps lifted. It rises from the edge by its depth, so
		// that it keeps the shape of its ground and does not become flat.
		const float above = std::fabs(height - edge) / span;
		terraced.values[i] = edge + (span * keep * above / (1.0f + (keep * above)));
	}

	// The cliffs: the ground on the low side of each edge rises to the edge
	Grid<uint8_t> higher(side, 0);
	for (size_t level = 1; level <= levels.size(); ++level)
	{
		for (size_t i = 0; i < steps.values.size(); ++i)
		{
			higher.values[i] = steps.values[i] >= level ? 1 : 0;
		}
		const auto distance = climate::DistanceToWater(higher);
		const float edge = levels[level - 1];
		for (int x = 0; x < side; ++x)
		{
			for (int z = 0; z < side; ++z)
			{
				const auto index = heights.Index(x, z);
				if (higher.values[index] != 0 || heights.values[index] <= k_SeaLevel)
				{
					continue;
				}
				const float u = static_cast<float>(x) / last * settings.cliffFrequency;
				const float v = static_cast<float>(z) / last * settings.cliffFrequency;
				const float steepness =
				    settings.cliffRiseLow + ((settings.cliffRiseHigh - settings.cliffRiseLow) *
				                             noise::SmoothStep(-0.5f, 0.5f, noise::Fractal(widthSeed, u, v, 2)));
				// The face of the cliff goes down from the edge at the same angle, and stops at the terrace below
				const float face = edge - (steepness * (distance.values[index] - 0.5f));
				auto& height = terraced.values[index];
				if (face <= height)
				{
					continue;
				}
				height = face;
				cliffs.values[index] = 1;
			}
		}
	}

	// A terrace floor that keeps a hollow of its ground would become a wet basin. A flood from the sea and the edges of
	// the map fills each hollow of the terraces and cliffs to the height where it spills over.
	using Entry = std::pair<float, uint32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
	std::vector<uint8_t> reached(terraced.values.size(), 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = terraced.Index(x, z);
			if (terraced.values[index] <= k_SeaLevel || x == 0 || z == 0 || x == side - 1 || z == side - 1)
			{
				reached[index] = 1;
				open.emplace(terraced.values[index], static_cast<uint32_t>(index));
			}
		}
	}
	while (!open.empty())
	{
		const auto [level, index] = open.top();
		open.pop();
		const int x = terraced.XOf(index);
		const int z = terraced.ZOf(index);
		for (const auto& neighbour : k_Neighbours)
		{
			if (!terraced.Contains(x + neighbour.dx, z + neighbour.dz))
			{
				continue;
			}
			const auto other = terraced.Index(x + neighbour.dx, z + neighbour.dz);
			if (reached[other] != 0)
			{
				continue;
			}
			reached[other] = 1;
			auto& height = terraced.values[other];
			if (steps.values[other] != 0 || cliffs.values[other] != 0)
			{
				height = std::max(height, level);
			}
			open.emplace(std::max(height, level), static_cast<uint32_t>(other));
		}
	}
	heights = std::move(terraced);
	return cliffs;
}

std::vector<terrain::Peak> terrain::PlaceMassifs(const Grid<float>& heights, uint64_t seed, const MassifSettings& settings)
{
	const int side = heights.side;
	std::vector<Peak> peaks;
	if (side < 2 || settings.countHigh <= 0)
	{
		return peaks;
	}
	const float last = static_cast<float>(side - 1);
	Grid<uint8_t> sea(side, 0);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		sea.values[i] = heights.values[i] <= k_SeaLevel ? 1 : 0;
	}
	const auto seaDistance = climate::DistanceToWater(sea);

	Random random(StageSeed(seed, 19));
	const int lowest = std::max(settings.countLow, 0);
	const int count =
	    lowest + static_cast<int>(random.NextBelow(static_cast<uint32_t>(std::max(settings.countHigh - lowest, 0) + 1)));
	std::vector<Peak> mains;
	for (int massif = 0; massif < count; ++massif)
	{
		const float radius = random.NextRange(settings.radiusLow, settings.radiusHigh) * last;
		std::optional<Peak> best;
		float bestInland = 0.0f;
		for (int attempt = 0; attempt < settings.tries; ++attempt)
		{
			const float x = random.NextRange(0.1f, 0.9f) * last;
			const float z = random.NextRange(0.1f, 0.9f) * last;
			const float inland = seaDistance.At(static_cast<int>(x), static_cast<int>(z));
			if (inland < radius * settings.seaClearance)
			{
				continue;
			}
			const bool apart = std::ranges::none_of(mains, [x, z, radius](const Peak& other) {
				const float dx = other.x - x;
				const float dz = other.z - z;
				const float reach = (other.radius + radius) * 0.9f;
				return (dx * dx) + (dz * dz) < reach * reach;
			});
			if (apart && (!best.has_value() || inland > bestInland))
			{
				best = Peak {.x = x, .z = z, .radius = radius, .summit = 0.0f};
				bestInland = inland;
			}
		}
		// The random numbers of the massif come after the search, so that each massif takes the same numbers
		const float summit = random.NextRange(settings.summitLow, settings.summitHigh);
		const int sidePeaks = settings.sidePeaksLow + static_cast<int>(random.NextBelow(static_cast<uint32_t>(
		                                                  std::max(settings.sidePeaksHigh - settings.sidePeaksLow, 0) + 1)));
		if (!best.has_value())
		{
			continue;
		}
		best->summit = std::min(summit, k_HighestAltitude);
		mains.push_back(*best);
		peaks.push_back(*best);
		for (int i = 0; i < sidePeaks; ++i)
		{
			// A direction from a point in the square that is not too near its middle or its corners
			float dx = 0.0f;
			float dz = 0.0f;
			float length = 0.0f;
			do
			{
				dx = random.NextRange(-1.0f, 1.0f);
				dz = random.NextRange(-1.0f, 1.0f);
				length = std::sqrt((dx * dx) + (dz * dz));
			} while (length < 0.25f || length > 1.0f);
			const float distance = random.NextRange(settings.sideDistanceLow, settings.sideDistanceHigh) * best->radius;
			const float x = std::clamp(best->x + (dx / length * distance), 0.0f, last);
			const float z = std::clamp(best->z + (dz / length * distance), 0.0f, last);
			const float sideRadius = best->radius * settings.sideRadius * random.NextRange(0.8f, 1.2f);
			const float sideSummit = best->summit * settings.sideSummit * random.NextRange(0.85f, 1.1f);
			peaks.push_back({.x = x, .z = z, .radius = sideRadius, .summit = std::min(sideSummit, k_HighestAltitude)});
		}
	}
	return peaks;
}

Grid<float> terrain::MassifHeights(int side, std::span<const Peak> peaks, uint64_t seed, const MassifSettings& settings)
{
	Grid<float> massifs(side, 0.0f);
	const float last = static_cast<float>(std::max(side - 1, 1));
	const auto outlineSeed = StageSeed(seed, 20);
	const auto spurSeeds = StageSeed(seed, 21);
	for (size_t p = 0; p < peaks.size(); ++p)
	{
		const auto& peak = peaks[p];
		const auto spurSeed = StageSeed(spurSeeds, p);
		const auto reach = static_cast<int>(peak.radius * (1.0f + settings.outline)) + 1;
		const auto cx = static_cast<int>(peak.x);
		const auto cz = static_cast<int>(peak.z);
		for (int x = std::max(cx - reach, 0); x <= std::min(cx + reach, side - 1); ++x)
		{
			for (int z = std::max(cz - reach, 0); z <= std::min(cz + reach, side - 1); ++z)
			{
				const float dx = static_cast<float>(x) - peak.x;
				const float dz = static_cast<float>(z) - peak.z;
				const float distance = std::sqrt((dx * dx) + (dz * dz));
				const float bend = noise::Fractal(outlineSeed, static_cast<float>(x) / last * settings.outlineFrequency,
				                                  static_cast<float>(z) / last * settings.outlineFrequency, 2);
				const float d = distance / (peak.radius * (1.0f + (settings.outline * bend)));
				if (d >= 1.0f)
				{
					continue;
				}
				// The cone is steep at the summit and gentle at the foot
				const float rest = 1.0f - d;
				float height = peak.summit * ProfileOf(rest, settings.profile);
				if (distance > 0.0f)
				{
					// The ridged noise round the peak gives the spurs. The gullies between them are deeper down the slope,
					// and the noise turns a little with the distance, so that the spurs bend.
					const float ridge = noise::Ridged(spurSeed, (dx / distance * settings.spurWaves) + (d * settings.spurBend),
					                                  (dz / distance * settings.spurWaves) - (d * settings.spurBend * 0.5f), 3);
					height *= 1.0f - (settings.gullyDepth * (1.0f - ridge) * noise::SmoothStep(0.05f, 0.45f, d));
				}
				auto& value = massifs.At(x, z);
				value = std::max(value, height);
			}
		}
	}
	// The cliff bands: in each band the height rises gently over most of the width, then steeply
	if (settings.bandHigh > 0.0f && settings.faceWidth > 0.0f && settings.faceWidth < 1.0f)
	{
		Random random(StageSeed(seed, 22));
		const float band = random.NextRange(settings.bandLow, settings.bandHigh);
		const auto bandSeed = StageSeed(seed, 23);
		const float faceSlope = settings.faceRise / settings.faceWidth;
		const float slope = (1.0f - settings.faceRise) / (1.0f - settings.faceWidth);
		for (int x = 0; x < side; ++x)
		{
			for (int z = 0; z < side; ++z)
			{
				auto& value = massifs.At(x, z);
				if (value <= 0.0f)
				{
					continue;
				}
				const float shift =
				    settings.bandNoise * noise::Fractal(bandSeed, static_cast<float>(x) / last * settings.bandFrequency,
				                                        static_cast<float>(z) / last * settings.bandFrequency, 3);
				const float moved = value + shift + band;
				const float index = std::floor(moved / band);
				const float part = (moved / band) - index;
				// The slope comes first and the cliff last, so that the bands never raise the cone
				const float rest = 1.0f - settings.faceWidth;
				const float rise = part < rest ? part * slope : (1.0f - settings.faceRise) + ((part - rest) * faceSlope);
				// The bands only lower the cone, so that no summit goes above its peak
				value = std::clamp((index * band) + (rise * band) - shift - band, 0.0f, value);
			}
		}
	}
	return massifs;
}

void terrain::RaiseMassifs(Grid<float>& heights, const Grid<float>& massifs, Grid<uint8_t>& cliffs)
{
	const int side = heights.side;
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		if (massifs.values[i] > heights.values[i] && heights.values[i] > k_SeaLevel)
		{
			heights.values[i] = massifs.values[i];
			cliffs.values[i] = 0;
		}
	}

	// A massif can close a low place of the land in all round. A flood from the sea and the edges of the map fills each
	// such hollow on the ground of the massifs, and next to it, to the height where it spills over.
	std::vector<uint8_t> onMassif(heights.values.size(), 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			if (massifs.At(x, z) <= 0.0f)
			{
				continue;
			}
			onMassif[heights.Index(x, z)] = 1;
			for (const auto& neighbour : k_Neighbours)
			{
				if (heights.Contains(x + neighbour.dx, z + neighbour.dz))
				{
					onMassif[heights.Index(x + neighbour.dx, z + neighbour.dz)] = 1;
				}
			}
		}
	}
	using Entry = std::pair<float, uint32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
	std::vector<uint8_t> reached(heights.values.size(), 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = heights.Index(x, z);
			if (heights.values[index] <= k_SeaLevel || x == 0 || z == 0 || x == side - 1 || z == side - 1)
			{
				reached[index] = 1;
				open.emplace(heights.values[index], static_cast<uint32_t>(index));
			}
		}
	}
	while (!open.empty())
	{
		const auto [level, index] = open.top();
		open.pop();
		const int x = heights.XOf(index);
		const int z = heights.ZOf(index);
		for (const auto& neighbour : k_Neighbours)
		{
			if (!heights.Contains(x + neighbour.dx, z + neighbour.dz))
			{
				continue;
			}
			const auto other = heights.Index(x + neighbour.dx, z + neighbour.dz);
			if (reached[other] != 0)
			{
				continue;
			}
			reached[other] = 1;
			auto& height = heights.values[other];
			if (onMassif[other] != 0)
			{
				height = std::max(height, level);
			}
			open.emplace(std::max(height, level), static_cast<uint32_t>(other));
		}
	}
}

void terrain::LowerHighGround(Grid<float>& heights, float knee, float ceiling)
{
	if (ceiling <= knee)
	{
		return;
	}
	for (auto& height : heights.values)
	{
		if (height > knee)
		{
			const float over = (height - knee) / (ceiling - knee);
			height = knee + ((ceiling - knee) * over / (1.0f + over));
		}
	}
}

namespace
{
/// A flood from the sea and the edges of the map: each corner that the mask has goes up to the height where the water
/// spills over, so that no pit holds water
void FillPits(Grid<float>& heights, std::span<const uint8_t> mask)
{
	const int side = heights.side;
	using Entry = std::pair<float, uint32_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
	std::vector<uint8_t> reached(heights.values.size(), 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = heights.Index(x, z);
			if (heights.values[index] <= terrain::k_SeaLevel || x == 0 || z == 0 || x == side - 1 || z == side - 1)
			{
				reached[index] = 1;
				open.emplace(heights.values[index], static_cast<uint32_t>(index));
			}
		}
	}
	while (!open.empty())
	{
		const auto [level, index] = open.top();
		open.pop();
		const int x = heights.XOf(index);
		const int z = heights.ZOf(index);
		for (const auto& neighbour : k_Neighbours)
		{
			if (!heights.Contains(x + neighbour.dx, z + neighbour.dz))
			{
				continue;
			}
			const auto other = heights.Index(x + neighbour.dx, z + neighbour.dz);
			if (reached[other] != 0)
			{
				continue;
			}
			reached[other] = 1;
			auto& height = heights.values[other];
			if (mask[other] != 0)
			{
				height = std::max(height, level);
			}
			open.emplace(std::max(height, level), static_cast<uint32_t>(other));
		}
	}
}
} // namespace

void terrain::RollGround(Grid<float>& heights, uint64_t seed, const RollSettings& settings)
{
	const int side = heights.side;
	const float last = static_cast<float>(std::max(side - 1, 1));
	Grid<uint8_t> sea(side, 0);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		sea.values[i] = heights.values[i] <= k_SeaLevel ? 1 : 0;
	}
	const auto distance = climate::DistanceToWater(sea);
	const auto rollSeed = StageSeed(seed, 24);
	const auto ridgeSeed = StageSeed(seed, 25);
	const auto partSeed = StageSeed(seed, 26);
	std::vector<uint8_t> moved(heights.values.size(), 0);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = heights.Index(x, z);
			auto& height = heights.values[index];
			const float fade = noise::SmoothStep(settings.seaStart, settings.seaFull, distance.values[index]);
			if (height <= k_SeaLevel || fade <= 0.0f)
			{
				continue;
			}
			const float u = static_cast<float>(x) / last;
			const float v = static_cast<float>(z) / last;
			const float part = noise::Fractal(partSeed, u * settings.partFrequency, v * settings.partFrequency, 2);
			const float calmEdge = (settings.calmShare * 2.0f) - 1.0f;
			const float rough = noise::SmoothStep(calmEdge * 0.5f - 0.1f, calmEdge * 0.5f + 0.1f, part);
			const float amount = settings.calmAmount + ((settings.roughAmount - settings.calmAmount) * rough);
			const float high = 1.0f - noise::SmoothStep(settings.highStart, settings.highEnd, height);
			const float fu = u * settings.frequency;
			const float fv = v * settings.frequency;
			const float shape = ((1.0f - settings.ridge) * noise::Fractal(rollSeed, fu, fv, settings.octaves)) +
			                    (settings.ridge * ((2.0f * noise::Ridged(ridgeSeed, fu, fv, settings.octaves)) - 1.0f));
			const float keep = shape > 0.0f ? high : high + ((1.0f - high) * settings.highFall);
			float change = amount * keep * fade * shape;
			if (settings.stepRise > 0.0f && settings.stepFace > 0.0f && settings.stepFace < 1.0f)
			{
				const float level = (height + change) / settings.stepRise;
				const float whole = std::floor(level);
				const float partOf = level - whole;
				const float rest = 1.0f - settings.stepFace;
				const float rise = partOf < rest ? partOf * (1.0f - settings.stepFaceRise) / rest
				                                 : (1.0f - settings.stepFaceRise) +
				                                       ((partOf - rest) * settings.stepFaceRise / settings.stepFace);
				const float stepped = (whole + rise) * settings.stepRise;
				change += (stepped - (height + change)) * high * fade;
			}
			height = std::max(height + change, k_SeaLevel + 0.01f);
			moved[index] = 1;
		}
	}
	FillPits(heights, moved);
}

void terrain::ShoreShelf(Grid<float>& heights, uint64_t seed, const ShelfSettings& settings)
{
	const int side = heights.side;
	const float last = static_cast<float>(std::max(side - 1, 1));
	Grid<uint8_t> sea(side, 0);
	for (size_t i = 0; i < heights.values.size(); ++i)
	{
		sea.values[i] = heights.values[i] <= k_SeaLevel ? 1 : 0;
	}
	const auto distance = climate::DistanceToWater(sea);
	const auto widthSeed = StageSeed(seed, 14);
	const auto bankSeed = StageSeed(seed, 15);
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const auto index = heights.Index(x, z);
			auto& height = heights.values[index];
			if (height <= k_SeaLevel)
			{
				continue;
			}
			const float u = static_cast<float>(x) / last * settings.frequency;
			const float v = static_cast<float>(z) / last * settings.frequency;
			const float width = settings.widthLow + ((settings.widthHigh - settings.widthLow) *
			                                         noise::SmoothStep(-0.5f, 0.5f, noise::Fractal(widthSeed, u, v, 2)));
			const float bank = settings.bankLow + ((settings.bankHigh - settings.bankLow) *
			                                       noise::SmoothStep(-0.5f, 0.5f, noise::Fractal(bankSeed, u, v, 2)));
			const float away = distance.values[index];
			const float strip = settings.altitude + (settings.rise * std::min(away, width));
			const float highest = strip + (bank * std::max(away - width, 0.0f));
			height = std::max(std::min(height, highest), k_SeaLevel + 0.01f);
		}
	}
}

Grid<float> terrain::Slopes(const Grid<float>& heights)
{
	Grid<float> slopes(heights.side, 0.0f);
	for (int x = 0; x < heights.side; ++x)
	{
		for (int z = 0; z < heights.side; ++z)
		{
			float steepest = 0.0f;
			for (size_t i = 0; i < 4; ++i)
			{
				const int nx = x + k_Neighbours[i].dx;
				const int nz = z + k_Neighbours[i].dz;
				if (heights.Contains(nx, nz))
				{
					steepest = std::max(steepest, std::fabs(heights.At(x, z) - heights.At(nx, nz)));
				}
			}
			slopes.At(x, z) = steepest;
		}
	}
	return slopes;
}
