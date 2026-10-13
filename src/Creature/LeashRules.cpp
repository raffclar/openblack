/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashRules.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_leash;
using creature_desires::Desire;

namespace
{
/// A creature of size 1 is fifteen units tall, which is what the hand's leash length goes by
constexpr float k_SizeToScale = 15.0f;
constexpr float k_HandSlackScale = 0.7f;
constexpr float k_HandSlackBase = 22.0f;
constexpr float k_HandMaxScale = 3.0f;
constexpr float k_HandMaxBase = 32.0f;
constexpr float k_TiedStaticReach = 1.5f;
constexpr float k_TiedStaticMin = 180.0f;
constexpr float k_TiedStaticMax = 360.0f;
constexpr float k_TiedMobileHeights = 7.0f;
constexpr float k_TiedMobileMax = 40.0f;
constexpr float k_SlackShare = 0.5f;
} // namespace

std::optional<size_t> creature_leash::IndexOf(LeashType type)
{
	const auto it = std::ranges::find(k_Types, type);
	if (it == k_Types.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(k_Types.begin(), it));
}

const char* creature_leash::Name(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return "Aggression";
	case LeashType::Rope:
		return "Learning";
	case LeashType::Good:
		return "Compassion";
	case LeashType::None:
		break;
	}
	return "None";
}

Lengths creature_leash::InHand(float creatureSize)
{
	const auto s = k_SizeToScale * creatureSize;
	return {.slack = (k_HandSlackScale * s) + k_HandSlackBase, .max = (k_HandMaxScale * s) + k_HandMaxBase};
}

Lengths creature_leash::TiedToStatic(float distance)
{
	const auto max = std::clamp(distance * k_TiedStaticReach, k_TiedStaticMin, k_TiedStaticMax);
	return {.slack = k_SlackShare * max, .max = max};
}

Lengths creature_leash::TiedToMobile(float creatureHeight)
{
	const auto max = std::min(creatureHeight * k_TiedMobileHeights, k_TiedMobileMax);
	return {.slack = k_SlackShare * max, .max = max};
}

leash_rope::Look creature_leash::LookFor(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return {.halfWidth = 0.15f, .v0 = 0.375f, .v1 = 0.5f, .uScale = 2.5f};
	case LeashType::Good:
		return {.halfWidth = 0.225f, .v0 = 0.25f, .v1 = 0.375f, .uScale = 2.5f};
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return {.halfWidth = 0.15f, .v0 = 0.125f, .v1 = 0.25f, .uScale = 2.5f};
}

std::optional<Desire> creature_leash::ForcedDesireFor(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return Desire::Anger;
	case LeashType::Good:
		return Desire::Compassion;
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return std::nullopt;
}

bool creature_leash::ImpressesTown(const TiedTown& town)
{
	if (town.leash == LeashType::Evil || town.impressing)
	{
		return false;
	}
	// A village of its player's that believes in them more than half as much as in anyone else is firmly theirs
	const bool firmlyOwned = town.ownersTown && (!town.creatureHasPlayer || town.beliefInOwner > 0.5f * town.mostOtherBelief);
	return !firmlyOwned;
}

uint32_t creature_leash::MiracleSightingWeight(bool learningLeash)
{
	return learningLeash ? 3 : 1;
}

bool creature_leash::FreeOfHome(float distanceFromHome, bool playerHasTemple)
{
	return playerHasTemple && distanceFromHome < k_HomeRange;
}

bool creature_leash::IsConfined(float radius, bool leashed, bool leashWorks)
{
	return radius > 0.0f && !(leashed && !leashWorks);
}

bool creature_leash::WalksBack(const Confinement& confinement)
{
	if (!IsConfined(confinement.radius, confinement.leashed, confinement.leashWorks) || confinement.orderInForce ||
	    confinement.mimicking || confinement.scriptControlled || confinement.teleporting ||
	    confinement.distance <= confinement.radius)
	{
		return false;
	}
	if (confinement.returnToCentre.has_value() &&
	    *confinement.returnToCentre <= std::max(confinement.radius * k_ReturnShare, k_ReturnLeast))
	{
		return false;
	}
	return confinement.centreOpen;
}

float creature_leash::ReturnExtraSpeed(float distance, float radius)
{
	const auto most = radius * 2.0f;
	if (most <= 0.0f)
	{
		return 0.0f;
	}
	return std::clamp(distance, 0.0f, most) * k_ReturnMostExtraSpeed / most;
}

float creature_leash::ReturnArrival(float radius, float height)
{
	return std::max(radius, height);
}

bool creature_leash::AttitudeDue(std::optional<uint32_t> lastTurn, uint32_t turn)
{
	return !lastTurn.has_value() || turn - *lastTurn > k_AttitudeTurns;
}

float creature_leash::AttitudeChange(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return -k_AttitudeStep;
	case LeashType::Good:
		return k_AttitudeStep;
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return 0.0f;
}

std::vector<Lesson> creature_leash::LessonsFor(LeashType type, bool objectIsCreature)
{
	const auto kind = objectIsCreature ? Desire::BeFriends : Desire::Compassion;
	switch (type)
	{
	case LeashType::Evil:
		return {{.desire = Desire::Anger, .change = 1.0f}, {.desire = kind, .change = -1.0f}};
	case LeashType::Good:
		return {{.desire = Desire::Anger, .change = -1.0f}, {.desire = kind, .change = 1.0f}};
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return {};
}
