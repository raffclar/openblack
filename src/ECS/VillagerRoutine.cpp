/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerRoutine.h"

#include <array>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"

using namespace openblack;
namespace villager_routine = openblack::ecs::villager_routine;

glm::vec2 villager_routine::Polar(glm::vec2 from, float angle, float metres)
{
	auto coords = map_coords::FromMetres(from);
	gutils::AddDistanceFromAngle(coords, angle, metres);
	return map_coords::ToMetres(coords);
}

glm::vec2 villager_routine::PosOutsideDoor(glm::vec2 abode, glm::vec2 door, const FloatRandom& random)
{
	const float angle = gutils::Get3DAngleFromXZ(abode, door);
	const float distance = random(k_OutsideDoorRange) + k_OutsideDoorLeast;
	const float jitter = k_EighthPi - random(k_QuarterPi);
	return Polar(door, angle + jitter, distance);
}

glm::vec2 villager_routine::PosOutside(glm::vec2 abode, glm::vec2 door, float divisions, float leastDistance, float range,
                                       const FloatRandom& random)
{
	float angle = random(k_TwoPi / divisions) - k_TwoPi / (divisions + divisions);
	angle += gutils::Get3DAngleFromXZ(abode, door);
	const float distance = random(range) + leastDistance;
	return Polar(door, angle, distance);
}

glm::vec2 villager_routine::ChillOutPos(glm::vec2 congregation, glm::vec2 villager, float chillOutDistance,
                                        const FloatRandom& random)
{
	const float radius = chillOutDistance * k_ChillOutRadiusShare;
	const float angle = gutils::Get3DAngleFromXZ(congregation, villager);
	const float jitter = random(k_QuarterPi) - k_EighthPi;
	const float distance = random(radius * k_ChillOutRadiusSpan) + radius;
	return Polar(congregation, jitter + angle, distance);
}

villager_routine::Idle villager_routine::NothingToDo(bool hasAbode, bool abodeFunctional, bool hasTown, const IntRandom& random)
{
	constexpr uint32_t k_Ways = 9;
	constexpr uint32_t k_LastGoHome = 0;
	constexpr uint32_t k_LastOutsideHome = 3;
	constexpr uint32_t k_HomelessGoHomeChance = 100;
	constexpr uint32_t k_HomelessGoHomeUnder = 10;
	const uint32_t way = random(k_Ways);
	if (way <= k_LastGoHome && ((hasAbode && abodeFunctional) || random(k_HomelessGoHomeChance) < k_HomelessGoHomeUnder))
	{
		return Idle::GoHome;
	}
	if (way <= k_LastOutsideHome && hasAbode)
	{
		return Idle::ChillOutsideHome;
	}
	return hasTown ? Idle::SitInTown : Idle::GoHome;
}

villager_routine::SleepResult villager_routine::CheckSleep(const Sleep& sleep)
{
	if (sleep.poisoned)
	{
		return {};
	}
	SleepResult result;
	float life = sleep.life;
	if (life < sleep.fullLife)
	{
		// Life never goes over full
		const float after = life + sleep.multiplier * sleep.restores;
		result.gain = (after > 1.0f ? 1.0f : after) - life;
		life += result.gain;
	}
	result.sleepsOn = sleep.townWantsSleep || life < sleep.sleepUntil;
	return result;
}

glm::vec2 villager_routine::TowardsTown(glm::vec2 town, glm::vec2 villager, const FloatRandom& random)
{
	const float angle = random(k_HalfPi) - k_QuarterPi + gutils::Get3DAngleFromXZ(town, villager);
	const float distance = random(k_TowardsTownRange) + k_TowardsTownLeast;
	return Polar(town, angle, distance);
}

glm::vec2 villager_routine::RoundAbout(glm::vec2 from, float least, float range, const FloatRandom& random)
{
	const float distance = random(range) + least;
	const float angle = random(k_TwoPi);
	return Polar(from, angle, distance);
}

glm::vec2 villager_routine::VagrantWander(glm::vec2 villager, float facing, const FloatRandom& random)
{
	const float angle = random(k_QuarterPi) - k_EighthPi + facing;
	const float distance = random(k_VagrantWanderRange) + k_VagrantWanderLeast;
	return Polar(villager, angle, distance);
}

glm::vec2 villager_routine::VagrantTentSearch(glm::vec2 villager, const FloatRandom& random)
{
	return RoundAbout(villager, 0.0f, k_VagrantTentRange, random);
}

glm::vec2 villager_routine::BrokenHomeTentSearch(glm::vec2 abode, glm::vec2 villager, const FloatRandom& random)
{
	const float jitter = k_EighthPi - random(k_QuarterPi);
	const float angle = jitter + gutils::Get3DAngleFromXZ(abode, villager);
	const float distance = random(k_BrokenHomeTentRange) + k_BrokenHomeTentLeast;
	return Polar(villager, angle, distance);
}

glm::vec2 villager_routine::TentRetry(glm::vec2 spot, const FloatRandom& random)
{
	return RoundAbout(spot, k_TentRetryLeast, k_TentRetryRange, random);
}

bool villager_routine::AnswersTownEmergency(VillagerStates state)
{
	// The states of the game's table that answer its emergencies
	static constexpr auto k_Answers = [] {
		constexpr std::array<uint16_t, 206> k_States {
		    1,   2,   3,   6,   7,   8,   9,   12,  19,  20,  21,  22,  23,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,
		    35,  36,  37,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
		    59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
		    82,  83,  84,  85,  86,  87,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  101, 102, 103, 104, 105, 106, 107,
		    108, 109, 113, 114, 115, 116, 117, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 132, 133, 134, 135, 136, 137,
		    138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
		    161, 162, 163, 164, 165, 166, 167, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184,
		    185, 186, 187, 188, 189, 190, 191, 192, 193, 197, 201, 202, 203, 204, 209, 210, 211, 214, 221, 222, 223, 224, 225,
		    226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 248, 249, 250, 251, 253, 254};
		std::array<bool, 256> answers {};
		for (const auto s : k_States)
		{
			answers.at(s) = true;
		}
		return answers;
	}();
	const auto index = static_cast<size_t>(state);
	return index < k_Answers.size() && k_Answers.at(index);
}
