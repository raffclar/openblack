/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>

namespace openblack::ecs::components
{

/// One player's belief symbol above a town centre
struct TownBeliefSymbol
{
	/// the two glows' cells and the second glow's angle, all 0 at first (frame_anim::PlayerSymbolCell /
	/// PlayerSymbolSpin)
	float glowA {0.0f}, glowB {0.0f}, glowSpin {0.0f};
	/// the symbol's angles; a1 and a2 start at a random angle in 0..2 pi each, phase at 0
	float a1 {0.0f}, a2 {0.0f}, phase {0.0f};
	/// the wait before the next fight (counted down), the fight's timer and its length: all 0 at first
	float waitLength {0.0f}, fightTimer {0.0f}, fightLength {0.0f};
	/// the last step's fight curve (1 - (2f - 1)^2 in a fight, else 0), which lowers the symbol in the position the
	/// same step writes
	float fight {0.0f};
	/// this frame's glow cells and spin (the symbol's draw, once a frame)
	float cellA {0.0f}, cellB {0.0f}, spin {0.0f};
};

/// The belief symbols of a town centre (psys::town_belief), by player: on the town centre's entity from the first
/// step that sees it, removed with the centre or when the effects are cleared
struct TownBeliefSymbols
{
	std::unordered_map<int, TownBeliefSymbol> symbols;
};

} // namespace openblack::ecs::components
