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

#include <glm/vec3.hpp>

/// The puff of dust thrown up where the hand grips the land, Black & White's GRIP_LANDSCAPE spot visual
/// (Data/Spells/ZSpellFiles/SF_GripLandscape).
///
/// A point a little above the ground grows from nothing to full size over a second. Eight dust sprites placed at random
/// within a sphere around it grow and spread out with it, cycling through the dust frames of S_SpriteSheet3, and fade out
/// within the first second.
///
/// The grip sends a command, and the turn of the command makes the puff on each computer of a network game. The turns
/// remove the puff, and the frames move it.
namespace openblack::GripLandscapeEffect
{
/// The age at which a puff goes, in seconds of game time
inline constexpr float k_DieSeconds = 2.65752f;

/// The steps of the turns that a puff lives: the step of the turn that makes it, then one step for each turn until the
/// game time of the turns after it reaches the age at which it goes
[[nodiscard]] constexpr int32_t LifeTurns(float dieSeconds, float turnSeconds)
{
	int32_t turns = 0;
	while (static_cast<float>(turns) * turnSeconds < dieSeconds)
	{
		++turns;
	}
	return turns + 1;
}

/// Throws up a puff of dust at a point on the ground. Only the turn of a grip command calls it.
void Spawn(glm::vec3 groundPosition);
/// At each turn: counts down the turns of the puffs, and removes those whose turns are done
void ProcessTurn();
/// Grows and fades the puffs by some seconds of game time. With no time, as while the game is paused, they stand still.
void Update(float gameSeconds);
} // namespace openblack::GripLandscapeEffect
