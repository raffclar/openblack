/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PredatorRules.h"

#include <algorithm>

using namespace openblack;
namespace predators = openblack::animals::predators;

namespace
{
/// A speed state is map units a turn: ten metres a second for every 65536 / 100 of them
constexpr double k_SpeedStateScale = 10.0 / 65536.0;
constexpr double k_SpeedStateMetres = 10.0;

uint8_t AsUrgency(uint32_t value)
{
	// The urgency is the low byte of the whole number
	return static_cast<uint8_t>(value & 0xFFu);
}
} // namespace

bool predators::IsPredator(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
	case AnimalInfo::Tiger:
	case AnimalInfo::Wolf:
	case AnimalInfo::Leopard:
	case AnimalInfo::SpellWolf:
		return true;
	default:
		return false;
	}
}

float predators::SpeedMetresPerSecond(uint16_t speedState)
{
	return static_cast<float>(k_SpeedStateScale * static_cast<double>(speedState) * k_SpeedStateMetres);
}

uint8_t predators::VillagerPriority(const VillagerSees& sees)
{
	if (!sees.animal)
	{
		return 0;
	}
	const bool far = sees.distance > sees.leastRunDistance;
	// Inside its home, hiding, or on its way to hide with the hunter far, it takes no notice
	if (sees.atHome || sees.hiding || (sees.goingToHide && far))
	{
		return 0;
	}
	// Looking about long enough outside where it hid, any hunter frightens it again; otherwise a resting hunter doesn't,
	// nor a far one it has fled before that isn't coming at it
	const bool lookingLong = sees.lookingAbout && sees.turnsLookingAbout >= k_LookingAboutTurnsBeforeFrightened;
	if (!lookingLong && (sees.hunterResting || (far && sees.frightenedBefore && !sees.comingAt)))
	{
		return 0;
	}
	const double factor =
	    static_cast<double>(sees.speed) / static_cast<double>(std::max<uint32_t>(sees.kindSpeed3, 1)) * 0.5 + 0.5;
	return AsUrgency(static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(sees.tablePriority) * factor)));
}

uint8_t predators::HunterPriority(bool thingLiving, float thingStrength, float ownStrength, uint32_t tablePriority)
{
	if (thingLiving && thingStrength <= ownStrength)
	{
		return 0;
	}
	return AsUrgency(tablePriority);
}

uint8_t predators::OtherPriority(bool thingAnimal, uint16_t thingSpeed, uint32_t thingKindSpeed2, uint32_t tablePriority)
{
	if (thingAnimal && static_cast<int32_t>(thingSpeed) <= static_cast<int32_t>(thingKindSpeed2))
	{
		return 0;
	}
	return AsUrgency(tablePriority);
}

predators::VillagerFlees predators::VillagerFleeStep(const VillagerFleeTurn& turn,
                                                     const std::function<uint32_t(uint32_t)>& random)
{
	if (!turn.hunterThere)
	{
		return VillagerFlees::Stop;
	}
	if (!turn.animal)
	{
		return VillagerFlees::Nothing;
	}
	if (turn.distance > turn.furthestRunDistance)
	{
		return VillagerFlees::Stop;
	}
	if ((turn.fast && turn.comingAt) || random(k_HideChance) == 0)
	{
		return VillagerFlees::TryToHide;
	}
	return VillagerFlees::RunOrWatch;
}

bool predators::WatchesRatherThanRuns(float distance, float leastRunDistance, bool comingAt)
{
	return distance > leastRunDistance && !comingAt;
}

bool predators::StillFrightensLookout(bool hunterThere, float distance, float reach, bool animal, bool hunterResting)
{
	return hunterThere && distance <= reach && animal && !hunterResting;
}

predators::AnimalFlees predators::AnimalFleeStep(bool hasHunter, bool hunterThere, float distance, float furthestRunDistance)
{
	if (!hasHunter)
	{
		return AnimalFlees::Nothing;
	}
	if (!hunterThere || distance > furthestRunDistance)
	{
		return AnimalFlees::Stop;
	}
	return AnimalFlees::Run;
}
