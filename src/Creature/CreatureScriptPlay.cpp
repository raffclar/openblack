/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureScriptPlay.h"

#include <algorithm>
#include <limits>

using namespace openblack;
using namespace openblack::creature_script_play;

std::vector<creature_mind::Step> creature_script_play::Agenda(const Request& request)
{
	using creature_mind::Step;
	// The game compares the number with no sign, so a number below 0 is an individual action
	const bool isStatic = request.animation >= 0 && request.animation <= k_LastStaticAction;
	if (request.plays > 0 && request.animation <= 0)
	{
		// The body has no clip below 0, so a static action 0 (which starts with the clip before it) and an individual
		// action below 0 never start. The creature tries again each turn and never gets past the first play.
		return {{.kind = Step::Kind::Wait,
		         .seconds = std::numeric_limits<float>::infinity(),
		         .retriedClip = isStatic ? creature_mind::RetriedClip::Static : creature_mind::RetriedClip::Individual}};
	}
	const auto animation = static_cast<size_t>(request.animation);
	// The game has no limit on the plays and writes the steps after the last slot of its agenda into other data. Only
	// the plays that fit in the agenda, with the wait after them, are kept.
	const auto plays = std::min<size_t>(request.plays, k_MostPlays);
	std::vector<Step> agenda;
	agenda.reserve(plays + 1);
	for (size_t i = 0; i < plays; ++i)
	{
		// Each starts once its body is free
		if (isStatic)
		{
			agenda.push_back({.kind = Step::Kind::Static,
			                  .seconds = k_StaticActionSeconds,
			                  .animation = animation,
			                  .sequence = {animation - 1, animation, animation + 1}});
		}
		else
		{
			agenda.push_back({.kind = Step::Kind::Action, .seconds = 0.0f, .animation = animation});
		}
	}
	// Then it waits until its body is free. With nothing to play, it only waits.
	agenda.push_back({.kind = Step::Kind::WaitUntilFree});
	return agenda;
}

bool creature_script_play::Played(const creature_mind::IdleMind& mind, bool planValid)
{
	// A last step that ended with empty last stages keeps the agenda until the next turn goes past those stages
	return !planValid || mind.activity == creature_mind::Activity::BeIdle || creature_mind::AgendaOver(mind);
}

bool creature_script_play::Played(const creature_mind::IdleMind& mind, bool planValid, bool knockedOut)
{
	if (knockedOut && planValid)
	{
		return false;
	}
	return Played(mind, planValid);
}

void creature_script_play::ClearAfterPlayed(creature_mind::IdleMind& mind)
{
	if (creature_mind::AgendaOver(mind))
	{
		return;
	}
	// The agenda does not go past the empty last stages of its last step, so it does not complete
	mind.endsNextTurn.reset();
	mind.step = mind.agenda.size();
	mind.stepStarted = false;
	mind.gaveUp = true;
	mind.stoppedByScript = true;
}
