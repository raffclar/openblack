/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerGodTimes.h"

#include <algorithm>

#include <fmt/format.h>

namespace openblack::computer_god_times
{

void Part::Add(double ms, uint32_t turn)
{
	totalMs += ms;
	if (ms > longestMs)
	{
		longestMs = ms;
		longestTurn = turn;
	}
}

void Times::Add(int god, uint32_t turn, double seeMs, double handMs, double fullMs)
{
	auto& times = _gods[god];
	++times.turns;
	times.see.Add(seeMs, turn);
	times.hand.Add(handMs, turn);
	times.mind.Add(std::max(fullMs - seeMs - handMs, 0.0), turn);
	times.turn.Add(fullMs, turn);
}

std::string Times::Report(uint32_t turn) const
{
	std::string report;
	for (const auto& [god, times] : _gods)
	{
		const double turns = std::max(static_cast<double>(times.turns), 1.0);
		const auto part = [turns](const Part& part) {
			return fmt::format("{:.4f}/{:.4f}@{}", part.totalMs / turns, part.longestMs, part.longestTurn);
		};
		report += fmt::format("Computer god times at turn {}: god {} turns {} see {} mind {} hand {} turn {}\n", turn, god,
		                      times.turns, part(times.see), part(times.mind), part(times.hand), part(times.turn));
	}
	return report;
}

} // namespace openblack::computer_god_times
