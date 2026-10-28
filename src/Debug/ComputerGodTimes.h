/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <string>

namespace openblack::computer_god_times
{

/// The log gives each turn of a god that takes this long or longer, in milliseconds
constexpr double k_SlowTurnMs = 1.0;

/// The time of one part of the turns of a computer god: the sum, the longest and the turn of the longest
struct Part
{
	double totalMs {0.0};
	double longestMs {0.0};
	uint32_t longestTurn {0};

	void Add(double ms, uint32_t turn);
};

/// The times of the turns of one computer god. The mind is the full turn less the hand.
struct God
{
	uint32_t turns {0};
	/// The fill of the seen list round the hand
	Part see;
	/// The mind: the needs that it works out and the actions that it starts, with their searches
	Part mind;
	/// The steps of the action of the hand
	Part hand;
	/// The full turn of the god
	Part turn;
};

/// The times of each computer god in a run, for the developer log
class Times
{
public:
	/// Adds one turn of a god. The full time includes the fill of the seen list and the hand.
	void Add(int god, uint32_t turn, double seeMs, double handMs, double fullMs);
	[[nodiscard]] const std::map<int, God>& Gods() const { return _gods; }
	/// One line for each god: the turns, the mean and longest time of each part, in milliseconds
	[[nodiscard]] std::string Report(uint32_t turn) const;

private:
	std::map<int, God> _gods;
};

} // namespace openblack::computer_god_times
