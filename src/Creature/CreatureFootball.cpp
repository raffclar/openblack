/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFootball.h"

#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_football;
using creature_mind::Gaze;
using creature_mind::ObjectOrder;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// The creature shows that it feels playful before the pick-up when a draw from this many lots is not 0
constexpr uint32_t k_PlayfulLots = 3;
} // namespace

bool creature_football::IsOnHomeTeam(uint32_t playerNumber)
{
	return (playerNumber & 1u) == 0;
}

bool creature_football::MySideJustScored(bool goalPause, bool homeMournedLastGoal, bool homeTeam)
{
	return goalPause && homeMournedLastGoal == homeTeam;
}

bool creature_football::OtherSideJustScored(bool goalPause, bool homeMournedLastGoal, bool homeTeam)
{
	return goalPause && homeMournedLastGoal == !homeTeam;
}

std::vector<Step> creature_football::ThrowBallAtGoal(uint32_t ball, glm::vec2 goal, bool handFull, const Random& random)
{
	std::vector<Step> agenda;
	if (!handFull)
	{
		if (random(k_PlayfulLots) > 0)
		{
			agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_FeelPlayful});
		}
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PickUp, .object = ball},
		                  .gaze = Gaze {.object = ball, .whileGoing = true}});
	}
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Throw, .point = goal}, .main = true});
	return agenda;
}

std::vector<Step> creature_football::StompOn(uint32_t thing)
{
	auto agenda = creature_mind::DestroyThing(thing);
	agenda.back().main = true;
	return agenda;
}

std::vector<Step> creature_football::CelebrateGoal()
{
	return {{.kind = Step::Kind::Action, .animation = animations::k_Happy, .main = true}};
}

std::vector<Step> creature_football::CommiserateGoal()
{
	return {{.kind = Step::Kind::Action, .animation = animations::k_Sad, .main = true}};
}
