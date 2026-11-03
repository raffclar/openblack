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

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// The states of a villager that plays football, is the referee, or watches a match
namespace openblack::ecs::villager_football
{

/// The state to go and build the pitch does nothing. Builders build a pitch as they build any building.
uint32_t MoveToPitchConstruction(components::LivingAction& action);
/// The player walks to its place for the kick-off
uint32_t WalkToPosition(components::LivingAction& action);
/// The player waits, and takes the attacker's role when the match plays
uint32_t WaitForKickOff(components::LivingAction& action);
/// The player plays in its role: attacker, goalkeeper or defender
uint32_t PlayRole(components::LivingAction& action);
/// The player celebrates or mourns a goal until the pitch gives it its role again
uint32_t AfterGoal(components::LivingAction& action);
/// The player that fetches a dead ball sets off for the ball
uint32_t StartMoveToPickUpBall(components::LivingAction& action);
/// At the ball, the player turns to the pitch and puts the ball down
uint32_t ArrivedAtPickUpBall(components::LivingAction& action);
uint32_t PutDownBallStart(components::LivingAction& action);
/// The player kicks the ball to the centre spot, and the match plays again
uint32_t PutDownBallEnd(components::LivingAction& action);
/// While the ball is out of play, the player looks at it
uint32_t MatchPaused(components::LivingAction& action);
/// One of the crowd watches the match
uint32_t WatchMatch(components::LivingAction& action);
/// One of the crowd does its part of the wave, then watches again
uint32_t MexicanWave(components::LivingAction& action);
/// The player runs to where a flying ball will come down
uint32_t MoveToBall(components::LivingAction& action);
/// A villager that goes into a state outside the match leaves its team. A walk inside the match keeps it in the team.
bool ExitFootball(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_football
