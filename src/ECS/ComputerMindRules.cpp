/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerMindRules.h"

#include <cstdint>

#include <optional>
#include <span>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::computer_mind_rules;

float computer_mind_rules::ChargeShare(const IconFacts& icon)
{
	float have = icon.store + k_SmallChants;
	if (icon.site)
	{
		const float available = icon.infinite ? k_InfiniteChants : icon.available - icon.used;
		have = available + icon.battery + have;
		if (have < icon.cost && icon.capacity == 0.0f)
		{
			return 0.0f;
		}
	}
	return have / (icon.cost + k_SmallChants);
}

float computer_mind_rules::TurnsToCharge(const IconFacts& icon)
{
	if (!icon.site)
	{
		return -1.0f;
	}
	const float available = icon.infinite ? k_InfiniteChants : icon.available;
	const float rest = icon.cost - icon.store - available;
	if (rest <= 0.0f)
	{
		return 0.0f;
	}
	if (icon.capacity == 0.0f)
	{
		return -1.0f;
	}
	return rest / icon.capacity;
}

float computer_mind_rules::RequestShare(uint32_t requests)
{
	const float share = static_cast<float>(static_cast<uint64_t>(requests)) * k_RequestShare;
	return share < 1.0f ? share : 1.0f;
}

float computer_mind_rules::SpellValue(float charge, float turnsToCharge, float placePull)
{
	const float soon = gutils::GetDistanceModifier(turnsToCharge, k_MostChargeTurns);
	const float value = placePull * soon * charge;
	return value < 1.0f ? value : 1.0f;
}

std::optional<size_t> computer_mind_rules::MostWanting(std::span<const float> desires)
{
	std::optional<size_t> best;
	float most = 0.0f;
	for (size_t i = 0; i < desires.size(); ++i)
	{
		if (desires[i] > most)
		{
			most = desires[i];
			best = i;
		}
	}
	return best;
}

std::optional<ChargeChoice> computer_mind_rules::ChooseCharge(std::span<const ChargeCandidate> icons)
{
	std::optional<ChargeChoice> best;
	float most = 0.0f;
	for (size_t i = 0; i < icons.size(); ++i)
	{
		const auto& icon = icons[i];
		if (icon.charging || icon.store != 0.0f || icon.requests == 0)
		{
			continue;
		}
		const float value = RequestShare(icon.requests);
		if (value > most)
		{
			most = value;
			best = ChargeChoice {.index = i, .value = value};
		}
	}
	return best;
}

float computer_mind_rules::TownWorshipNeed(uint32_t population, uint32_t worshipping, uint32_t onTheWay)
{
	if (population == 0)
	{
		return 0.0f;
	}
	float share = static_cast<float>(worshipping + onTheWay) / static_cast<float>(population);
	if (!(share < 1.0f))
	{
		share = 1.0f;
	}
	const float missing = 1.0f - share;
	const float need = (missing * missing) * missing;
	if (!(need < k_FullWorshipNeed))
	{
		return 1.0f;
	}
	return need < k_LeastWorshipNeed ? 0.0f : need;
}

float computer_mind_rules::SiteDancerNeed(uint32_t dancers, uint32_t icons)
{
	if (icons == 0)
	{
		return 0.0f;
	}
	float share = static_cast<float>(dancers) / (static_cast<float>(icons) * k_DancersPerIcon);
	if (!(share < 1.0f))
	{
		share = 1.0f;
	}
	return 1.0f - ((share * share) * share) * share;
}

float computer_mind_rules::FieldWish(float food, float totalFood, float distanceFromHand, float placePull)
{
	const float share = (food + k_SmallChants) / (totalFood + k_SmallChants);
	const float near = gutils::GetDistanceModifier(distanceFromHand, k_FieldReach) * share;
	return placePull * near;
}

std::optional<size_t> computer_mind_rules::MostWantedField(std::span<const float> wishes)
{
	std::optional<size_t> best;
	float most = k_LeastFieldWish;
	for (size_t i = 0; i < wishes.size(); ++i)
	{
		if (wishes[i] > most)
		{
			most = wishes[i];
			best = i;
		}
	}
	return best;
}

std::optional<map_coords::MapCoords> computer_mind_rules::NearestInfluenceEdge(const map_coords::MapCoords& place,
                                                                               std::span<const InfluenceCircle> circles)
{
	std::optional<map_coords::MapCoords> nearest;
	float best = k_FarFromInfluence;
	for (const auto& circle : circles)
	{
		const float distance = gutils::GetDistanceInMetres(circle.centre, place) - circle.radius;
		if (distance < best)
		{
			// The point on the circle in the direction of the place
			const float angle = gutils::Get3DAngleFromXZ(circle.centre, place);
			nearest = circle.centre + gutils::GetPosFromAngle(angle, circle.radius);
			best = distance;
		}
	}
	return nearest;
}

float computer_mind_rules::OutsidePlacePull(float distanceToEdge, float reach)
{
	const float pull = gutils::GetDistanceModifier(distanceToEdge, reach);
	return static_cast<double>(pull) >= k_LeastPlacePull ? pull * pull : 0.0f;
}

float computer_mind_rules::FishFarmWish(float distanceFromHand, float placePull)
{
	constexpr float k_FishFarmShare = 1.0f;
	const float near = gutils::GetDistanceModifier(distanceFromHand, k_FieldReach) * k_FishFarmShare;
	return placePull * near;
}

int32_t computer_mind_rules::FoodShortfall(float wanted, uint32_t held)
{
	// The whole number of food held is exact in the subtraction, which rounds one time
	return map_coords::FtoL(x87::Sub(wanted, static_cast<double>(held)));
}

float computer_mind_rules::OtherTownAsk(float need)
{
	return need * k_AskPerNeed;
}

std::optional<GivingChoice> computer_mind_rules::ChooseGivingTown(std::span<const GivingTown> towns, float need)
{
	const float ask = OtherTownAsk(need);
	if (!(ask > k_LeastAsk))
	{
		return std::nullopt;
	}
	const auto least = static_cast<uint32_t>(map_coords::FtoL(ask));
	std::optional<GivingChoice> best;
	float most = 0.0f;
	for (size_t i = 0; i < towns.size(); ++i)
	{
		const auto& town = towns[i];
		if (town.stock <= least)
		{
			continue;
		}
		const float desire = town.rawDesire < 1.0f ? town.rawDesire : 1.0f;
		const float keep = town.pull * desire;
		if (!(keep < need))
		{
			continue;
		}
		const float give = 1.0f - keep;
		if (give > most)
		{
			most = give;
			best = GivingChoice {.index = i, .value = give};
		}
	}
	return best;
}

int32_t computer_mind_rules::ContainerTake(int32_t wanted, std::optional<uint32_t> inPot, uint32_t inContainer)
{
	auto take = wanted;
	if (inPot.has_value())
	{
		take -= static_cast<int32_t>(*inPot);
	}
	const auto have = static_cast<int32_t>(inContainer);
	return take < have ? take : have;
}
