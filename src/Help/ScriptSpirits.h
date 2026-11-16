/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>

/// How the scripts name the advisors: the rules between a script's spirit and the advisor it means
namespace openblack::help::script_spirits
{

/// The advisors as the help names them: 1 the good one, 2 the evil one
constexpr int32_t k_GoodSpirit = 1;
constexpr int32_t k_EvilSpirit = 2;

/// An alignment from -1 (evil) to 1 (good) in seven steps, 0 the most evil to 6 the most good
[[nodiscard]] int32_t DiscreteAlignment(float alignment);

/// The advisor a script's spirit means: none or good (0, 1) the good one, evil (2) the evil one, the player's alignment
/// (3) the evil one for an alignment below neutral, its opposite (4) the evil one from neutral up, random (5) either,
/// by a number below 100 that `rand100` draws only then
[[nodiscard]] int32_t HelpSpiritOf(int32_t scriptSpirit, int32_t discreteAlignment, const std::function<uint32_t()>& rand100);

/// Whether the advisor natives of a script read the state of this computer: its advisors, the alignment of its player
/// and the random numbers of its help. They do in a game alone. In a network game, only the help scripts do. Each
/// computer runs these for its own player, and they change only what this computer shows.
[[nodiscard]] constexpr bool ReadsThisComputer(bool networkGame, bool helpScript)
{
	return !networkGame || helpScript;
}

/// Whether an advisor has played its animation, as a script asks. A script that reads this computer asks the advisor.
/// In a network game, a script that each computer runs reads the world of the turns. That world has no advisors, as on
/// the game server, thus each advisor has played.
[[nodiscard]] constexpr bool SpiritPlayed(bool readsThisComputer, bool advisorPlays)
{
	return !readsThisComputer || !advisorPlays;
}

/// The advisor who says a help text, by its narrator: 1 good, 2 evil, 0 neither
[[nodiscard]] int32_t SpiritWhoTalks(int32_t narrator);

/// The help text that a script asks the speaker of. A number past the last help text asks for the first text.
[[nodiscard]] uint32_t TextToAskSpeakerOf(uint32_t text, uint32_t textCount);

/// A script's fraction across or down the screen, from 0 to 1. Not a number is not one.
[[nodiscard]] bool IsScreenFraction(float value);

} // namespace openblack::help::script_spirits
