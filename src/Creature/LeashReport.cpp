/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashReport.h"

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::creature_leash;

std::string creature_leash::ReportLine(const CommandReport& report)
{
	auto line = fmt::format("Leash command: player {} {} at turn {}: ", report.player, report.command, report.turn);
	if (!report.creature.has_value())
	{
		return line + "the player has no creature";
	}
	line += fmt::format("creature {} ", *report.creature);
	if (!report.worn.has_value())
	{
		return line + "wears no leash";
	}
	const auto& worn = *report.worn;
	line += fmt::format("wears leash {} of player {}", static_cast<int>(worn.type), worn.holder);
	if (worn.tiedTo.has_value())
	{
		return line + fmt::format(", tied to thing {}", *worn.tiedTo);
	}
	return line + ", held in the hand";
}
