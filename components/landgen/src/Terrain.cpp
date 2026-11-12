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
#include <vector>

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
				height = (settings.plainHeight * noise::SmoothStep(0.0f, 0.35f, land)) + 0.5f;
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

void terrain::ErodeThermal(Grid<float>& heights, const ThermalSettings& settings)
{
	Grid<float> change(heights.side, 0.0f);
	for (int pass = 0; pass < settings.passes; ++pass)
	{
		std::ranges::fill(change.values, 0.0f);
		for (int x = 0; x < heights.side; ++x)
		{
			for (int z = 0; z < heights.side; ++z)
			{
				const float height = heights.At(x, z);
				const float talus = settings.plainTalus +
				                    ((settings.mountainTalus - settings.plainTalus) * noise::SmoothStep(60.0f, 180.0f, height));
				for (size_t i = 0; i < 4; ++i)
				{
					const int nx = x + k_Neighbours[i].dx;
					const int nz = z + k_Neighbours[i].dz;
					if (!heights.Contains(nx, nz))
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
