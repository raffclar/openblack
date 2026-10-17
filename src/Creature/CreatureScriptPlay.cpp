/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureScriptPlay.h"

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
	std::vector<Step> agenda;
	agenda.reserve(static_cast<size_t>(request.plays) + 1);
	for (uint32_t i = 0; i < request.plays; ++i)
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
	// Then it waits for its body to be free, which the last action's end already waits for; with nothing to play, it
	// only waits
	if (agenda.empty())
	{
		agenda.push_back({.kind = Step::Kind::Wait, .seconds = 0.0f});
	}
	return agenda;
}

bool creature_script_play::Played(const creature_mind::IdleMind& mind)
{
	return mind.activity == creature_mind::Activity::BeIdle || mind.step >= mind.agenda.size();
}

void creature_script_play::ClearAfterPlayed(creature_mind::IdleMind& mind)
{
	if (mind.step >= mind.agenda.size())
	{
		return;
	}
	mind.step = mind.agenda.size();
	mind.stepStarted = false;
	mind.gaveUp = true;
	mind.stoppedByScript = true;
}
