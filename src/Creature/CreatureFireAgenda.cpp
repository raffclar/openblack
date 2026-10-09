/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFireAgenda.h"

#include "CreatureCastAgenda.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;

std::vector<Step> creature_mind::PutOutFireWithWater(uint32_t object, float height)
{
	Step douse {.kind = Step::Kind::Douse};
	douse.object = object;
	return {
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = k_WaterGoNearDistance}},
	    {.kind = Step::Kind::Move,
	     .movement = {.kind = Movement::Kind::GetAwayFromObject, .object = object, .maxDistance = k_WaterCastHeights * height}},
	    {.kind = Step::Kind::Move,
	     .seconds = k_CastSettleSeconds,
	     .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object}},
	    {.kind = Step::Kind::Cast,
	     .seconds = k_CastHoldSeconds,
	     .animation = k_CastPose[1],
	     .sequence = k_CastPose,
	     .cast = {.magicType = static_cast<uint32_t>(MagicType::Water), .object = object}},
	    douse,
	};
}

std::optional<std::vector<Step>> creature_mind::SetFireTo(uint32_t object, std::optional<uint32_t> instrument, bool handFull)
{
	if (!instrument.has_value())
	{
		return std::nullopt;
	}
	std::vector<Step> agenda;
	if (!handFull)
	{
		Step pickUp {.kind = Step::Kind::Object};
		pickUp.order = {.kind = ObjectOrder::Kind::PickUp, .object = *instrument};
		agenda.push_back(pickUp);
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = k_SetFireDistance}});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_CastSettleSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object}});
	Step toss {.kind = Step::Kind::Object};
	toss.order = {.kind = ObjectOrder::Kind::Discard, .animation = k_SetFireToss};
	agenda.push_back(toss);
	Step land {.kind = Step::Kind::WaitInMap};
	land.object = *instrument;
	agenda.push_back(land);
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = k_SetFireWaitSeconds});
	return agenda;
}
