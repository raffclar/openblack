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

namespace openblack::audio
{

/// The first game turn of a land that does the audio turn. Before this turn, the land's turns do not change the music,
/// the ambience or the placed sounds, and the ambience is silent. The turns of the temple do not use this rule.
constexpr uint32_t k_FirstAudioTurn = 6;

/// The land's game turn does the audio turn: it chooses the music, starts and stops the script's music, and sets the
/// ambience and the placed sounds
[[nodiscard]] constexpr bool AudioTurnRuns(uint32_t turn)
{
	return turn >= k_FirstAudioTurn;
}

} // namespace openblack::audio
