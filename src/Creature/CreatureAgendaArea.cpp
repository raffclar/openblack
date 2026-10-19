/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAgendaArea.h"

#include <algorithm>

using namespace openblack;
using creature_mind::Movement;
using creature_mind::Step;

bool creature_agenda_area::RefusesStep(const creature_leash::AgendaArea& area, glm::vec2 creature, const Step& step)
{
	if (step.kind != Step::Kind::Move || step.movement.object.has_value())
	{
		return false;
	}
	switch (step.movement.kind)
	{
	case Movement::Kind::ToPoint:
		return creature_leash::RefusesWalk(area, step.movement.point);
	case Movement::Kind::Nearby:
		return creature_leash::RefusesWalk(area, creature + step.movement.point);
	default:
		return false;
	}
}

bool creature_agenda_area::RefusesAgenda(const creature_leash::AgendaArea& area, glm::vec2 creature,
                                         std::span<const Step> agenda)
{
	return std::ranges::any_of(agenda, [&](const Step& step) { return RefusesStep(area, creature, step); });
}

std::vector<Step> creature_agenda_area::WithoutRefusedWalks(const creature_leash::AgendaArea& area, glm::vec2 creature,
                                                            std::vector<Step> agenda)
{
	std::vector<Step> kept;
	kept.reserve(agenda.size());
	for (auto& step : agenda)
	{
		if (!RefusesStep(area, creature, step))
		{
			kept.push_back(std::move(step));
			continue;
		}
		// The main step is the last step that the agenda has when the agenda gets its main step
		if (step.main && !kept.empty())
		{
			kept.back().main = true;
		}
	}
	return kept;
}
