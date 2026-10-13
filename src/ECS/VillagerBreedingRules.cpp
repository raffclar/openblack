/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerBreedingRules.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/GUtilsAngle.h"
#include "ECS/VillagerRoutine.h"

using namespace openblack;
namespace villager_breeding = openblack::ecs::villager_breeding;
namespace routine = openblack::ecs::villager_routine;

bool villager_breeding::IsSexuallyActive(uint32_t age, uint32_t startAge, uint32_t stopAge)
{
	return startAge <= age && age < stopAge;
}

bool villager_breeding::IsPregnant(bool female, int16_t pregnancy)
{
	return female && pregnancy != 0;
}

bool villager_breeding::IsWoman(bool female, bool child)
{
	return female && !child;
}

bool villager_breeding::ConceivesAtHome(const HomeConception& facts)
{
	return !facts.pregnant && facts.hasTown && facts.hasAbode && facts.sexuallyActive && facts.childrenWanted > 0.0f &&
	       facts.children + facts.pregnantWomen < facts.childRoom;
}

bool villager_breeding::BedTimeMayConceive(bool hasTown, float townSleep, bool sexuallyActive)
{
	return hasTown && townSleep >= 1.0f && sexuallyActive;
}

std::vector<size_t> villager_breeding::BedTimeConceptions(bool bedderFemale, size_t bedder, std::span<const Housemate> home)
{
	std::vector<size_t> chances;
	if (bedderFemale)
	{
		// Herself, once, for the first male she finds in
		if (std::ranges::any_of(home, [](const Housemate& mate) { return mate.male && mate.atHome; }))
		{
			chances.push_back(bedder);
		}
		return chances;
	}
	for (size_t i = 0; i < home.size(); ++i)
	{
		if (!home[i].male && home[i].atHome)
		{
			chances.push_back(i);
		}
	}
	return chances;
}

villager_breeding::Countdown villager_breeding::CountDown(int16_t pregnancy, uint32_t turnsGone)
{
	const auto left = static_cast<int16_t>(pregnancy - static_cast<int16_t>(turnsGone));
	return {.pregnancy = left, .birthDue = left < 1};
}

uint16_t villager_breeding::BirthTurns(float turnsPerDay, const IntRandom& random)
{
	const auto extra = static_cast<float>(random(static_cast<uint32_t>(turnsPerDay)));
	return static_cast<uint16_t>(static_cast<int32_t>(extra + turnsPerDay * k_BirthLeastShare + 1.0f));
}

std::optional<VillagerNumber> villager_breeding::NewbornJob(uint32_t boyChance, const IntRandom& random)
{
	const auto draw = random(k_NewbornDraw);
	if (static_cast<float>(draw) > static_cast<float>(boyChance))
	{
		return std::nullopt;
	}
	return static_cast<VillagerNumber>(random(k_NewbornJobs) + 1);
}

uint16_t villager_breeding::LoveTurns(uint32_t mateTime, uint32_t ownTime)
{
	return static_cast<uint16_t>((mateTime + ownTime + ownTime) / 2 + 1);
}

glm::vec2 villager_breeding::MeetingPoint(glm::vec2 from, glm::vec2 to, float gap)
{
	const float distance = glm::distance(from, to);
	if (distance == 0.0f)
	{
		return from;
	}
	return routine::Polar(from, gutils::Get3DAngleFromXZ(from, to), (distance - gap) * 0.5f);
}

bool villager_breeding::Touching(float centreDistance, float radiusA, float radiusB, float gap)
{
	return centreDistance - (radiusA + radiusB) <= gap;
}

bool villager_breeding::LeavesLove(bool nextIsLoveState, bool nextIsFinal)
{
	return !nextIsLoveState && nextIsFinal;
}

bool villager_breeding::BreederMayBreed(const TownHeadcount& town, float overAllowance)
{
	const auto over = [](uint32_t count, uint32_t places) {
		return static_cast<float>(static_cast<int32_t>(count) - static_cast<int32_t>(places));
	};
	return over(town.children, town.childPlaces) < overAllowance &&
	       over(town.adults, town.adultPlaces) < overAllowance * k_BreederAdultShare;
}

glm::vec2 villager_breeding::BreederSpotByHome(glm::vec2 home, glm::vec2 door, float homeRadius, const IntRandom& intRandom,
                                               const FloatRandom& floatRandom)
{
	const float out = gutils::Get3DAngleFromXZ(home, door);
	const float side = intRandom(2) != 0 ? 1.0f : -1.0f;
	const float angle = side * floatRandom(k_BreederFan) + out;
	const float half = homeRadius * k_BreederHomeShare;
	const float distance = floatRandom(half) + half;
	return routine::Polar(door, angle, distance);
}

glm::vec2 villager_breeding::BreederSpotByStore(glm::vec2 store, const FloatRandom& random)
{
	const float distance = random(k_BreederStoreRange) + k_BreederStoreLeast;
	const float angle = random(routine::k_TwoPi);
	return routine::Polar(store, angle, distance);
}

villager_breeding::Amble villager_breeding::BreederAmble(glm::vec2 spot, const FloatRandom& floatRandom,
                                                         const IntRandom& intRandom)
{
	const float angle = floatRandom(routine::k_TwoPi);
	const float distance = floatRandom(k_BreederAmble);
	const auto goal = routine::Polar(spot, angle, distance);
	return {.goal = goal, .pause = static_cast<uint16_t>(intRandom(k_BreederPauseRange) + k_BreederPauseLeast)};
}

glm::vec2 villager_breeding::NearMother(glm::vec2 centre, const FloatRandom& random)
{
	return routine::Polar(centre, random(routine::k_TwoPi), k_NearMother);
}
