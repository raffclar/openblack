/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerHidingRules.h"

using namespace openblack;
namespace villager_hiding = openblack::ecs::villager_hiding;

bool villager_hiding::CanBeHiddenIn(AbodeType type, bool functional)
{
	switch (type)
	{
	case AbodeType::Field:
	case AbodeType::FootballPitch:
	case AbodeType::Wonder:
	case AbodeType::TownCentre:
	case AbodeType::Graveyard:
		return false;
	default:
		return functional;
	}
}

std::optional<size_t> villager_hiding::NearestShelter(std::span<const Shelter> shelters)
{
	std::optional<size_t> nearest;
	float best = k_HideSearchDistance;
	for (size_t i = 0; i < shelters.size(); ++i)
	{
		if (shelters[i].canBeHiddenIn && shelters[i].distance < best)
		{
			best = shelters[i].distance;
			nearest = i;
		}
	}
	return nearest;
}

villager_hiding::HidingStep villager_hiding::HideStep(const HidingTurn& turn, const std::function<uint32_t(uint32_t)>& random)
{
	if (!turn.shelterStands)
	{
		return {.what = Hiding::GiveUp, .turnsInside = turn.turnsInside};
	}
	if (!turn.atDoor)
	{
		return {.what = Hiding::WalkToDoor, .turnsInside = turn.turnsInside};
	}
	if (turn.threatNear)
	{
		return {.what = Hiding::StayWhileThreatNear, .turnsInside = turn.turnsInside};
	}
	const uint32_t turns = turn.turnsInside + 1;
	if (turns > random(k_HideTurns) + k_HideTurns)
	{
		return {.what = Hiding::ComeOut, .turnsInside = 0};
	}
	return {.what = Hiding::Stay, .turnsInside = turns};
}

villager_hiding::LookAboutStep villager_hiding::LookAbout(uint32_t turns, const std::function<uint32_t(uint32_t)>& random)
{
	const uint32_t now = turns + 1;
	return {.safe = now > random(k_LookAboutTurns) + k_LookAboutTurns, .turns = now};
}
