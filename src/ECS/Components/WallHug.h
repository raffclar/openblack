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

#include <entt/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

enum class MoveStateClockwise
{
	Undefined,
	CounterClockwise,
	Clockwise,
};

enum class MoveState
{
	Linear,
	Orbit,
	ExitCircle,
	StepThrough,
	FinalStep,
	Arrived,
};

template <MoveState S>
struct MoveStateTagComponent
{
	static constexpr MoveState k_Value = S;
	MoveStateClockwise clockwise;
	glm::vec2 stepGoal;
};

using MoveStateLinearTag = MoveStateTagComponent<MoveState::Linear>;
using MoveStateOrbitTag = MoveStateTagComponent<MoveState::Orbit>;
using MoveStateExitCircleTag = MoveStateTagComponent<MoveState::ExitCircle>;
using MoveStateStepThroughTag = MoveStateTagComponent<MoveState::StepThrough>;
using MoveStateFinalStepTag = MoveStateTagComponent<MoveState::FinalStep>;
using MoveStateArrivedTag = MoveStateTagComponent<MoveState::Arrived>;

/// The circle a walker hugs or is heading for, and how many turns it goes before it looks again (0xff for no end in
/// reach)
struct WallHugObjectReference
{
	uint8_t stepsAway;
	/// The thing whose circle it is; none for a stretch of water or the land's edge
	entt::entity entity;
	glm::vec2 centre;
	float radius;
	/// How far from its goal the walker was when it started round the circle, in 128ths of a metre less one
	uint32_t entryDistance {0};
};

/// A walker that came up against water, the land's edge or a fence while going round: once a turn the game takes one
/// such walker and tries out its walk both ways round, turning it round if the other way is better. A walker is tried
/// once until it next leaves a circle, stops hugging or sets off on a new walk.
struct WallHugLookahead
{
	/// Whether its trial has been made (else it waits for one)
	bool tried {false};
	/// How the trial went: the turns to spare each way (-1 for never getting there), and whether it was turned round
	int32_t spareAsItGoes {0};
	int32_t spareTurnedRound {0};
	bool turnedRound {false};
	/// Whether turning round would have put it where a closed ring of overlapping circles shuts it in, so it was not
	/// turned round (openblack's own rule)
	bool landingShutIn {false};
};

struct WallHug
{
	glm::vec2 goal;
	/// The step the walker makes each turn, in whole map units (a 65536th of ten metres); none until the walk is set up
	glm::ivec2 step {0, 0};
	float yAngle; // FIXME(bwrsandman): member is a little redundant with transform or atan on step could keep or not
	/// In metres a second; the walk goes a tenth of it each turn
	float speed;
	/// The way the walker faces, in game angles (2048 to the circle)
	uint16_t gameAngle {0};
	/// Turns until a walker stepping straight to its goal aims at it again
	int8_t turnsUntilStepRebuild {0};
	/// Where the walk holds the walker, in whole map units, and where it last put it in metres: when something else
	/// moves the walker the walk takes up its new place
	glm::ivec2 position {0, 0};
	glm::vec2 placedAt {-1.0f, -1.0f};
};

} // namespace openblack::ecs::components
