/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureScriptPlay.h"

using namespace openblack;
using namespace openblack::creature_script_play;

std::optional<std::vector<creature_mind::Step>> creature_script_play::Agenda(const Request& request)
{
	if (request.animation <= k_LastStaticAction)
	{
		// TODO(opening-skip): static actions take their start, loop and end from the game's table of them, held for
		// k_StaticActionSeconds; no script of the first land plays one
		return std::nullopt;
	}
	std::vector<creature_mind::Step> agenda;
	agenda.reserve(static_cast<size_t>(request.plays) + 1);
	for (uint32_t i = 0; i < request.plays; ++i)
	{
		// Each starts once its body is free, and is played through
		agenda.push_back(
		    {.kind = creature_mind::Step::Kind::Action, .seconds = 0.0f, .animation = static_cast<size_t>(request.animation)});
	}
	// Then it waits for its body to be free, which the last action's end already waits for; with nothing to play, it
	// only waits
	if (agenda.empty())
	{
		agenda.push_back({.kind = creature_mind::Step::Kind::Wait, .seconds = 0.0f});
	}
	return agenda;
}

bool creature_script_play::Played(const creature_mind::IdleMind& mind)
{
	return mind.activity == creature_mind::Activity::BeIdle || mind.step >= mind.agenda.size();
}
