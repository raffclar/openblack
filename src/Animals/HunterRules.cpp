/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HunterRules.h"

#include "3D/MapCoords.h"

using namespace openblack;
namespace hunters = openblack::animals::hunters;

namespace
{
/// The big cats sleep after this hour of the day
constexpr float k_CatsSleepAfter = 22.0f;
/// A wolf pack leaves its lair after this hour of the day
constexpr float k_WolvesHuntAfter = 23.0f;
/// Of each 100 turns, a hunter at its hunting distance creeps for this many
constexpr uint32_t k_StalkTurns = 33;
constexpr uint32_t k_StalkCycle = 100;
/// A flock with its home higher than this above the land is in the air, and a wolf pack does not go to it
constexpr float k_HighestFlock = 2.0f;
/// A wolf pack measures the distance to a flock in hundreds of map units
constexpr uint32_t k_PackDistanceUnit = 100;
} // namespace

bool hunters::IsWildHunter(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
	case AnimalInfo::Tiger:
	case AnimalInfo::Wolf:
	case AnimalInfo::Leopard:
		return true;
	default:
		return false;
	}
}

std::optional<hunters::KindClips> hunters::ClipsOf(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
		return KindClips {.stand = AnimId::ALionStand,
		                  .eat = AnimId::ALionEat,
		                  .startToEat = AnimId::ALionGotoEat,
		                  .upFromEat = AnimId::ALionUpfromEat,
		                  .sleep = AnimId::ALionSleep,
		                  .pounce = AnimId::ALionPounceHi,
		                  .prowl = AnimId::ALionProwl,
		                  .walk = AnimId::ALionWalk,
		                  .run = AnimId::ALionRun};
	case AnimalInfo::Tiger:
		return KindClips {.stand = AnimId::ATigerStand,
		                  .eat = AnimId::ATigerEat,
		                  .startToEat = AnimId::ATigerGotoEat,
		                  .upFromEat = AnimId::ATigerUpfromEat,
		                  .sleep = AnimId::ATigerSleep,
		                  .pounce = AnimId::ATigerPounceHi,
		                  .prowl = AnimId::ATigerProwl,
		                  .walk = AnimId::ATigerWalk,
		                  .run = AnimId::ATigerRun};
	case AnimalInfo::Leopard:
		return KindClips {.stand = AnimId::ALeopardStand,
		                  .eat = AnimId::ALeopardEat,
		                  .startToEat = AnimId::ALeopardGotoEat,
		                  .upFromEat = AnimId::ALeopardUpfromEat,
		                  .sleep = AnimId::ALeopardSleep,
		                  .pounce = AnimId::ALeopardPounceHi,
		                  .prowl = AnimId::ALeopardProwl,
		                  .walk = AnimId::ALeopardWalk,
		                  .run = AnimId::ALeopardRun};
	case AnimalInfo::Wolf:
		return KindClips {.stand = AnimId::AWolfStand,
		                  .eat = AnimId::AWolfEat,
		                  .startToEat = AnimId::AWolfGotoEat,
		                  .upFromEat = AnimId::AWolfUpfromEat,
		                  .sleep = AnimId::AWolfSleep,
		                  .pounce = AnimId::AWolfPounce,
		                  .prowl = AnimId::AWolfProwl,
		                  .walk = AnimId::AWolfWalk,
		                  .run = AnimId::AWolfRun};
	default:
		return std::nullopt;
	}
}

SpeedThreshold hunters::ThresholdRowOf(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Tiger:
		return SpeedThreshold::AnimalTiger;
	case AnimalInfo::Leopard:
		return SpeedThreshold::AnimalLeopard;
	case AnimalInfo::Wolf:
		return SpeedThreshold::AnimalWolf;
	case AnimalInfo::Lion:
	default:
		return SpeedThreshold::AnimalLion;
	}
}

AnimId hunters::MoveClip(const KindClips& clips, uint16_t speed, uint16_t usualSpeed, uint16_t runAbove)
{
	if (speed < usualSpeed)
	{
		return clips.prowl;
	}
	return speed > runAbove ? clips.run : clips.walk;
}

std::optional<size_t> hunters::ChaseSpeed(float distance, float attackDistance, float stalkingDistance, float huntingDistance,
                                          uint32_t turn)
{
	if (distance < attackDistance)
	{
		return k_SprintSpeed;
	}
	if (distance < stalkingDistance)
	{
		return k_StalkSpeed;
	}
	if (distance < huntingDistance)
	{
		return turn % k_StalkCycle < k_StalkTurns ? k_StalkSpeed : k_HuntSpeed;
	}
	return std::nullopt;
}

bool hunters::CatsSleepAt(float visualHours)
{
	return visualHours > k_CatsSleepAfter;
}

bool hunters::WolvesHuntAt(float visualHours)
{
	return visualHours > k_WolvesHuntAfter;
}

hunters::Need hunters::FirstNeed(grazers::Needs& needs, const grazers::NeedLimits& limits,
                                 std::optional<grazers::HerdSize> herd)
{
	if (limits.breed != 0 && static_cast<int32_t>(limits.breed) <= needs.breed)
	{
		if (herd.has_value() && herd->members < herd->most)
		{
			return Need::Breed;
		}
		needs.breed = 0;
	}
	if (limits.hunger != 0 && static_cast<int32_t>(limits.hunger) <= needs.hunger)
	{
		return Need::Hunger;
	}
	if (limits.sleep != 0 && static_cast<int32_t>(limits.sleep) <= needs.sleep)
	{
		return Need::Sleep;
	}
	return Need::None;
}

std::optional<size_t> hunters::PackGoal(std::span<const FlockSeen> flocks, float ownStrength, uint16_t reach)
{
	std::optional<size_t> best;
	auto nearest = static_cast<uint32_t>(reach) + 1;
	for (size_t i = 0; i < flocks.size(); ++i)
	{
		const auto& flock = flocks[i];
		if (!(flock.leaderStrength <= ownStrength) || !(flock.height <= k_HighestFlock))
		{
			continue;
		}
		// The distance is cut to whole hundreds, as an unsigned number
		const auto distance = static_cast<uint32_t>(flock.distance) / k_PackDistanceUnit;
		if (distance <= reach && distance < nearest)
		{
			nearest = distance;
			best = i;
		}
	}
	return best;
}

glm::ivec2 hunters::RandomPointInSquare(glm::ivec2 centre, float size, const std::function<float(float)>& random)
{
	const float half = size * 0.5f;
	const float dx = half - random(size);
	const float dz = half - random(size);
	const auto place = [](int32_t from, float offset) {
		const float metres = static_cast<float>(from) * 10.0f * (1.0f / 65536.0f) + offset;
		return map_coords::FtoL(metres * 65536.0f / 10.0f);
	};
	return {place(centre.x, dx), place(centre.y, dz)};
}
