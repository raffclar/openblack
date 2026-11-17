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

#include <algorithm>
#include <array>
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
} // namespace

Plan settlement::Make(float startX, float startZ, const Grid<uint8_t>& keepClear, uint64_t seed, const Settings& settings)
{
	Random random(StageSeed(seed, 50));
	Plan plan;
	plan.tribe = k_Tribes.at(random.NextBelow(static_cast<uint32_t>(k_Tribes.size())));
	const auto along = k_Axes.at(random.NextBelow(static_cast<uint32_t>(k_Axes.size())));
	// Across the line, a quarter turn from it
	const Axis across {.x = -along.z, .z = along.x};
	const float middleX = startX * k_MetresPerCorner;
	const float middleZ = startZ * k_MetresPerCorner;

	plan.citadelX = middleX - (along.x * settings.citadelDistance);
	plan.citadelZ = middleZ - (along.z * settings.citadelDistance);
	plan.citadelRotation = static_cast<int32_t>(random.NextBelow(k_FullTurn));
	plan.townX = middleX + (along.x * settings.townDistance);
	plan.townZ = middleZ + (along.z * settings.townDistance);
	plan.buildings.push_back({.building = Building::TownCentre,
	                          .x = plan.townX,
	                          .z = plan.townZ,
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
		const auto& slot = slots.at(i);
		const float stepAcross = static_cast<float>(slot.across) * settings.buildingSpacing;
		const float stepAlong = static_cast<float>(slot.along) * settings.buildingSpacing;
		const float x = plan.townX + (across.x * stepAcross) + (along.x * stepAlong) +
		                random.NextRange(-settings.buildingShift, settings.buildingShift);
		const float z = plan.townZ + (across.z * stepAcross) + (along.z * stepAlong) +
		                random.NextRange(-settings.buildingShift, settings.buildingShift);
		const auto rotation = static_cast<int32_t>(random.NextBelow(k_FullTurn));
		if (!IsFree(keepClear, x, z))
		{
			continue;
		}
		plan.buildings.push_back({.building = buildings.at(i), .x = x, .z = z, .rotation = rotation});
	}

	// The adults of each house: a housewife first, then a forester, then each kind in turn
	for (const auto& placed : plan.buildings)
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
			plan.villagers.push_back({.kind = i % 2 == 0 ? Villager::Housewife : Villager::Forester,
			                          .x = placed.x + random.NextRange(-k_PersonShift, k_PersonShift),
			                          .z = placed.z + random.NextRange(-k_PersonShift, k_PersonShift),
			                          .age = age});
		}
	}

	// The block of fields past the last row of buildings, with its middle row on the line of the start
	const float fieldDistance = settings.buildingSpacing + settings.fieldGap + (k_FieldSide * 0.5f);
	const float firstX = OnFieldLine(plan.townX + (along.x * fieldDistance));
	const float firstZ = OnFieldLine(plan.townZ + (along.z * fieldDistance));
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
				plan.fields.push_back({.x = x, .z = z});
			}
		}
	}
	return plan;
}
