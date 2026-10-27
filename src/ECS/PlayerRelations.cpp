/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerRelations.h"

#include <cstdint>
#include <cstdlib>

#include <bit>

#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

namespace openblack::player_relations
{

namespace
{

/// The small amount that the game adds to each power before a division, as a double
constexpr double k_PowerFloor = std::bit_cast<double>(uint64_t {0x3F1A36E2EB1C432D}); // 0.0001
/// The small amount that the game adds to each harm before a division, as a double
constexpr double k_HarmFloor = std::bit_cast<double>(uint64_t {0x3F50624DD2F1A9FC}); // 0.001
/// The strength of an alliance above which an ally lends its influence, as a double
constexpr double k_LendStrength = std::bit_cast<double>(uint64_t {0x3FB999999999999A}); // 0.1
/// The distance over which the nearness of two sides falls to nothing
constexpr float k_AttackReach = std::bit_cast<float>(uint32_t {0x45A00000}); // 5120
/// The distance to an abode that counts, and the part of one metre in it
constexpr float k_AbodeReach = std::bit_cast<float>(uint32_t {0x42C80000}); // 100
constexpr float k_AbodeShare = std::bit_cast<float>(uint32_t {0x3C23D70A}); // 0.01

/// The cube of a value, in the order that the game multiplies it
float Cube(float value)
{
	const float square = value * value;
	return square * value;
}

} // namespace

float KeptAttitude(float given)
{
	return given + given;
}

float ReadAttitude(float kept)
{
	return kept * 0.5f;
}

bool IsAlly(float strength)
{
	return strength > 0.0f;
}

bool LendsInfluence(float strength)
{
	return x87::Greater(strength, k_LendStrength);
}

float NextAverageInfluence(float average, float power)
{
	return (average + power) * 0.5f;
}

float NearestAbode(const map_coords::MapCoords& hand, std::span<const map_coords::MapCoords> abodes)
{
	float nearest = k_AbodeReach;
	for (const auto& abode : abodes)
	{
		const float distance = gutils::GetDistanceInMetres(hand, abode);
		if (distance < nearest)
		{
			nearest = distance;
		}
	}
	return nearest;
}

float DesireToAttack(const AttackSide& god, const AttackSide& rival, float nearestAbode)
{
	// The gap between the edges of the two influences
	float gap = gutils::GetDistanceInMetres(god.place, rival.place) - (god.power + rival.power);
	if (!(gap > 0.0f))
	{
		gap = 0.0f;
	}
	const float nearness = gutils::GetDistanceModifier(gap, k_AttackReach);

	const float rivalPower = x87::Add(rival.power, k_PowerFloor);
	const float godPower = x87::Add(god.power, k_PowerFloor);
	const float rivalAverage = x87::Add(rival.averagePower, k_PowerFloor);
	const float godAverage = x87::Add(god.averagePower, k_PowerFloor);
	float growth = rivalPower / rivalAverage - godPower / godAverage;
	if (!(growth > 0.0f))
	{
		growth = 0.0f;
	}

	const float ratio = rivalPower / godPower;
	float stronger = ratio - 1.0f;
	if (!(stronger > 0.0f))
	{
		stronger = 0.0f;
	}
	const float strength = nearness * stronger;

	const float weakness = Cube(1.0f - ratio);
	const float weak = weakness >= 0.0f ? weakness : 0.0f;

	const float harmRatio = x87::Add(rival.harmFromOther, k_HarmFloor) / x87::Add(god.harmFromOther, k_HarmFloor);
	const float revengeValue = 1.0f - harmRatio;
	const float revenge = revengeValue < 0.0f ? 0.0f : revengeValue;

	// The difference of alignment counts in whole quarters of a whole number, so it is 0 for alignments from -1 to 1
	const auto difference = std::abs(static_cast<int32_t>(god.alignment - rival.alignment));
	const auto alignment = static_cast<float>(difference / 4);

	const float proximity = 1.0f - nearestAbode * k_AbodeShare;

	return (((((proximity + alignment) + revenge) + weak) + strength) + growth) * nearness;
}

std::optional<size_t> ChooseRival(std::span<const float> values)
{
	std::optional<size_t> chosen;
	float best = 0.0f;
	for (size_t index = 0; index < values.size(); ++index)
	{
		if (values[index] > best || !chosen.has_value())
		{
			best = values[index];
			chosen = index;
		}
	}
	return chosen;
}

float DefeatIntensity(float value)
{
	return value >= 1.0f ? 1.0f : value;
}

} // namespace openblack::player_relations
