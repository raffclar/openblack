/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "landgen/Settlement.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <numbers>
#include <numeric>
#include <optional>
#include <utility>

#include "landgen/Random.h"
#include "landgen/Stages.h"

using namespace openblack::landgen;
using namespace openblack::landgen::settlement;

namespace
{
constexpr float k_MetresPerCorner = 10.0f;
/// The side of a field, in metres. The lands of the story put the middle of each field 5 metres past a line of 10 metres.
constexpr float k_FieldSide = 10.0f;
/// The highest turn, in thousandths of a radian
constexpr uint32_t k_FullTurn = 6283;
/// The most distance between a person and the house of the person, along each axis, in metres
constexpr float k_PersonShift = 4.0f;

/// A direction along an axis of the map
struct Axis
{
	float x;
	float z;
};
constexpr std::array<Axis, 4> k_Axes {{{1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {0.0f, -1.0f}}};

/// A place for a building, in steps across the line of the start and along it from the town centre
struct Slot
{
	int across;
	int along;
};
/// The places round the town centre: four beside it, and five in the row after it, away from the temple
constexpr std::array<Slot, 9> k_Slots {
    {{-2, 0}, {-1, 0}, {1, 0}, {2, 0}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1}},
};
/// The places for a workshop and for a graveyard, at the two ends of the rows of houses.
/// The towns of the story put the workshop 65 m from the centre and the graveyard 109 m (the medians).
constexpr std::array<Slot, 2> k_WorkshopSlots {{{-3, 1}, {3, 1}}};
constexpr std::array<Slot, 2> k_GraveyardSlots {{{-3, 0}, {3, 0}}};
/// The places for a wonder, behind the town centre on the side away from the fields. The towns of the story put a wonder
/// 66 m from the centre (the median).
constexpr std::array<Slot, 2> k_WonderSlots {{{-1, -3}, {1, -3}}};

/// Whether a place in metres is free of what the land keeps clear
bool IsFree(const Grid<uint8_t>& keepClear, float x, float z)
{
	const int side = keepClear.side;
	if (side == 0)
	{
		return true;
	}
	// NOLINTBEGIN(bugprone-incorrect-roundings): A change gives a different land.
	const auto cx = std::clamp(static_cast<int>((x / k_MetresPerCorner) + 0.5f), 0, side - 1);
	const auto cz = std::clamp(static_cast<int>((z / k_MetresPerCorner) + 0.5f), 0, side - 1);
	// NOLINTEND(bugprone-incorrect-roundings)
	return keepClear.At(cx, cz) == 0;
}

/// A whole number from the first value to the second value
uint32_t Between(Random& random, uint32_t fewest, uint32_t most)
{
	return most > fewest ? fewest + random.NextBelow(most - fewest + 1) : fewest;
}

/// The middle of the 10 metre square that holds a value, as the fields of the lands of the story are
float OnFieldLine(float value)
{
	return (std::floor(value / k_FieldSide) * k_FieldSide) + (k_FieldSide * 0.5f);
}

float Distance(float ax, float az, float bx, float bz)
{
	return std::sqrt(((ax - bx) * (ax - bx)) + ((az - bz) * (az - bz)));
}

/// The place in metres of a slot of a town, moved by a random amount
std::pair<float, float> PlaceOf(const Town& town, Axis along, Axis across, Slot slot, Random& random, const Settings& settings)
{
	const float stepAcross = static_cast<float>(slot.across) * settings.buildingSpacing;
	const float stepAlong = static_cast<float>(slot.along) * settings.buildingSpacing;
	const float x = town.x + (across.x * stepAcross) + (along.x * stepAlong) +
	                random.NextRange(-settings.buildingShift, settings.buildingShift);
	const float z = town.z + (across.z * stepAcross) + (along.z * stepAlong) +
	                random.NextRange(-settings.buildingShift, settings.buildingShift);
	return {x, z};
}

/// A workshop or a graveyard, the share of the towns that have it, and its places
struct Extra
{
	Building building;
	float share;
	std::array<Slot, 2> slots;
};

/// A building that the town puts in a slot, and whether it is a planned building
struct Wanted
{
	Building building;
	bool planned;
};

/// The count of the buildings before the houses: the storage pit and the creche
constexpr size_t k_FixedBuildings = 2;

/// Adds the town centre with a random turn
void AddCentre(Town& town, Random& random)
{
	town.buildings.push_back({.building = Building::TownCentre,
	                          .x = town.x,
	                          .z = town.z,
	                          .rotation = static_cast<int32_t>(random.NextBelow(k_FullTurn))});
}

/// Puts each building in a slot round the centre. The seed puts the slots in a random order.
void AddInSlots(Town& town, std::span<const Wanted> buildings, Axis along, Axis across, const Grid<uint8_t>& keepClear,
                Random& random, const Settings& settings)
{
	auto slots = k_Slots;
	for (auto i = static_cast<uint32_t>(slots.size()) - 1; i > 0; --i)
	{
		std::swap(slots.at(i), slots.at(random.NextBelow(i + 1)));
	}
	const auto count = std::min(buildings.size(), slots.size());
	for (size_t i = 0; i < count; ++i)
	{
		const auto [x, z] = PlaceOf(town, along, across, slots.at(i), random, settings);
		const auto rotation = static_cast<int32_t>(random.NextBelow(k_FullTurn));
		if (!IsFree(keepClear, x, z))
		{
			continue;
		}
		town.buildings.push_back(
		    {.building = buildings[i].building, .x = x, .z = z, .rotation = rotation, .planned = buildings[i].planned});
	}
}

/// Adds the adults of each house that stands: a housewife first, then a forester, then each kind in turn
void AddPeople(Town& town, Random& random, const Settings& settings)
{
	for (const auto& placed : town.buildings)
	{
		if (placed.planned || std::ranges::find(k_AllHouses, placed.building) == k_AllHouses.end())
		{
			continue;
		}
		const auto people = Between(random, settings.fewestPeople, settings.mostPeople);
		for (uint32_t i = 0; i < people; ++i)
		{
			const auto age = static_cast<int32_t>(
			    Between(random, static_cast<uint32_t>(settings.youngest), static_cast<uint32_t>(settings.oldest)));
			town.villagers.push_back({.kind = i % 2 == 0 ? Villager::Housewife : Villager::Forester,
			                          .x = placed.x + random.NextRange(-k_PersonShift, k_PersonShift),
			                          .z = placed.z + random.NextRange(-k_PersonShift, k_PersonShift),
			                          .age = age});
		}
	}
}

/// Adds the block of fields past the last row of buildings, with its middle row on the line of the town
void AddFields(Town& town, Axis along, Axis across, const Grid<uint8_t>& keepClear, const Settings& settings)
{
	const float fieldDistance = settings.buildingSpacing + settings.fieldGap + (k_FieldSide * 0.5f);
	const float firstX = OnFieldLine(town.x + (along.x * fieldDistance));
	const float firstZ = OnFieldLine(town.z + (along.z * fieldDistance));
	const int half = (settings.fieldColumns - 1) / 2;
	for (int row = 0; row < settings.fieldRows; ++row)
	{
		for (int column = 0; column < settings.fieldColumns; ++column)
		{
			const float stepAlong = static_cast<float>(row) * k_FieldSide;
			const float stepAcross = static_cast<float>(column - half) * k_FieldSide;
			const float x = firstX + (along.x * stepAlong) + (across.x * stepAcross);
			const float z = firstZ + (along.z * stepAlong) + (across.z * stepAcross);
			if (IsFree(keepClear, x, z))
			{
				town.fields.push_back({.x = x, .z = z});
			}
		}
	}
}

/// Adds a building at one of its two slots. `planned` is empty when the town does not have the building, but the slot,
/// the place and the turn always take their random numbers.
void AddExtra(Town& town, Building building, std::optional<bool> planned, const std::array<Slot, 2>& slots, Axis along,
              Axis across, const Grid<uint8_t>& keepClear, Random& random, const Settings& settings)
{
	const auto& slot = slots.at(random.NextBelow(static_cast<uint32_t>(slots.size())));
	const auto [x, z] = PlaceOf(town, along, across, slot, random, settings);
	const auto rotation = static_cast<int32_t>(random.NextBelow(k_FullTurn));
	if (planned.has_value() && IsFree(keepClear, x, z))
	{
		town.buildings.push_back({.building = building, .x = x, .z = z, .rotation = rotation, .planned = *planned});
	}
}

/// Adds the buildings, the people and the fields of the town of the start about its centre. The town reaches along the
/// direction: its fields are past the houses on that side.
void Lay(Town& town, Axis along, const Grid<uint8_t>& keepClear, Random& random, const Settings& settings)
{
	// Across the line, a quarter turn from it
	const Axis across {.x = -along.z, .z = along.x};
	AddCentre(town, random);

	// The storage pit and the creche, then the houses, each at a slot that the seed picks. The storage pit and the
	// creche use no random numbers, thus the count of houses comes first. The vector gets its full size before the
	// fill, because GCC 13 gives a false overflow warning when the vector grows.
	const auto houses = Between(random, settings.fewestHouses, settings.mostHouses);
	std::vector<Wanted> buildings(k_FixedBuildings + houses);
	buildings[0] = {.building = Building::StoragePit, .planned = false};
	buildings[1] = {.building = Building::Creche, .planned = false};
	for (uint32_t i = 0; i < houses; ++i)
	{
		buildings[k_FixedBuildings + i] = {.building = k_Houses.at(random.NextBelow(static_cast<uint32_t>(k_Houses.size()))),
		                                   .planned = false};
	}
	AddInSlots(town, buildings, along, across, keepClear, random, settings);
	AddPeople(town, random, settings);
	AddFields(town, along, across, keepClear, settings);

	// The workshop and the graveyard come last, so that they do not change the other buildings of a seed
	const std::array extras {
	    Extra {.building = Building::Workshop, .share = settings.workshopShare, .slots = k_WorkshopSlots},
	    Extra {.building = Building::Graveyard, .share = settings.graveyardShare, .slots = k_GraveyardSlots},
	};
	for (const auto& extra : extras)
	{
		const bool has = random.NextUnit() < extra.share;
		AddExtra(town, extra.building, has ? std::optional(false) : std::nullopt, extra.slots, along, across, keepClear, random,
		         settings);
	}
}

/// A number from 0 to below the count of the weights, picked with the weight of each number
size_t Weighted(Random& random, std::span<const uint32_t> weights)
{
	uint32_t total = 0;
	for (const auto weight : weights)
	{
		total += weight;
	}
	if (total == 0)
	{
		return 0;
	}
	auto pick = random.NextBelow(total);
	for (size_t i = 0; i < weights.size(); ++i)
	{
		if (pick < weights[i])
		{
			return i;
		}
		pick -= weights[i];
	}
	return weights.size() - 1;
}

/// Whether the town has a building that stands (false), a planned building (true), or no building (empty)
std::optional<bool> Draw(Random& random, Share share)
{
	const float pick = random.NextUnit();
	if (pick < share.built)
	{
		return false;
	}
	if (pick < share.built + share.planned)
	{
		return true;
	}
	return std::nullopt;
}

/// A building that a neutral town can have, with its share of the towns of the story and its two slots
struct MixExtra
{
	Building building;
	Share share;
	std::array<Slot, 2> slots;
};

/// Adds the buildings, the people and the fields of a neutral town with the mix of buildings of the towns of the story
void LayNeutral(Town& town, Axis along, const Grid<uint8_t>& keepClear, Random& random, const Settings& settings,
                const TownMix& mix)
{
	const Axis across {.x = -along.z, .z = along.x};
	AddCentre(town, random);

	// The town can have a storage pit and a creche. The vector gets its full size before the fill, because GCC 13
	// gives a false overflow warning when the vector grows.
	std::array<Wanted, k_FixedBuildings> fixed {};
	size_t fixedCount = 0;
	for (const auto& [building, share] :
	     std::array {std::pair {Building::StoragePit, mix.storagePit}, std::pair {Building::Creche, mix.creche}})
	{
		if (const auto planned = Draw(random, share); planned.has_value())
		{
			fixed.at(fixedCount) = {.building = building, .planned = *planned};
			++fixedCount;
		}
	}
	// The kinds of house of the tribe, with the weights of the towns of the story of the tribe
	const auto tribe = static_cast<size_t>(std::ranges::find(k_Tribes, town.tribe) - k_Tribes.begin());
	const auto& weights = mix.houseWeights.at(std::min(tribe, mix.houseWeights.size() - 1));
	const auto houses = Between(random, settings.fewestHouses, settings.mostHouses);
	std::vector<Wanted> buildings(fixedCount + houses);
	std::ranges::copy_n(fixed.begin(), static_cast<std::ptrdiff_t>(fixedCount), buildings.begin());
	for (uint32_t i = 0; i < houses; ++i)
	{
		buildings[fixedCount + i] = {.building = k_AllHouses.at(Weighted(random, weights)), .planned = false};
	}
	AddInSlots(town, buildings, along, across, keepClear, random, settings);
	AddPeople(town, random, settings);
	AddFields(town, along, across, keepClear, settings);

	const std::array extras {
	    MixExtra {.building = Building::Workshop, .share = mix.workshop, .slots = k_WorkshopSlots},
	    MixExtra {.building = Building::Graveyard, .share = mix.graveyard, .slots = k_GraveyardSlots},
	    MixExtra {.building = Building::Wonder,
	              .share = houses >= mix.largeTown ? mix.largeWonder : mix.wonder,
	              .slots = k_WonderSlots},
	};
	for (const auto& extra : extras)
	{
		const auto planned = Draw(random, extra.share);
		AddExtra(town, extra.building, planned, extra.slots, along, across, keepClear, random, settings);
	}
}

/// The tribe and the direction of the town of the start, which are the first numbers of the seed of the start
struct StartDraw
{
	Tribe tribe;
	Axis along;
};

StartDraw DrawStart(Random& random)
{
	const auto tribe = k_Tribes.at(random.NextBelow(static_cast<uint32_t>(k_Tribes.size())));
	const auto along = k_Axes.at(random.NextBelow(static_cast<uint32_t>(k_Axes.size())));
	return {.tribe = tribe, .along = along};
}

} // namespace

Site settlement::TownOfStart(float startX, float startZ, uint64_t seed, const Settings& settings)
{
	Random random(StageSeed(seed, Stage::StartTown));
	const auto start = DrawStart(random);
	const float distance = settings.townDistance / k_MetresPerCorner;
	return {.x = startX + (start.along.x * distance), .z = startZ + (start.along.z * distance)};
}

Plan settlement::Make(float startX, float startZ, const Grid<uint8_t>& keepClear, uint64_t seed, const Settings& settings)
{
	Random random(StageSeed(seed, Stage::StartTown));
	Plan plan;
	const auto start = DrawStart(random);
	plan.town.tribe = start.tribe;
	const auto along = start.along;
	const float middleX = startX * k_MetresPerCorner;
	const float middleZ = startZ * k_MetresPerCorner;

	plan.citadelX = middleX - (along.x * settings.citadelDistance);
	plan.citadelZ = middleZ - (along.z * settings.citadelDistance);
	plan.citadelRotation = static_cast<int32_t>(random.NextBelow(k_FullTurn));
	plan.town.x = middleX + (along.x * settings.townDistance);
	plan.town.z = middleZ + (along.z * settings.townDistance);
	Lay(plan.town, along, keepClear, random, settings);
	return plan;
}

Town settlement::MakeTown(Site site, Tribe tribe, const Grid<uint8_t>& keepClear, uint64_t seed, uint32_t number,
                          const Settings& settings, const TownMix& mix)
{
	Random random(PartSeed(StageSeed(seed, Stage::Towns), number));
	Town town;
	town.tribe = tribe;
	town.x = site.x * k_MetresPerCorner;
	town.z = site.z * k_MetresPerCorner;
	const auto along = k_Axes.at(random.NextBelow(static_cast<uint32_t>(k_Axes.size())));
	LayNeutral(town, along, keepClear, random, settings, mix);
	return town;
}

std::vector<Tribe> settlement::TribesOf(Tribe first, uint64_t seed, const SiteSettings& settings)
{
	Random random(StageSeed(seed, Stage::LandTribes));
	const auto count = settings.tribeCounts.at(random.NextBelow(static_cast<uint32_t>(settings.tribeCounts.size())));
	std::vector<Tribe> tribes {first};
	auto weights = settings.tribeWeights;
	while (tribes.size() < count)
	{
		// A tribe that the land has already takes no more turns
		for (size_t i = 0; i < k_Tribes.size(); ++i)
		{
			if (std::ranges::find(tribes, k_Tribes.at(i)) != tribes.end())
			{
				weights.at(i) = 0;
			}
		}
		if (std::ranges::all_of(weights, [](uint32_t weight) { return weight == 0; }))
		{
			break;
		}
		tribes.push_back(k_Tribes.at(Weighted(random, weights)));
	}
	return tribes;
}

std::vector<Tribe> settlement::TribesOfTowns(std::span<const Tribe> landTribes, size_t towns, uint64_t seed)
{
	std::vector<Tribe> result;
	if (landTribes.empty())
	{
		return result;
	}
	Random random(StageSeed(seed, Stage::TownTribes));
	for (size_t i = 1; i < landTribes.size() && result.size() < towns; ++i)
	{
		result.push_back(landTribes[i]);
	}
	while (result.size() < towns)
	{
		result.push_back(landTribes[random.NextBelow(static_cast<uint32_t>(landTribes.size()))]);
	}
	for (auto i = static_cast<uint32_t>(result.size()); i > 1; --i)
	{
		std::swap(result.at(i - 1), result.at(random.NextBelow(i)));
	}
	return result;
}

std::vector<Site> settlement::FindSites(const Grid<float>& heights, const HeightCurve& curve, const Grid<float>& seaDistance,
                                        const Grid<float>& riverDistance, Site first, std::span<const Area> keepOut,
                                        uint64_t seed, const SiteSettings& settings)
{
	return FindSites(heights, curve, seaDistance, riverDistance, std::span<const Site>(&first, 1), keepOut, seed, settings);
}

std::vector<Site> settlement::FindSites(const Grid<float>& heights, const HeightCurve& curve, const Grid<float>& seaDistance,
                                        const Grid<float>& riverDistance, std::span<const Site> firsts,
                                        std::span<const Area> keepOut, uint64_t seed, const SiteSettings& settings,
                                        const Grid<uint8_t>* lakes)
{
	const int side = heights.side;
	const auto reach = static_cast<int>(settings.blendRadius);
	const auto flat = static_cast<int>(settings.flatRadius);
	const int step = std::max(settings.step, 1);

	// Each place that meets the rules of a town centre, and its height in metres
	const float lowest = settings.heightTenths.front();
	const float highest = settings.heightTenths.back();
	std::vector<Site> candidates;
	std::vector<float> candidateHeights;
	for (int x = reach; x < side - reach; x += step)
	{
		for (int z = reach; z < side - reach; z += step)
		{
			const float metres = curve.MetresOf(heights.At(x, z));
			const float sea = seaDistance.At(x, z) * k_MetresPerCorner;
			if (metres < lowest || metres > highest || sea < settings.nearestSea || sea > settings.farthestSea ||
			    riverDistance.At(x, z) < settings.riverDistance)
			{
				continue;
			}
			const auto fx = static_cast<float>(x);
			const auto fz = static_cast<float>(z);
			if (std::ranges::any_of(keepOut, [&](const Area& area) {
				    return Distance(fx, fz, area.x, area.z) < area.radius + settings.blendRadius;
			    }))
			{
				continue;
			}
			float rough = 0.0f;
			for (int dx = -flat; dx <= flat; dx += 3)
			{
				for (int dz = -flat; dz <= flat; dz += 3)
				{
					if ((dx * dx) + (dz * dz) <= flat * flat)
					{
						rough = std::max(rough, std::fabs(curve.MetresOf(heights.At(x + dx, z + dz)) - metres));
					}
				}
			}
			if (rough > settings.roughest)
			{
				continue;
			}
			// The town gets the height that the generator gives to its flat ground: the mean height in the flat circle
			float sum = 0.0f;
			int count = 0;
			bool wet = false;
			for (int dx = -flat; dx <= flat; ++dx)
			{
				for (int dz = -flat; dz <= flat; ++dz)
				{
					if ((dx * dx) + (dz * dz) <= flat * flat)
					{
						sum += heights.At(x + dx, z + dz);
						++count;
						wet = wet || (lakes != nullptr && lakes->At(x + dx, z + dz) != 0);
					}
				}
			}
			if (wet)
			{
				continue;
			}
			const float level = curve.MetresOf(sum / static_cast<float>(count));
			if (level < lowest || level > highest)
			{
				continue;
			}
			candidates.push_back({.x = fx, .z = fz});
			candidateHeights.push_back(level);
		}
	}

	Random random(StageSeed(seed, Stage::TownSites));
	// The towns of the other players come in addition to the neutral towns
	const auto towns = std::min(settings.townCounts.at(random.NextBelow(static_cast<uint32_t>(settings.townCounts.size()))),
	                            settings.mostNeutral + 1) +
	                   (firsts.size() - 1);
	std::vector<Site> placed(firsts.begin(), firsts.end());
	constexpr int k_Tenths = 10;
	// The tenths in a random order. Each tenth comes once before a tenth comes again, so that the heights of the towns of
	// a land spread over all the tenths, as on a land of the story.
	std::array<int, k_Tenths> tenths {};
	size_t nextTenth = tenths.size();
	const float nearest = settings.nearestTown / k_MetresPerCorner;
	const float farthest = settings.farthestTown / k_MetresPerCorner;
	while (placed.size() < towns && !candidates.empty())
	{
		// A place in the reach of a town of the land, and not too near any town
		std::vector<size_t> usable;
		// When no place is in the reach, as for a start far from the sea, the nearest place that is not too near goes on
		// the chain
		std::optional<size_t> nearestPlace;
		float nearestPlaceDistance = std::numeric_limits<float>::max();
		std::vector<float> closests(candidates.size());
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			float closest = std::numeric_limits<float>::max();
			for (const auto& town : placed)
			{
				closest = std::min(closest, Distance(candidates[i].x, candidates[i].z, town.x, town.z));
			}
			closests[i] = closest;
			if (closest >= nearest && closest <= farthest)
			{
				usable.push_back(i);
			}
			if (closest >= nearest && closest < nearestPlaceDistance)
			{
				nearestPlace = i;
				nearestPlaceDistance = closest;
			}
		}
		if (usable.empty())
		{
			if (!nearestPlace.has_value())
			{
				break;
			}
			usable.push_back(*nearestPlace);
		}
		// The seed picks a tenth of the heights of the towns of the story. Without a place of that tenth in the reach, the
		// nearest place of that tenth goes on the chain, if it is not farther from the towns than the farthest town of the
		// story from its nearest town. Without such a place, the nearest tenth with a place in the reach gives it. The
		// higher tenth comes first, because the low ground near the sea is usually nearer than the high ground.
		if (nextTenth == tenths.size())
		{
			std::iota(tenths.begin(), tenths.end(), 0);
			for (auto i = static_cast<uint32_t>(tenths.size()) - 1; i > 0; --i)
			{
				std::swap(tenths.at(i), tenths.at(random.NextBelow(i + 1)));
			}
			nextTenth = 0;
		}
		const int tenth = tenths.at(nextTenth++);
		const auto inBand = [&](size_t i, int band) {
			return candidateHeights[i] >= settings.heightTenths.at(static_cast<size_t>(band)) &&
			       candidateHeights[i] <= settings.heightTenths.at(static_cast<size_t>(band) + 1);
		};
		std::vector<size_t> inTenth;
		std::ranges::copy_if(usable, std::back_inserter(inTenth), [&](size_t i) { return inBand(i, tenth); });
		if (inTenth.empty())
		{
			std::optional<size_t> outOfReach;
			float outOfReachDistance = settings.farthestNearest / k_MetresPerCorner;
			for (size_t i = 0; i < candidates.size(); ++i)
			{
				if (inBand(i, tenth) && closests[i] >= nearest && closests[i] <= outOfReachDistance)
				{
					outOfReach = i;
					outOfReachDistance = closests[i];
				}
			}
			if (outOfReach.has_value())
			{
				inTenth.push_back(*outOfReach);
			}
		}
		for (int away = 1; away < k_Tenths && inTenth.empty(); ++away)
		{
			for (const int band : {tenth + away, tenth - away})
			{
				if (band < 0 || band >= k_Tenths || !inTenth.empty())
				{
					continue;
				}
				std::ranges::copy_if(usable, std::back_inserter(inTenth), [&](size_t i) { return inBand(i, band); });
			}
		}
		const auto pick = inTenth.at(random.NextBelow(static_cast<uint32_t>(inTenth.size())));
		placed.push_back(candidates.at(pick));
		candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(pick));
		candidateHeights.erase(candidateHeights.begin() + static_cast<std::ptrdiff_t>(pick));
	}
	return {placed.begin() + static_cast<std::ptrdiff_t>(firsts.size()), placed.end()};
}

std::vector<Field> settlement::FishFarms(const Town& town, const Grid<float>& aboveSea, const Grid<float>& seaDistance,
                                         const Grid<uint8_t>& keepClear, std::span<const Field> taken, uint64_t seed,
                                         uint32_t number, const FarmSettings& settings)
{
	std::vector<Field> farms;
	const int side = aboveSea.side;
	if (side < 2)
	{
		return farms;
	}
	Random random(PartSeed(StageSeed(seed, Stage::FishFarms), number));
	// NOLINTBEGIN(bugprone-incorrect-roundings): A change gives a different land.
	const auto centreX = std::clamp(static_cast<int>((town.x / k_MetresPerCorner) + 0.5f), 0, side - 1);
	const auto centreZ = std::clamp(static_cast<int>((town.z / k_MetresPerCorner) + 0.5f), 0, side - 1);
	// NOLINTEND(bugprone-incorrect-roundings)
	const float sea = seaDistance.At(centreX, centreZ) * k_MetresPerCorner;
	const auto band = static_cast<size_t>(std::ranges::find_if(settings.seaBands, [sea](float upper) { return sea < upper; }) -
	                                      settings.seaBands.begin());
	const bool has = random.NextUnit() < settings.shares.at(band);
	const auto count = settings.fewest + static_cast<uint32_t>(Weighted(random, settings.countWeights));
	if (!has)
	{
		return farms;
	}

	// Each square of 10 metres in the reach of the town that is on low, flat and dry ground of the shore. The square
	// with the corner (x, z) has its middle 5 metres past the corner on each axis.
	// The steepest gradient of the ground, as the rise in metres for each metre
	const float steepest = std::tan(settings.steepest * std::numbers::pi_v<float> / 180.0f);
	const auto reach = static_cast<int>(std::ceil(settings.farthest / k_MetresPerCorner));
	std::vector<Field> squares;
	for (int x = std::max(centreX - reach, 0); x < std::min(centreX + reach, side - 1); ++x)
	{
		for (int z = std::max(centreZ - reach, 0); z < std::min(centreZ + reach, side - 1); ++z)
		{
			const Field square {.x = (static_cast<float>(x) * k_MetresPerCorner) + (k_FieldSide * 0.5f),
			                    .z = (static_cast<float>(z) * k_MetresPerCorner) + (k_FieldSide * 0.5f)};
			const float distance = Distance(square.x, square.z, town.x, town.z);
			if (distance < settings.nearest || distance > settings.farthest)
			{
				continue;
			}
			const std::array corners {aboveSea.At(x, z), aboveSea.At(x + 1, z), aboveSea.At(x, z + 1),
			                          aboveSea.At(x + 1, z + 1)};
			const float height = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
			// The gradient of the plane through the four corners, as the towns of the story measure it
			const float alongX = ((corners[1] + corners[3]) - (corners[0] + corners[2])) / (2.0f * k_MetresPerCorner);
			const float alongZ = ((corners[2] + corners[3]) - (corners[0] + corners[1])) / (2.0f * k_MetresPerCorner);
			const float rise = std::sqrt((alongX * alongX) + (alongZ * alongZ));
			const bool clear = keepClear.side == 0 || (keepClear.At(x, z) == 0 && keepClear.At(x + 1, z) == 0 &&
			                                           keepClear.At(x, z + 1) == 0 && keepClear.At(x + 1, z + 1) == 0);
			if (height < settings.lowest || height > settings.highest || rise > steepest || !clear)
			{
				continue;
			}
			const auto same = [&square](const Field& other) {
				return Distance(square.x, square.z, other.x, other.z) < k_FieldSide * 0.5f;
			};
			// The fields of the town and the fish farms of other towns keep their squares
			if (std::ranges::any_of(taken, same) || std::ranges::any_of(town.fields, same))
			{
				continue;
			}
			squares.push_back(square);
		}
	}

	// The chain starts at the square nearest the centre. Each next fish farm is the nearest square that touches a fish
	// farm of the chain, at a side or at a corner.
	const auto nearestOf = [&](const auto& usable) {
		std::optional<size_t> best;
		float bestDistance = std::numeric_limits<float>::max();
		for (size_t i = 0; i < squares.size(); ++i)
		{
			const float distance = Distance(squares[i].x, squares[i].z, town.x, town.z);
			if (usable(squares[i]) && distance < bestDistance)
			{
				best = i;
				bestDistance = distance;
			}
		}
		return best;
	};
	const float touch = (k_FieldSide * std::numbers::sqrt2_v<float>)+0.01f;
	auto next = nearestOf([](const Field&) { return true; });
	while (next.has_value() && farms.size() < count)
	{
		farms.push_back(squares[*next]);
		squares.erase(squares.begin() + static_cast<std::ptrdiff_t>(*next));
		next = nearestOf([&](const Field& square) {
			return std::ranges::any_of(
			    farms, [&](const Field& farm) { return Distance(square.x, square.z, farm.x, farm.z) <= touch; });
		});
	}
	// A town of the story with fish farms has two or more
	if (farms.size() < settings.fewest)
	{
		farms.clear();
	}
	return farms;
}
