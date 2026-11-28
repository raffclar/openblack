/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/LandTypes.h"

#include <cmath>

#include <algorithm>
#include <numeric>
#include <ranges>
#include <vector>

#include "landgen/Random.h"
#include "landgen/Stages.h"

using namespace openblack::landgen;
using landtypes::Type;

namespace
{
/// The sizes of an island of each type, as a part of the usual island, from the smallest to the largest that the seed
/// can pick. The usual island has about 13 square kilometres of land, and the land grows with the square of the size.
struct SizeRange
{
	float low;
	float high;
};
constexpr SizeRange k_StoryIslandSize {.low = 0.36f, .high = 0.52f};
constexpr SizeRange k_HighlandsSize {.low = 0.46f, .high = 0.58f};
constexpr SizeRange k_ValleysSize {.low = 0.46f, .high = 0.58f};
constexpr SizeRange k_ArchipelagoIslandSize {.low = 0.2f, .high = 0.34f};

/// The middles of the islands of an archipelago stay in this part of the map
constexpr float k_GroupLow = 0.22f;
constexpr float k_GroupHigh = 0.78f;
/// The half width of the usual island as a part of the map, from its middle to its coast
constexpr float k_UsualHalfWidth = 0.41f;
/// Two islands of an archipelago keep this many of their half widths apart, so that the sea stays between them
constexpr float k_IslandGap = 1.15f;
constexpr int k_PlaceTries = 400;

/// The highlands rise this many altitude units above the usual plains, and the land away from the massifs can rise
/// this many units higher before it rises slower
constexpr float k_HighlandRise = 15.0f;
/// The highlands rise to the plains in this part of the usual distance from the coast
constexpr float k_HighlandCoast = 0.6f;
/// On an island of the story size and on the valleys, the land away from the massifs can rise this many altitude units
/// higher before it rises slower. Thus its hills and terraces reach the height of the summits of the lands of the story
/// (6 to 44 summits of 60 m or more), and the massifs are not the only summits.
constexpr float k_LowlandCeilingRise = 30.0f;
/// The mountain ranges that the valleys add, from the fewest to the most
constexpr int k_ValleyRangesLow = 2;
constexpr int k_ValleyRangesHigh = 3;

/// A smaller island has fewer massifs, but each massif tries more places, so that it finds land
constexpr int k_MostMassifTries = 120;
/// From its north coast to its south coast, an island that is smaller than the usual island has this part of the change
/// of temperature of the whole map
constexpr float k_SmallClimateSpan = 0.5f;
/// The middle of a small island is between these parts of the change of temperature of the map
constexpr float k_ClimateMiddleLow = 0.25f;
constexpr float k_ClimateMiddleHigh = 0.75f;

/// Changes the massifs, the hollows and the climate for islands that are smaller than the usual island
void ScaleForIslands(uint64_t seed, terrain::BaseSettings& base, terrain::MassifSettings& massifs, climate::Settings& climate,
                     float& topHeight)
{
	const auto& islands = base.islands;
	const float area = std::accumulate(islands.begin(), islands.end(), 0.0f, [](float sum, const terrain::Outline& island) {
		return sum + (island.size * island.size);
	});
	const float largest = std::ranges::max(islands, {}, &terrain::Outline::size).size;

	// The massifs: fewer for less land, and smaller and lower on a smaller island, so that their slopes keep their
	// steepness. A small island has more massifs for its area than the big island, as the small lands of the story have
	// more summits for their area.
	massifs.radiusLow *= largest;
	massifs.radiusHigh *= largest;
	topHeight = std::max(topHeight * largest, landtypes::k_StoryTopHeight);
	const float massifShare = std::sqrt(area);
	const auto fewer = [](int count, float share) {
		return std::max(1, static_cast<int>(std::lround(static_cast<float>(count) * share)));
	};
	massifs.countLow = massifs.countLow > 0 ? fewer(massifs.countLow, massifShare) : 0;
	massifs.countHigh = massifs.countHigh > 0 ? std::max(fewer(massifs.countHigh, massifShare), massifs.countLow) : 0;
	massifs.tries = std::min(static_cast<int>(std::ceil(static_cast<float>(massifs.tries) / area)), k_MostMassifTries);
	base.hollows = base.hollows > 0 ? fewer(base.hollows, area) : 0;

	// The climate: the seed puts the land at a place in the change of temperature of the map. The land keeps a change
	// from its north coast to its south coast.
	const auto [north, south] =
	    std::ranges::minmax(islands | std::views::transform([](const terrain::Outline& island) { return island.centreV; }));
	float northCoast = 1.0f;
	float southCoast = 0.0f;
	for (const auto& island : islands)
	{
		northCoast = std::min(northCoast, island.centreV - (island.size * k_UsualHalfWidth));
		southCoast = std::max(southCoast, island.centreV + (island.size * k_UsualHalfWidth));
	}
	const float extent = std::max(southCoast - northCoast, 0.05f);
	const float span = climate.southTemperature - climate.northTemperature;
	Random random(StageSeed(seed, Stage::LandTypeIslandScale));
	const float middle = climate.northTemperature + (span * random.NextRange(k_ClimateMiddleLow, k_ClimateMiddleHigh));
	const float change = span * k_SmallClimateSpan / extent;
	const float centre = (north + south) * 0.5f;
	climate.northTemperature = middle - (change * centre);
	climate.southTemperature = middle + (change * (1.0f - centre));
}

/// The islands of an archipelago, apart from each other
std::vector<terrain::Outline> ArchipelagoIslands(uint64_t seed)
{
	Random random(StageSeed(seed, Stage::LandTypeArchipelago));
	const int count =
	    landtypes::k_FewestIslands +
	    static_cast<int>(random.NextBelow(static_cast<uint32_t>(landtypes::k_MostIslands - landtypes::k_FewestIslands + 1)));
	std::vector<terrain::Outline> islands;
	for (int tries = 0; tries < k_PlaceTries && static_cast<int>(islands.size()) < count; ++tries)
	{
		const terrain::Outline island {
		    .centreU = random.NextRange(k_GroupLow, k_GroupHigh),
		    .centreV = random.NextRange(k_GroupLow, k_GroupHigh),
		    .size = random.NextRange(k_ArchipelagoIslandSize.low, k_ArchipelagoIslandSize.high),
		};
		const bool apart = std::ranges::none_of(islands, [&island](const terrain::Outline& other) {
			const float du = other.centreU - island.centreU;
			const float dv = other.centreV - island.centreV;
			const float reach = (other.size + island.size) * k_UsualHalfWidth * k_IslandGap;
			return (du * du) + (dv * dv) < reach * reach;
		});
		if (apart)
		{
			islands.push_back(island);
		}
	}
	return islands;
}

/// One island in the middle of the map, with a size from the range
terrain::Outline OneIsland(uint64_t seed, SizeRange size)
{
	Random random(StageSeed(seed, Stage::LandTypeOneIsland));
	return {.centreU = 0.5f, .centreV = 0.5f, .size = random.NextRange(size.low, size.high)};
}
} // namespace

std::string_view landtypes::NameOf(Type type)
{
	switch (type)
	{
	case Type::StoryIsland:
		return "island of the story size";
	case Type::BigIsland:
		return "big island";
	case Type::Highlands:
		return "highlands";
	case Type::Valleys:
		return "valleys";
	case Type::Archipelago:
		return "archipelago";
	}
	return "big island";
}

Type landtypes::Pick(uint64_t seed, const Settings& settings)
{
	if (settings.type.has_value())
	{
		return *settings.type;
	}
	auto weights = settings.weights;
	if (!settings.archipelago)
	{
		weights[static_cast<size_t>(Type::Archipelago)] = 0;
	}
	const uint32_t total = std::accumulate(weights.begin(), weights.end(), 0u);
	if (total == 0)
	{
		return Type::BigIsland;
	}
	uint32_t pick = Random(StageSeed(seed, Stage::LandTypePick)).NextBelow(total);
	for (size_t i = 0; i < weights.size(); ++i)
	{
		if (pick < weights[i])
		{
			return static_cast<Type>(i);
		}
		pick -= weights[i];
	}
	return Type::BigIsland;
}

void landtypes::Apply(Type type, uint64_t seed, terrain::BaseSettings& base, terrain::MassifSettings& massifs,
                      climate::Settings& climate, float& topHeight)
{
	switch (type)
	{
	case Type::BigIsland:
		return;
	case Type::StoryIsland:
		base.islands = {OneIsland(seed, k_StoryIslandSize)};
		massifs.lowlandKnee += k_LowlandCeilingRise;
		massifs.lowlandCeiling += k_LowlandCeilingRise;
		break;
	case Type::Highlands:
		base.islands = {OneIsland(seed, k_HighlandsSize)};
		base.plainHeight += k_HighlandRise;
		base.plainRise *= k_HighlandCoast;
		massifs.lowlandKnee += k_HighlandRise;
		massifs.lowlandCeiling += k_HighlandRise;
		break;
	case Type::Valleys:
	{
		base.islands = {OneIsland(seed, k_ValleysSize)};
		Random random(StageSeed(seed, Stage::LandTypeApply));
		massifs.lowlandKnee += k_LowlandCeilingRise;
		massifs.lowlandCeiling += k_LowlandCeilingRise;
		base.ranges += k_ValleyRangesLow +
		               static_cast<int>(random.NextBelow(static_cast<uint32_t>(k_ValleyRangesHigh - k_ValleyRangesLow + 1)));
		break;
	}
	case Type::Archipelago:
		base.islands = ArchipelagoIslands(seed);
		break;
	}
	if (!base.islands.empty())
	{
		ScaleForIslands(seed, base, massifs, climate, topHeight);
	}
}
