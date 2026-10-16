/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTownTheft.h"

#include <limits>

#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_town_theft;
using creature_face::Cue;
using creature_mind::Effect;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;

bool creature_town_theft::IsOwnStoragePit(bool built, std::optional<uint32_t> pitPlayer, std::optional<uint32_t> creaturePlayer)
{
	return built && pitPlayer == creaturePlayer;
}

bool creature_town_theft::IsOthersStoragePit(bool built, std::optional<uint32_t> pitPlayer,
                                             std::optional<uint32_t> creaturePlayer)
{
	return built && pitPlayer != creaturePlayer;
}

bool creature_town_theft::IsOthersField(std::optional<uint32_t> fieldPlayer, std::optional<uint32_t> creaturePlayer)
{
	return fieldPlayer != creaturePlayer;
}

std::optional<size_t> creature_town_theft::NearestTown(std::span<const float> metres)
{
	std::optional<size_t> nearest;
	float nearestMetres = std::numeric_limits<float>::max();
	for (size_t i = 0; i < metres.size(); ++i)
	{
		if (metres[i] < nearestMetres)
		{
			nearest = i;
			nearestMetres = metres[i];
		}
	}
	return nearest;
}

std::vector<Step> creature_town_theft::StealAndPutInTown(std::optional<uint32_t> pickUp, uint32_t pit, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = *pickUp},
		                  .gaze = Gaze {.object = *pickUp, .whileGoing = true}});
	}
	agenda.push_back(
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = pit, .maxDistance = height * k_PutInTownHeights},
	     .gaze = Gaze {.object = pit, .whileGoing = true}});
	agenda.push_back(
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}});
	return agenda;
}

std::vector<Step> creature_town_theft::StealFromStore(uint32_t from, uint32_t into, ObjectOrder::Handful look, float height)
{
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = from, .maxDistance = 0.0f},
	     .gaze = Gaze {.object = from, .whileGoing = true}},
	    {.kind = Step::Kind::Move,
	     .seconds = k_FaceStoreSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = from}},
	    {.kind = Step::Kind::Object,
	     .effect = Effect::TookFromPile,
	     .order = {.kind = ObjectOrder::Kind::TakeFromPile, .handful = look, .object = from}},
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::ToThrowPosition, .object = into, .maxDistance = height},
	     .face = Cue::Compassion,
	     .gaze = Gaze {.object = into}},
	    {.kind = Step::Kind::Move,
	     .seconds = k_FaceStoreSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = into},
	     .effect = Effect::Completed,
	     .gaze = Gaze {.object = into}},
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::ThrowInStore, .object = into}, .main = true},
	};
}
