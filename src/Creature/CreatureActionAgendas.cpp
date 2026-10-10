/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureActionAgendas.h"

#include <algorithm>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureThrow.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
Step PlayOnce(size_t animation, bool sleepyEyes = false, creature_face::Cue face = creature_face::Cue::None)
{
	return {.kind = Step::Kind::Action, .animation = animation, .sleepyEyes = sleepyEyes, .face = face};
}

Step GoNear(uint32_t thing, float distance, creature_face::Cue face = creature_face::Cue::None,
            std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = thing, .maxDistance = distance},
	        .face = face,
	        .gaze = gaze};
}

/// Turning to face a thing, holding still a while once it does
Step FaceThing(uint32_t thing, float seconds, creature_face::Cue face = creature_face::Cue::None,
               std::optional<Gaze> gaze = std::nullopt)
{
	return {.kind = Step::Kind::Move,
	        .seconds = seconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .face = face,
	        .gaze = gaze};
}

} // namespace

std::vector<Step> creature_mind::HurlAt(uint32_t thrown, uint32_t target, glm::vec2 targetPoint, float height, bool handFull,
                                        const Random& random)
{
	std::vector<Step> agenda;
	if (!handFull)
	{
		if (random(k_AngryBeforeHurlingLots) == 0)
		{
			agenda.push_back(PlayOnce(animations::k_Angry, false, creature_face::Cue::Amazed));
		}
		agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::PickUp, .object = thrown}});
	}
	const Gaze watch {.object = target};
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = target, .maxDistance = height},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	agenda.push_back({.kind = Step::Kind::Object,
	                  .effect = Effect::Hurled,
	                  .order = {.kind = ObjectOrder::Kind::Throw, .object = target, .point = targetPoint},
	                  .face = creature_face::Cue::Anger,
	                  .gaze = watch});
	return agenda;
}

std::vector<Step> creature_mind::SleepAtHome(glm::vec2 home, float height, const Random& random)
{
	std::vector<Step> agenda;
	if (random(k_YawnBeforeSleepingLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Tired, true));
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToPoint,
	                               .point = home,
	                               .maxDistance = std::min(k_FurthestFromHomeToSleep, height)}});
	agenda.push_back({.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::FaceDownSlope}});
	agenda.push_back({.kind = Step::Kind::Static,
	                  .sequence = {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep},
	                  .untilRested = true,
	                  .closedEyes = true,
	                  .effect = Effect::Slept});
	agenda.push_back(PlayOnce(animations::k_Confused, true, creature_face::Cue::Grimace));
	return agenda;
}

std::vector<Step> creature_mind::ExamineByLooking(uint32_t thing, float height, float thingHeight)
{
	// TODO(creature-do-action-2): the game then observes the thing (following it while it is interesting, keeping a
	// distance from it), which isn't ported yet; the agenda ends after the look
	return {
	    GoNear(thing, std::max(height * k_ExamineHeights, thingHeight)),
	    FaceThing(thing, k_ExamineFaceSeconds, creature_face::Cue::None, Gaze {.object = thing}),
	    {.kind = Step::Kind::Wait, .seconds = k_ExamineWatchSeconds, .gaze = Gaze {.object = thing, .bottom = true}},
	};
}

std::vector<Step> creature_mind::SmileAt(uint32_t friendly, float height, float chance, const Random& random)
{
	const Gaze look {.object = friendly};
	std::vector<Step> agenda {
	    GoNear(friendly, k_SmileHeights * height, creature_face::Cue::Smile, look),
	    FaceThing(friendly, k_SmileSeconds + (chance * k_SmileExtraSeconds), creature_face::Cue::Smile, look),
	};
	if (random(k_HappyAfterSmilingLots) == 0)
	{
		agenda.push_back(PlayOnce(animations::k_Happy));
	}
	return agenda;
}

std::vector<Step> creature_mind::WaveAt(uint32_t thing, float height)
{
	const Gaze look {.object = thing};
	auto wave = PlayOnce(animations::k_FriendlyWave);
	wave.gaze = look;
	return {
	    GoNear(thing, k_WaveHeights * height, creature_face::Cue::Smile),
	    FaceThing(thing, k_WaveFaceSeconds, creature_face::Cue::Smile, look),
	    wave,
	};
}

std::vector<Step> creature_mind::BeFrightenedOnTheSpot()
{
	return {
	    PlayOnce(animations::k_Frightened),
	    {.kind = Step::Kind::Wait, .seconds = k_FrightenedSeconds, .face = creature_face::Cue::Frightened},
	    PlayOnce(animations::k_Frightened),
	};
}

std::vector<Step> creature_mind::PutDown()
{
	return {
	    {.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::Discard, .animation = creature_throw::k_PutDown}}};
}

std::vector<Step> creature_mind::DeadForever()
{
	return {{.kind = Step::Kind::Static,
	         .seconds = k_DeadForeverSeconds,
	         .sequence = {animations::k_Faint, animations::k_Faint, animations::k_GetUp},
	         .holdLoop = true,
	         .closedEyes = true}};
}
