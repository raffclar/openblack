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

#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// The actions of a creature on a villager that plays football: it throws the ball at a goal, stomps on the ball or on
/// a player, and celebrates or mourns a goal. The agendas and the rules here are pure, free of the game.
namespace openblack::creature_football
{
using creature_mind::Random;
using creature_mind::Step;

/// The creature of a player with an even number plays for the home team, and the creature of a player with an odd
/// number plays for the away team
[[nodiscard]] bool IsOnHomeTeam(uint32_t playerNumber);

/// Whether the side of the creature scored the last goal, as the game sees it. The game compares the side of the
/// creature with the side that mourns the goal, thus the result is the side that let the goal in.
[[nodiscard]] bool MySideJustScored(bool goalPause, bool homeMournedLastGoal, bool homeTeam);
/// Whether the other side scored the last goal, as the game sees it (see MySideJustScored)
[[nodiscard]] bool OtherSideJustScored(bool goalPause, bool homeMournedLastGoal, bool homeTeam);

/// With an empty hand, the creature shows two times in three that it feels playful, and picks up the ball. Then it
/// throws the ball at the goal. The actions to kick the ball at a goal, to clear it and to catch it do the same.
[[nodiscard]] std::vector<Step> ThrowBallAtGoal(uint32_t ball, glm::vec2 goal, bool handFull, const Random& random);
/// The creature walks to a thing and stomps on it in anger: the ball, or the first player of the other team
[[nodiscard]] std::vector<Step> StompOn(uint32_t thing);
/// The creature is happy about a goal of its side
[[nodiscard]] std::vector<Step> CelebrateGoal();
/// The creature is sad about a goal of the other side
[[nodiscard]] std::vector<Step> CommiserateGoal();

} // namespace openblack::creature_football
