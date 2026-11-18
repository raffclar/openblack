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
#include <optional>
#include <utility>

#include "landgen/Random.h"

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

/// Whether a place in metres is free of what the land keeps clear
bool IsFree(const Grid<uint8_t>& keepClear, float x, float z)
{
	const int side = keepClear.side;
	if (side == 0)
	{
		return true;
	}
	const auto cx = std::clamp(static_cast<int>((x / k_MetresPerCorner) + 0.5f), 0, side - 1);
	const auto cz = std::clamp(static_cast<int>((z / k_MetresPerCorner) + 0.5f), 0, side - 1);
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

/// Adds the buildings, the people and the fields of a town about its centre. The town reaches along the direction: its
/// fields are past the houses on that side.
void Lay(Town& town, Axis along, const Grid<uint8_t>& keepClear, Random& random, const Settings& settings)
{
	// Across the line, a quarter turn from it
	const Axis across {.x = -along.z, .z = along.x};
	town.buildings.push_back({.building = Building::TownCentre,
	                          .x = town.x,
	                          .z = town.z,
	                          .rotation = static_cast<int32_t>(random.NextBelow(k_FullTurn))});

	// The storage pit and the creche, then the houses, each at a slot that the seed picks
	std::vector<Building> buildings {Building::StoragePit, Building::Creche};
	const auto houses = Between(random, settings.fewestHouses, settings.mostHouses);
	for (uint32_t i = 0; i < houses; ++i)
	{
		buildings.push_back(k_Houses.at(random.NextBelow(static_cast<uint32_t>(k_Houses.size()))));
	}
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
		town.buildings.push_back({.building = buildings.at(i), .x = x, .z = z, .rotation = rotation});
	}

	// The adults of each house: a housewife first, then a forester, then each kind in turn
	for (const auto& placed : town.buildings)
	{
		if (std::ranges::find(k_Houses, placed.building) == k_Houses.end())
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

	// The block of fields past the last row of buildings, with its middle row on the line of the town
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

	// The workshop and the graveyard come last, so that they do not change the other buildings of a seed
	const std::array extras {
	    Extra {.building = Building::Workshop, .share = settings.workshopShare, .slots = k_WorkshopSlots},
	    Extra {.building = Building::Graveyard, .share = settings.graveyardShare, .slots = k_GraveyardSlots},
	};
	for (const auto& extra : extras)
	{
		const bool has = random.NextUnit() < extra.share;
		const auto& slot = extra.slots.at(random.NextBelow(static_cast<uint32_t>(extra.slots.size())));
		const auto [x, z] = PlaceOf(town, along, across, slot, random, settings);
		const auto rotation = static_cast<int32_t>(random.NextBelow(k_FullTurn));
		if (has && IsFree(keepClear, x, z))
		{
			town.buildings.push_back({.building = extra.building, .x = x, .z = z, .rotation = rotation});
		}
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
} // namespace

Site settlement::TownOfStart(float startX, float startZ, uint64_t seed, const Settings& settings)
{
	Random random(StageSeed(seed, 50));
	const auto start = DrawStart(random);
	const float distance = settings.townDistance / k_MetresPerCorner;
	return {.x = startX + (start.along.x * distance), .z = startZ + (start.along.z * distance)};
}

Plan settlement::Make(float startX, float startZ, const Grid<uint8_t>& keepClear, uint64_t seed, const Settings& settings)
{
	Random random(StageSeed(seed, 50));
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
                          const Settings& settings)
{
	Random random(StageSeed(StageSeed(seed, 54), number));
	Town town;
	town.tribe = tribe;
	town.x = site.x * k_MetresPerCorner;
	town.z = site.z * k_MetresPerCorner;
	const auto along = k_Axes.at(random.NextBelow(static_cast<uint32_t>(k_Axes.size())));
	Lay(town, along, keepClear, random, settings);
	return town;
}

std::vector<Tribe> settlement::TribesOf(Tribe first, uint64_t seed, const SiteSettings& settings)
{
	Random random(StageSeed(seed, 52));
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
	Random random(StageSeed(seed, 53));
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
			if (rough <= settings.roughest)
			{
				candidates.push_back({.x = fx, .z = fz});
				candidateHeights.push_back(metres);
			}
		}
	}

	Random random(StageSeed(seed, 51));
	const auto towns = std::min(settings.townCounts.at(random.NextBelow(static_cast<uint32_t>(settings.townCounts.size()))),
	                            settings.mostNeutral + 1);
	std::vector<Site> placed {first};
	constexpr int k_Tenths = 10;
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
		for (size_t i = 0; i < candidates.size(); ++i)
		{
			float closest = std::numeric_limits<float>::max();
			for (const auto& town : placed)
			{
				closest = std::min(closest, Distance(candidates[i].x, candidates[i].z, town.x, town.z));
			}
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
		// The seed picks a tenth of the heights of the towns of the story. Without a place in that tenth, the nearest
		// tenth with a place gives it.
		const auto tenth = static_cast<int>(random.NextBelow(k_Tenths));
		std::vector<size_t> inTenth;
		for (int away = 0; away < k_Tenths && inTenth.empty(); ++away)
		{
			for (const int band : {tenth - away, tenth + away})
			{
				if (band < 0 || band >= k_Tenths || !inTenth.empty())
				{
					continue;
				}
				const float low = settings.heightTenths.at(static_cast<size_t>(band));
				const float high = settings.heightTenths.at(static_cast<size_t>(band) + 1);
				std::ranges::copy_if(usable, std::back_inserter(inTenth),
				                     [&](size_t i) { return candidateHeights[i] >= low && candidateHeights[i] <= high; });
			}
		}
		const auto pick = inTenth.at(random.NextBelow(static_cast<uint32_t>(inTenth.size())));
		placed.push_back(candidates.at(pick));
		candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(pick));
		candidateHeights.erase(candidateHeights.begin() + static_cast<std::ptrdiff_t>(pick));
	}
	return {placed.begin() + 1, placed.end()};
}
