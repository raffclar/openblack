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

float computer_mind_rules::KindScore(SpellKind kind, CreatureDesires perceived, float impressiveBalance, float attackValue)
{
	switch (kind)
	{
	case SpellKind::Aggressive:
		return perceived == CreatureDesires::Anger ? 1.0f : 0.0f;
	case SpellKind::Compassionate:
		return perceived == CreatureDesires::Compassion ? 1.0f : 0.0f;
	case SpellKind::Impressive:
		return impressiveBalance;
	case SpellKind::Attack:
		return attackValue;
	}
	return 0.0f;
}

float computer_mind_rules::KindWish(float wish, float score)
{
	return wish * score;
}

bool computer_mind_rules::TooNearOwnCreature(float distance, float ownSize, float targetSize)
{
	const float own = ownSize * k_CreatureReachPerSize;
	const float target = targetSize * k_CreatureReachPerSize;
	const float twice = (target + own) + (target + own);
	const float clearance = twice <= k_LeastCreatureClearance ? k_LeastCreatureClearance : twice;
	return distance < clearance;
}

std::optional<DamagedChoice> computer_mind_rules::MostDamaged(std::span<const float> shares)
{
	std::optional<DamagedChoice> choice;
	float least = 1.0f;
	for (size_t i = 0; i < shares.size(); ++i)
	{
		if (shares[i] < least)
		{
			least = shares[i];
			choice = DamagedChoice {.index = i, .share = least};
		}
	}
	return choice;
}

float computer_mind_rules::DamageNeed(float share)
{
	if (share < k_BadDamage)
	{
		return 1.0f;
	}
	return 1.0f - share * share;
}

std::optional<TownThreat> computer_mind_rules::ThreatOf(std::span<const TownAttack, k_TownAttackPlayers> attacks, uint32_t turn)
{
	TownThreat threat;
	float most = 0.0f;
	for (const auto& attack : attacks)
	{
		// The turns since the attack, as a number that is never negative
		const uint32_t since = turn - attack.turn;
		float forgotten = x87::Mul(static_cast<double>(since), k_AttackMemoryShare);
		if (!(forgotten < 1.0f))
		{
			forgotten = 1.0f;
		}
		const float fear = 1.0f - forgotten * forgotten * forgotten;
		if (most < fear)
		{
			threat.miracleHarm = attack.miracleHarm;
			threat.thingHarm = attack.thingHarm + attack.otherThingHarm;
			threat.fear = fear;
			most = fear;
		}
	}
	if (threat.miracleHarm <= 0.0f && threat.thingHarm <= 0.0f)
	{
		return std::nullopt;
	}
	return threat;
}

MagicType computer_mind_rules::ShieldAgainst(const TownThreat& threat)
{
	return threat.miracleHarm > threat.thingHarm ? MagicType::Shield : MagicType::PhysicalShield;
}

MagicType computer_mind_rules::ShieldToCheck(const TownThreat& threat)
{
	return threat.thingHarm > threat.miracleHarm ? MagicType::PhysicalShield : MagicType::Shield;
}

float computer_mind_rules::ImpressPlaceValue(uint32_t villagers, float distanceFromHand)
{
	const float nearness = gutils::GetDistanceModifier(distanceFromHand, k_ImpressReach);
	return x87::Mul(static_cast<double>(villagers), nearness);
}

std::optional<size_t> computer_mind_rules::BestImpressPlace(std::span<const float> values)
{
	if (values.empty())
	{
		return std::nullopt;
	}
	size_t best = 0;
	for (size_t i = 1; i < values.size(); ++i)
	{
		if (values[best] < values[i])
		{
			best = i;
		}
	}
	return best;
}

float computer_mind_rules::CarryDistance(float random, float targetRadius, float thingRadius)
{
	const float factor = x87::Add(random, k_LeastCarryFactor);
	return thingRadius + targetRadius * factor;
}

bool computer_mind_rules::SameCastPlace(const map_coords::MapCoords& a, const map_coords::MapCoords& b)
{
	const auto apart = [](int32_t from, int32_t to) {
		const auto difference = static_cast<int32_t>(static_cast<uint32_t>(from) - static_cast<uint32_t>(to));
		return difference < 0 ? -difference : difference;
	};
	return apart(a.x, b.x) < k_SameCastPlace && apart(a.z, b.z) < k_SameCastPlace;
}
