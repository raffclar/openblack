/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptContainerWalk.h"

#include <algorithm>
#include <iterator>
#include <limits>

using namespace openblack::ecs;

entt::entity script_containers::FirstInTown(std::span<const entt::entity> firstBuildingPeople,
                                            std::span<const entt::entity> homeless)
{
	if (!firstBuildingPeople.empty())
	{
		return firstBuildingPeople.front();
	}
	return homeless.empty() ? entt::entity {entt::null} : homeless.front();
}

entt::entity script_containers::NextAfter(std::span<const entt::entity> members, entt::entity after)
{
	const auto found = std::ranges::find(members, after);
	if (found == members.end() || std::next(found) == members.end())
	{
		return entt::null;
	}
	return *std::next(found);
}

bool script_containers::IsNotNear(float distance, float radius)
{
	return distance > radius;
}

float script_containers::TownAdults(uint32_t adults)
{
	// The count has no sign, and one rounding gives the nearest number
	return static_cast<float>(adults);
}

float script_containers::TownAdultRoom(int32_t room)
{
	return static_cast<float>(room);
}

float script_containers::AbodeAdults(uint32_t adults)
{
	return static_cast<float>(static_cast<uint8_t>(adults));
}

float script_containers::AbodeAdultRoom(uint32_t room)
{
	return static_cast<float>(room);
}

float script_containers::TownCapacity(int32_t adultRoom, int32_t childRoom)
{
	return static_cast<float>(adultRoom + childRoom);
}

float script_containers::AbodeCapacity(uint32_t adultRoom, uint32_t childRoom)
{
	return static_cast<float>(adultRoom + childRoom);
}

float script_containers::SlowestSpeed(std::span<const float> speeds)
{
	float slowest = std::numeric_limits<float>::max();
	for (const float speed : speeds)
	{
		if (speed < slowest)
		{
			slowest = speed;
		}
	}
	return slowest;
}

entt::entity script_containers::BuildingInTown(std::span<const TownBuilding> buildings, AbodeNumber number, float minBuilt,
                                               bool excludingScripted)
{
	for (const auto& building : buildings)
	{
		const bool agrees = building.number == number && !(building.built < minBuilt) && !building.inScript;
		if (agrees || !excludingScripted)
		{
			return building.entity;
		}
	}
	return entt::null;
}

uint8_t script_containers::CarriedObjectFromScript(uint32_t value)
{
	return static_cast<uint8_t>(value & 0xFFU);
}
