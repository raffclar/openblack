/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSteal.h"

#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_steal;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;

bool creature_steal::IsStealable(const Facts& facts)
{
	// A thing in the influence of its own player is not a thing to steal
	return (facts.toy || facts.scaffold || facts.oneShot) && facts.canPickUp && !(facts.ownInfluence > 0.0f);
}

float creature_steal::PutDownWithin(float height)
{
	return height + height;
}

std::vector<Step> creature_steal::StealAndPutByTemple(std::optional<uint32_t> pickUp, glm::vec2 home, float height)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = *pickUp},
		                  .gaze = Gaze {.object = *pickUp, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint, .point = home, .maxDistance = PutDownWithin(height)},
	                  .gaze = creature_mind::GoingLook(home),
	                  .main = true});
	agenda.push_back(
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}});
	return agenda;
}
