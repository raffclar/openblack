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
	return CastWaterAt(static_cast<uint32_t>(MagicType::Water), object, height);
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
		Step pickUp {.kind = Step::Kind::Object, .gaze = Gaze {.object = instrument, .whileGoing = true}};
		pickUp.order = {.kind = ObjectOrder::Kind::PickUp, .object = *instrument};
		agenda.push_back(pickUp);
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = k_SetFireDistance},
	                  .gaze = Gaze {.object = object, .whileGoing = true}});
	// It turns to the thing with an angry face, fluttering its eyelids at it, and smiles as it throws the fire
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_CastSettleSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = Gaze {.object = object},
	                  .flutterEyelids = true});
	Step toss {.kind = Step::Kind::Object, .face = creature_face::Cue::Smile};
	toss.order = {.kind = ObjectOrder::Kind::Discard, .animation = k_SetFireToss};
	agenda.push_back(toss);
	// It watches the burning thing until it lands, then waits a moment with an angry face
	Step land {.kind = Step::Kind::WaitInMap, .gaze = Gaze {.object = instrument}};
	land.object = *instrument;
	agenda.push_back(land);
	agenda.push_back({.kind = Step::Kind::Wait, .seconds = k_SetFireWaitSeconds, .face = creature_face::Cue::Anger});
	return agenda;
}
