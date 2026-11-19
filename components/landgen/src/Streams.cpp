/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Streams.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <numeric>
#include <utility>

#include "landgen/Random.h"

using namespace openblack::landgen;
using namespace openblack::landgen::streams;
using openblack::landgen::biomes::Biome;
using openblack::landgen::features::Tree;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
/// The area of the land round one corner, in square kilometres
constexpr float k_SquareKmPerCorner = k_MetresPerCorner * k_MetresPerCorner / 1.0e6f;
/// The metres of one altitude unit on the lands of the story
constexpr float k_MetresPerAltitude = 0.67f;

// The trees 10 m or less from the streams of the palm island of the story
constexpr std::array<BankTree, 5> k_WarmBank {{
    {Tree::Cedar, 12},
    {Tree::Bush, 7},
    {Tree::BushB, 6},
    {Tree::PalmC, 3},
    {Tree::Birch, 1},
}};
// The trees 10 m or less from the streams of the dry land of scrub and sand of the story. The hedges stay in the towns.
constexpr std::array<BankTree, 15> k_DryBank {{
    {Tree::Bush, 107},
    {Tree::Birch, 34},
    {Tree::Beech, 18},
    {Tree::Olive, 12},
    {Tree::Palm, 4},
    {Tree::PalmC, 4},
    {Tree::Cedar, 4},
    {Tree::BushB, 4},
    {Tree::Conifer, 3},
    {Tree::PalmA, 3},
    {Tree::ConiferA, 2},
    {Tree::Oak, 2},
    {Tree::PalmB, 2},
    {Tree::BushA, 2},
    {Tree::OakA, 1},
}};
// The trees 10 m or less from the streams of the two green lands of the story
constexpr std::array<BankTree, 14> k_GreenBank {{
    {Tree::Bush, 73},
    {Tree::Oak, 19},
    {Tree::Conifer, 16},
    {Tree::Birch, 15},
    {Tree::Beech, 12},
    {Tree::ConiferA, 10},
    {Tree::BushB, 7},
    {Tree::PalmA, 6},
    {Tree::OakA, 6},
    {Tree::PalmC, 6},
    {Tree::Palm, 5},
    {Tree::Olive, 4},
    {Tree::PalmB, 4},
    {Tree::Pine, 2},
}};
// The conifers of the green banks, for the cold lands
constexpr std::array<BankTree, 3> k_ColdBank {{
    {Tree::Conifer, 16},
    {Tree::ConiferA, 10},
    {Tree::Pine, 2},
}};

/// The steps of the random number for the spreads of the stands and the scales: the least, a tenth, a quarter, the
/// median, three quarters, nine tenths and the most
constexpr std::array<float, 7> k_Steps {0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f};
constexpr std::array<float, 7> k_StandSizes {1.0f, 1.0f, 1.0f, 3.0f, 5.0f, 12.2f, 74.0f};
constexpr std::array<float, 7> k_Scales {0.354f, 0.75f, 0.97f, 1.119f, 1.522f, 2.011f, 3.07f};

float Interpolate(const std::array<float, 7>& values, float unit)
{
	const auto upper = std::ranges::upper_bound(k_Steps, unit);
	if (upper == k_Steps.begin())
	{
		return values.front();
	}
	if (upper == k_Steps.end())
	{
		return values.back();
	}
	const auto i = static_cast<size_t>(upper - k_Steps.begin());
	const float t = (unit - k_Steps.at(i - 1)) / (k_Steps.at(i) - k_Steps.at(i - 1));
	return values.at(i - 1) + ((values.at(i) - values.at(i - 1)) * t);
}

/// The lengths of the streams of the story in metres: the shortest, a tenth, a quarter, the median, three quarters, nine
/// tenths and the longest
constexpr std::array<float, 7> k_StreamLengths {27.0f, 72.0f, 130.0f, 183.0f, 259.0f, 411.0f, 639.0f};

/// The number of corners of a stream, from a random number from 0 to below 1
size_t StreamCorners(float unit, float metresPerCorner);

/// Whether a corner or one of the eight corners round it is in the water
bool NearWater(const Grid<uint8_t>& water, uint32_t corner)
{
	const int x = water.XOf(corner);
	const int z = water.ZOf(corner);
	for (int dx = -1; dx <= 1; ++dx)
	{
		for (int dz = -1; dz <= 1; ++dz)
		{
			if (water.Contains(x + dx, z + dz) && water.At(x + dx, z + dz) != 0)
			{
				return true;
			}
		}
	}
	return false;
}

size_t StreamCorners(float unit, float metresPerCorner)
{
	const float metres = Interpolate(k_StreamLengths, unit);
	return static_cast<size_t>(std::floor((metres / metresPerCorner) + 0.5f)) + 1;
}

/// Whether a corner of a stream that stays is this many corners or less from a corner, along each axis
bool NearStream(const Grid<uint8_t>& kept, uint32_t corner, int gap)
{
	const int x = kept.XOf(corner);
	const int z = kept.ZOf(corner);
	for (int dx = -gap; dx <= gap; ++dx)
	{
		for (int dz = -gap; dz <= gap; ++dz)
		{
			if (kept.Contains(x + dx, z + dz) && kept.At(x + dx, z + dz) != 0)
			{
				return true;
			}
		}
	}
	return false;
}

/// The points of a stream: one at each corner, moved by the seed from the middle of the corner. The last point stays at
/// its corner, so that a stream that joins another meets it.
std::vector<hydrology::Point> PointsOf(const hydrology::River& river, const Grid<uint8_t>& water, uint64_t seed, float wander)
{
	std::vector<hydrology::Point> points;
	points.reserve(river.corners.size());
	for (size_t i = 0; i < river.corners.size(); ++i)
	{
		const auto corner = river.corners[i];
		const int x = water.XOf(corner);
		const int z = water.ZOf(corner);
		auto px = static_cast<float>(x);
		auto pz = static_cast<float>(z);
		if (i + 1 < river.corners.size())
		{
			Random random(Hash(seed, x, z));
			px += random.NextRange(-wander, wander);
			pz += random.NextRange(-wander, wander);
		}
		points.push_back({.x = px, .z = pz});
	}
	return points;
}
} // namespace

std::vector<hydrology::River> streams::Choose(std::span<const hydrology::River> rivers, std::span<const int32_t> receivers,
                                              const Grid<float>& heights, const Grid<uint8_t>& water, size_t landCorners,
                                              uint64_t seed, const Settings& settings)
{
	Random random(StageSeed(seed, 140));
	const float density = random.NextRange(settings.fewestPerKm2, settings.mostPerKm2);
	const float area = static_cast<float>(landCorners) * k_SquareKmPerCorner;
	const auto wanted = std::max(static_cast<size_t>(std::floor((area * density) + 0.5f)), settings.fewest);

	// The places where a stream can start, each with the most corners of its stream:
	// - a spring on a hill, at a point of a grid that the seed moves a little;
	// - a corner of a river. A longer river starts lower down, so that the stream keeps the end of the river.
	// Each place is as high as a spring or higher.
	// Each stream has a length from the spread of the streams of the story.
	struct Start
	{
		uint32_t corner;
		size_t most;
		float fall;
	};
	std::vector<Start> starts;
	const auto step = std::max(settings.springSpacing, 1);
	for (int gx = 0; gx < heights.side; gx += step)
	{
		for (int gz = 0; gz < heights.side; gz += step)
		{
			Random place(Hash(StageSeed(seed, 143), gx, gz));
			const int x = std::min(gx + static_cast<int>(place.NextBelow(static_cast<uint32_t>(step))), heights.side - 1);
			const int z = std::min(gz + static_cast<int>(place.NextBelow(static_cast<uint32_t>(step))), heights.side - 1);
			const float metres = heights.At(x, z) * k_MetresPerAltitude;
			if (water.At(x, z) == 0 && metres >= settings.springLowest && metres <= settings.springHighest)
			{
				starts.push_back({static_cast<uint32_t>(heights.Index(x, z)),
				                  StreamCorners(place.NextUnit(), settings.metresPerCorner), 0.0f});
			}
		}
	}
	for (const auto& river : rivers)
	{
		const auto most = StreamCorners(random.NextUnit(), settings.metresPerCorner);
		if (!river.corners.empty())
		{
			const auto skip = river.corners.size() > most ? river.corners.size() - most : 0;
			const auto corner = river.corners[skip];
			if (heights.values[corner] * k_MetresPerAltitude >= settings.springLowest)
			{
				starts.push_back({corner, most, 0.0f});
			}
		}
	}

	// The path of the water from a corner down: it stops at its first corner in the water, on a corner of a stream that stays,
	// or after the most corners. It gives no corners when the water does not get to the sea, a lake or a stream that stays.
	Grid<uint8_t> keptCorners(water.side, 0);
	const auto pathOf = [&](uint32_t start, size_t most, bool joins) {
		std::vector<uint32_t> path {start};
		while (true)
		{
			const auto corner = path.back();
			if (water.values[corner] != 0 || (joins && path.size() > 1 && keptCorners.values[corner] != 0))
			{
				return path;
			}
			const auto next = receivers[corner];
			if (next < 0)
			{
				return path;
			}
			if (path.size() >= most)
			{
				return std::vector<uint32_t> {};
			}
			path.push_back(static_cast<uint32_t>(next));
		}
	};

	// The streams that fall the most come first, as the streams of the story run down the hills. The fall has a random
	// part, so that some streams that fall less also stay.
	for (auto& start : starts)
	{
		auto path = pathOf(start.corner, start.most, false);
		if (path.empty())
		{
			// A stream that only a join lets stay: its fall along its most corners
			std::vector<uint32_t> corners {start.corner};
			while (corners.size() < start.most && receivers[corners.back()] >= 0)
			{
				corners.push_back(static_cast<uint32_t>(receivers[corners.back()]));
			}
			path = std::move(corners);
		}
		if (path.size() >= 2)
		{
			// The height of the source counts with the fall, so that the streams start high on the hills
			const float fall = (2.0f * heights.values[path.front()]) - heights.values[path.back()];
			start.fall = fall * random.NextRange(1.0f - settings.mix, 1.0f + settings.mix);
		}
	}
	std::ranges::stable_sort(starts, [](const Start& a, const Start& b) { return a.fall > b.fall; });

	// A stream stays when it gets to the water, or to a corner of a stream that stays. Each pass can let a stream stay
	// that joins a stream of the pass before.
	std::vector<uint8_t> used(starts.size(), 0);
	std::vector<hydrology::River> chosen;
	bool changed = true;
	while (changed && chosen.size() < wanted)
	{
		changed = false;
		for (size_t index = 0; index < starts.size() && chosen.size() < wanted; ++index)
		{
			const auto& start = starts[index];
			if (used[index] != 0 || NearStream(keptCorners, start.corner, settings.sourceGap))
			{
				continue;
			}
			auto path = pathOf(start.corner, start.most, true);
			if (path.size() < std::max(settings.shortest, size_t {2}))
			{
				continue;
			}
			used[index] = 1;
			changed = true;
			for (const auto corner : path)
			{
				keptCorners.values[corner] = 1;
			}
			hydrology::River stream;
			stream.corners = std::move(path);
			chosen.push_back(std::move(stream));
		}
	}
	for (auto& stream : chosen)
	{
		stream.points = PointsOf(stream, water, StageSeed(seed, 141), settings.wander);
	}
	return chosen;
}

bool streams::Reaches(const hydrology::River& river, std::span<const hydrology::River> rivers, const Grid<uint8_t>& water)
{
	if (river.corners.empty())
	{
		return false;
	}
	const auto last = river.corners.back();
	if (NearWater(water, last))
	{
		return true;
	}
	return std::ranges::any_of(rivers, [&river, last](const hydrology::River& other) {
		return &other != &river && std::ranges::find(other.corners, last) != other.corners.end();
	});
}

std::span<const BankTree> streams::BankTreesOf(Biome biome)
{
	switch (biome)
	{
	case Biome::Beach:
	case Biome::DenseJungle:
		return k_WarmBank;
	case Biome::Desert:
		return k_DryBank;
	case Biome::Grassland:
	case Biome::TemperateForest:
	case Biome::Swamp:
		return k_GreenBank;
	case Biome::NordicWastes:
	case Biome::Alpine:
		return k_ColdBank;
	default:
		return {};
	}
}

uint32_t streams::BankStandSize(float unit)
{
	return std::max(static_cast<uint32_t>(std::floor(Interpolate(k_StandSizes, unit) + 0.5f)), 1u);
}

float streams::BankTreeScale(float unit)
{
	return Interpolate(k_Scales, unit);
}

std::vector<features::Feature> streams::PlaceBankTrees(const features::Ground& ground,
                                                       std::span<const hydrology::River> streams, uint64_t seed,
                                                       const BankSettings& settings)
{
	std::vector<features::Feature> trees;
	const auto pick = [](std::span<const BankTree> kinds, float unit) {
		const auto total = std::accumulate(kinds.begin(), kinds.end(), uint32_t {0},
		                                   [](uint32_t sum, const BankTree& kind) { return sum + kind.weight; });
		auto roll = static_cast<uint32_t>(unit * static_cast<float>(total));
		for (const auto& kind : kinds)
		{
			if (roll < kind.weight)
			{
				return kind.tree;
			}
			roll -= kind.weight;
		}
		return kinds.back().tree;
	};
	for (size_t index = 0; index < streams.size(); ++index)
	{
		const auto& points = streams[index].points;
		if (points.size() < 2)
		{
			continue;
		}
		Random random(Mix(StageSeed(seed, 142) + index));
		// The distance along the stream in metres where the next stand starts, and the trees of the stand still to come
		float along = 0.0f;
		const float gapForEachTree = settings.treeSpacing * (1.0f - settings.cover) / settings.cover;
		float next = random.NextRange(0.0f, 2.0f) * gapForEachTree * static_cast<float>(BankStandSize(0.5f));
		uint32_t left = 0;
		uint32_t stand = 0;
		for (size_t i = 0; i + 1 < points.size(); ++i)
		{
			const float ax = points[i].x * k_MetresPerCorner;
			const float az = points[i].z * k_MetresPerCorner;
			const float dx = (points[i + 1].x * k_MetresPerCorner) - ax;
			const float dz = (points[i + 1].z * k_MetresPerCorner) - az;
			const float length = std::sqrt((dx * dx) + (dz * dz));
			if (length <= 0.0f)
			{
				continue;
			}
			while (next < along + length)
			{
				if (left == 0)
				{
					stand = BankStandSize(random.NextUnit());
					left = stand;
				}
				const float t = (next - along) / length;
				const float side = random.NextUnit() < 0.5f ? -1.0f : 1.0f;
				const float offset = side * random.NextRange(settings.nearest, settings.farthest);
				const float x = ax + (dx * t) - (dz / length * offset);
				const float z = az + (dz * t) + (dx / length * offset);
				const float unit = random.NextUnit();
				const float angle = random.NextRange(0.0f, 2.0f * std::numbers::pi_v<float>);
				const float scale = BankTreeScale(random.NextUnit());
				const float cx = x / k_MetresPerCorner;
				const float cz = z / k_MetresPerCorner;
				if (features::IsFree(ground, cx, cz))
				{
					const auto biome =
					    ground.biomes.At(static_cast<int>(std::floor(cx + 0.5f)), static_cast<int>(std::floor(cz + 0.5f)));
					const auto kinds = BankTreesOf(biome);
					if (!kinds.empty())
					{
						const auto tree = pick(kinds, unit);
						if (features::Suits(tree, features::SpotAt(ground, cx, cz)))
						{
							trees.push_back({.kind = features::Kind::Tree,
							                 .type = static_cast<uint8_t>(tree),
							                 .x = x,
							                 .z = z,
							                 .angle = angle,
							                 .size = scale,
							                 .maxSize = scale});
						}
					}
				}
				--left;
				// The next tree of the stand, or the gap to the next stand. A larger stand has a larger gap after it, so
				// that the stands cover their part of the banks.
				next += settings.treeSpacing;
				if (left == 0)
				{
					next += random.NextRange(0.0f, 2.0f) * gapForEachTree * static_cast<float>(stand);
				}
			}
			along += length;
		}
	}
	return trees;
}
