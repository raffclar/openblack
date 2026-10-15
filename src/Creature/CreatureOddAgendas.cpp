/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureOddAgendas.h"

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_odd;
using creature_face::Cue;
using creature_mind::Effect;
using creature_mind::Eyes;
using creature_mind::Gaze;
using creature_mind::Movement;
namespace animations = openblack::creature_layers::animations;

std::vector<Step> creature_odd::ShowLearntLesson()
{
	return {{.kind = Step::Kind::Action, .animation = animations::k_Scratch, .face = Cue::Smile}};
}

std::vector<Step> creature_odd::LookConfused(std::optional<glm::vec2> camera)
{
	const Gaze atCamera {.camera = true};
	std::vector<Step> agenda;
	if (camera.has_value())
	{
		agenda.push_back(
		    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::TurnToFace, .point = *camera}, .gaze = atCamera});
	}
	agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Scratch, .face = Cue::Puzzled, .gaze = atCamera});
	agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_Confused, .face = Cue::Puzzled, .gaze = atCamera});
	return agenda;
}

std::vector<Step> creature_odd::BehaveStrangely(float turnsPerSecond, const Random& random)
{
	std::vector<Step> agenda {
	    {.kind = Step::Kind::WatchCamera,
	     .seconds = k_StrangeStareSeconds,
	     .face = Cue::Curiosity,
	     .heldEyes = Eyes::Stoned,
	     .main = true},
	    {.kind = Step::Kind::Action, .animation = animations::k_Happy, .heldEyes = Eyes::Stoned},
	};
	// One time in two it faints and lies still for five to ten seconds, in whole turns
	if (random(2) == 0)
	{
		const float lie = k_StrangeFaintSeconds + creature_emote::FloatRandom(random, k_StrangeFaintExtraSeconds);
		agenda.push_back({.kind = Step::Kind::Static,
		                  .seconds = creature_emote::WholeTurnSeconds(lie, turnsPerSecond),
		                  .sequence = {animations::k_Faint, animations::k_Faint, animations::k_GetUp},
		                  .holdLoop = true,
		                  .closedEyes = true,
		                  .effect = Effect::Revived,
		                  .faint = true,
		                  .main = true});
	}
	agenda.push_back(
	    {.kind = Step::Kind::Action, .animation = animations::k_Confused, .sleepyEyes = true, .face = Cue::Grimace});
	agenda.push_back(
	    {.kind = Step::Kind::Action, .animation = animations::k_Confused, .face = Cue::Grimace, .heldEyes = Eyes::Stoned});
	return agenda;
}
