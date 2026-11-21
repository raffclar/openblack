/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Players.h"

#include <cmath>

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>

#include "landgen/Random.h"
#include "landgen/Terrain.h"

using namespace openblack::landgen;
using namespace openblack::landgen::players;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
constexpr float k_MetresPerAltitude = 0.67f;
/// The stage of the seeds of the starts of the other players
constexpr uint64_t k_PlayerStage = 130;

[[nodiscard]] float Distance(float ax, float az, float bx, float bz)
{
	const float dx = ax - bx;
	const float dz = az - bz;
	return std::sqrt((dx * dx) + (dz * dz));
}

/// The most rise or fall from the middle to the ground of the flat circle, in altitudes
[[nodiscard]] float RoughnessAt(const Grid<float>& heights, int x, int z, int reach)
{
	const float height = heights.At(x, z);
	float rough = 0.0f;
	for (int dx = -reach; dx <= reach; dx += 3)
	{
		for (int dz = -reach; dz <= reach; dz += 3)
		{
			rough = std::max(rough, std::fabs(heights.At(x + dx, z + dz) - height));
		}
	}
	return rough;
}

/// A place that the search can use, with what the search compares
struct Candidate
{
	Start place;
	float rough;
	float height;
	StartGround ground;
};

/// The part of the difference from the reference value, or 0 when the reference is 0
[[nodiscard]] float ShareOff(float value, float reference)
{
	return reference > 0.0f ? std::fabs(value - reference) / reference : 0.0f;
}
} // namespace

Grid<float> players::Rises(const Grid<float>& heights)
{
	const int side = heights.side;
	Grid<float> rises(side, 0.0f);
	// The rise along one axis, in metres for each metre
	const auto along = [&heights, side](int x, int z, int dx, int dz) {
		const int lowX = std::max(x - dx, 0);
		const int lowZ = std::max(z - dz, 0);
		const int highX = std::min(x + dx, side - 1);
		const int highZ = std::min(z + dz, side - 1);
		const int run = (highX - lowX) + (highZ - lowZ);
		if (run == 0)
		{
			return 0.0f;
		}
		return (heights.At(highX, highZ) - heights.At(lowX, lowZ)) * k_MetresPerAltitude /
		       (static_cast<float>(run) * k_MetresPerCorner);
	};
	for (int x = 0; x < side; ++x)
	{
		for (int z = 0; z < side; ++z)
		{
			const float rx = along(x, z, 1, 0);
			const float rz = along(x, z, 0, 1);
			rises.At(x, z) = std::sqrt((rx * rx) + (rz * rz));
		}
	}
	return rises;
}

StartGround players::GroundAt(const Grid<float>& heights, const Grid<float>& rises, float x, float z, const Settings& settings)
{
	const float reach = settings.groundRadius / k_MetresPerCorner;
	const int whole = static_cast<int>(reach);
	const int step = std::max(settings.countStep, 1);
	const auto cx = static_cast<int>(x);
	const auto cz = static_cast<int>(z);
	const auto cell = static_cast<float>(step * step);
	StartGround ground {.land = 0.0f, .flat = 0.0f};
	for (int dx = -whole; dx <= whole; dx += step)
	{
		for (int dz = -whole; dz <= whole; dz += step)
		{
			const int px = cx + dx;
			const int pz = cz + dz;
			if (!heights.Contains(px, pz) || static_cast<float>((dx * dx) + (dz * dz)) > reach * reach ||
			    heights.At(px, pz) <= terrain::k_SeaLevel)
			{
				continue;
			}
			ground.land += cell;
			if (rises.At(px, pz) < settings.flatRise)
			{
				ground.flat += cell;
			}
		}
	}
	return ground;
}

std::vector<Start> players::FindStarts(const Grid<float>& heights, const Grid<float>& seaDistance,
                                       const Grid<float>& riverDistance, Start first, std::span<const Start> townOffsets,
                                       const Ground& ground, const Settings& settings)
{
	const uint32_t count = std::min({settings.count, k_MostPlayers, static_cast<uint32_t>(townOffsets.size())});
	if (count <= 1)
	{
		return {};
	}
	const int side = heights.side;
	const float last = static_cast<float>(std::max(side - 1, 1));
	const auto reach = static_cast<int>(ground.flatRadius);
	const int step = std::max(settings.step, 1);
	const auto rises = Rises(heights);

	const auto firstX = static_cast<int>(first.x);
	const auto firstZ = static_cast<int>(first.z);
	const Candidate reference {
	    .place = first,
	    .rough = RoughnessAt(heights, firstX, firstZ, reach),
	    .height = heights.At(firstX, firstZ),
	    .ground = GroundAt(heights, rises, first.x, first.z, settings),
	};
	const float nearest = settings.nearestRival / k_MetresPerCorner;
	const float farthest = settings.farthestRival / k_MetresPerCorner;

	// Each place that meets the rules of a start, as for the start of player one, and not too near player one
	std::vector<Candidate> candidates;
	for (int x = reach; x < side - reach; x += step)
	{
		for (int z = reach; z < side - reach; z += step)
		{
			const float height = heights.At(x, z);
			if (height < ground.lowest || height > ground.highest || seaDistance.At(x, z) < ground.nearestSea ||
			    seaDistance.At(x, z) > ground.farthestSea || riverDistance.At(x, z) < ground.riverDistance)
			{
				continue;
			}
			const auto fx = static_cast<float>(x);
			const auto fz = static_cast<float>(z);
			candidates.push_back({.place = {.x = fx, .z = fz},
			                      .rough = RoughnessAt(heights, x, z, reach),
			                      .height = height,
			                      .ground = GroundAt(heights, rises, fx, fz, settings)});
		}
	}

	// The cost of a place: its roughness, and how far its height, its climate and its ground are from those of player one
	const float firstNorth = first.z / last;
	const auto cost = [&](const Candidate& candidate) {
		return candidate.rough + (settings.heightCost * std::fabs(candidate.height - reference.height)) +
		       (settings.climateCost * std::fabs((candidate.place.z / last) - firstNorth)) +
		       (settings.landCost * ShareOff(candidate.ground.land, reference.ground.land)) +
		       (settings.flatCost * ShareOff(candidate.ground.flat, reference.ground.flat));
	};

	std::vector<Start> placed {first};
	std::vector<Start> towns {{.x = first.x + townOffsets[0].x, .z = first.z + townOffsets[0].z}};
	while (placed.size() < count)
	{
		const auto& offset = townOffsets[placed.size()];
		std::optional<size_t> best;
		float bestCost = 0.0f;
		bool bestInReach = false;
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			const float townX = candidates[i].place.x + offset.x;
			const float townZ = candidates[i].place.z + offset.z;
			float closest = std::numeric_limits<float>::max();
			for (const auto& town : towns)
			{
				closest = std::min(closest, Distance(townX, townZ, town.x, town.z));
			}
			if (closest < nearest)
			{
				continue;
			}
			// A place in the reach of a town of a start comes before a place out of the reach
			const bool inReach = closest <= farthest;
			const float placeCost = cost(candidates[i]);
			if (!best.has_value() || (inReach && !bestInReach) || (inReach == bestInReach && placeCost < bestCost))
			{
				best = i;
				bestCost = placeCost;
				bestInReach = inReach;
			}
		}
		if (!best.has_value())
		{
			break;
		}
		const auto& start = candidates[*best].place;
		placed.push_back(start);
		towns.push_back({.x = start.x + offset.x, .z = start.z + offset.z});
	}
	return {placed.begin() + 1, placed.end()};
}

size_t players::TreesNear(std::span<const features::Feature> features, float townX, float townZ, float radius)
{
	return static_cast<size_t>(std::ranges::count_if(features, [&](const features::Feature& feature) {
		return feature.kind == features::Kind::Tree && feature.type != static_cast<uint8_t>(features::Tree::Burnt) &&
		       Distance(feature.x, feature.z, townX, townZ) <= radius;
	}));
}

void players::EvenTheWoods(std::vector<features::Feature>& features, const features::Ground& ground,
                           std::span<const Start> towns, const Settings& settings)
{
	if (towns.size() < 2)
	{
		return;
	}
	const auto metresOf = [](const Start& town) {
		return Start {.x = town.x * k_MetresPerCorner, .z = town.z * k_MetresPerCorner};
	};
	const auto near = [&](float x, float z) {
		return std::ranges::any_of(features, [&](const features::Feature& other) {
			const float gap =
			    other.kind == features::Kind::BigForest ? settings.bigForestClearing * other.size : settings.treeGap;
			return Distance(x, z, other.x, other.z) < gap;
		});
	};
	// The far woods first, then the near woods, so that the near woods keep their share
	for (const float radius : settings.woodRadii)
	{
		std::vector<size_t> counts;
		for (const auto& town : towns)
		{
			const auto place = metresOf(town);
			counts.push_back(TreesNear(features, place.x, place.z, radius));
		}
		const auto richest = static_cast<size_t>(std::distance(counts.begin(), std::ranges::max_element(counts)));
		const auto source = metresOf(towns[richest]);
		// The trees of the richest woods, in their order on the land
		std::vector<features::Feature> woods;
		std::ranges::copy_if(features, std::back_inserter(woods), [&](const features::Feature& feature) {
			return feature.kind == features::Kind::Tree && feature.type != static_cast<uint8_t>(features::Tree::Burnt) &&
			       Distance(feature.x, feature.z, source.x, source.z) <= radius;
		});
		const auto target = static_cast<size_t>(std::ceil(static_cast<float>(counts[richest]) * settings.woodShare));
		for (size_t start = 0; start < towns.size(); ++start)
		{
			const auto place = metresOf(towns[start]);
			// The copies keep the place of each tree from the town. When the ground does not take enough copies, the
			// places turn about the town by a quarter turn, then by a half turn and by three quarter turns.
			for (int turn = 0; turn < 4 && counts[start] < target; ++turn)
			{
				for (const auto& tree : woods)
				{
					if (counts[start] >= target)
					{
						break;
					}
					float dx = tree.x - source.x;
					float dz = tree.z - source.z;
					for (int quarter = 0; quarter < turn; ++quarter)
					{
						const float along = dx;
						dx = -dz;
						dz = along;
					}
					const float x = place.x + dx;
					const float z = place.z + dz;
					const float cornerX = x / k_MetresPerCorner;
					const float cornerZ = z / k_MetresPerCorner;
					if (!features::IsFree(ground, cornerX, cornerZ) ||
					    !features::Suits(static_cast<features::Tree>(tree.type), features::SpotAt(ground, cornerX, cornerZ)) ||
					    near(x, z))
					{
						continue;
					}
					auto copy = tree;
					copy.x = x;
					copy.z = z;
					copy.forest = 0;
					features.push_back(copy);
					++counts[start];
				}
			}
		}
	}
}

uint64_t players::SeedOfPlayer(uint64_t seed, uint32_t player)
{
	return StageSeed(StageSeed(seed, k_PlayerStage), player);
}
