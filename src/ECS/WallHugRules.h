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

#include <optional>
#include <span>

#include <glm/vec2.hpp>

// The rules of the wall-hugging walk that villagers (and other walkers) use to get round the circles of the things in
// their way.
//
// Speeds: a walker's speed is held in metres a second, as the game's tables give it. The game keeps it as a whole
// number of map units (a 65536th of ten metres) walked each turn, ten turns a second, so a walker goes a tenth of its
// speed in metres each turn.
//
// Going round a circle: each time the walker's next step crosses into another map cell, and when it has walked as many
// turns as the last look ahead found it could go, it looks along the arc of the circle it hugs, in the way it goes
// round, for the first point where another circle in that cell (or a stretch of water or the land's edge nearby) cuts
// across its path. When the walker's goal lies inside the circle it hugs, the point of the arc facing the goal ends the
// walk round as well: once the walker gets there it stops hugging and walks straight in to the goal. Whichever comes
// first along the arc decides: a point less than a step away hands the walker over to the circle that blocks it (looked
// at again in the same way, at most three times in a turn), or sends it straight to its goal; otherwise the walker
// keeps hugging for as many turns as the arc's length is worth, and turns to face round the circle, a little more
// outwards the further it is from the circle's middle.
//
// Pure, tested on made-up circles.

namespace openblack::ecs::wall_hug
{

/// The walk's turns in a second
constexpr float k_TurnsPerSecond = 10.0f;
/// The game's map units in a metre
constexpr float k_WholePerMetre = 65536.0f / 10.0f;
/// The fastest a walker can go, in whole map units a turn
constexpr int32_t k_MaxWholeSpeed = 0xffff;
/// The look ahead's answer when nothing blocks the walker in reach
constexpr uint8_t k_NoObstacleInReach = 0xff;
/// How far round a walker that is handed over too often in one turn keeps going before it looks again
constexpr uint8_t k_TurnsAfterTooManyHandovers = 10;
/// The radius of the circle a stretch of water or the land's edge takes up in a map cell, in metres
constexpr float k_LandscapeBlockerRadius = 7.2f;

/// A speed in metres a second as the game holds it: whole map units a turn
[[nodiscard]] int32_t WholeSpeed(float metresPerSecond);
/// How far a walker at this speed (metres a second) goes in a turn, in metres
[[nodiscard]] float StepMetres(float metresPerSecond);
/// How far a walker at this speed (metres a second) turns each turn going round a circle of this radius, in radians:
/// its step over the radius
[[nodiscard]] float OrbitTurn(float metresPerSecond, float radius);

/// When a walker starts to go round a circle it notes how far from its goal it is, in 128ths of a metre less one; it
/// may only leave the circle once nearer the goal than that.
[[nodiscard]] uint32_t EntryDistance(float metresFromGoal);
/// Whether a walker going round a circle leaves it now: it is nearer its goal than when it started round, and the goal
/// lies ahead of it, off to the side away from the way it turns.
[[nodiscard]] bool LeavesCircle(glm::vec2 position, glm::vec2 goal, glm::vec2 step, uint32_t entryDistance, bool clockwise);

/// A circle that can block a walker
struct BlockingCircle
{
	glm::vec2 centre;
	float radius;
	/// Water or the land's edge rather than a thing
	bool landscape;
	/// Water, the land's edge or a fence
	bool landscapeOrFence;
};

/// What a walker going round a circle does after looking along it
struct CircleSweep
{
	enum class Outcome
	{
		/// It keeps going round the circle it now hugs
		Hug,
		/// It stops hugging and steps straight towards its goal
		StepThrough,
	};
	Outcome outcome;
	/// The blocking circle it was handed over to, if it was
	std::optional<size_t> hugged;
	/// The turns it goes round before it looks again; k_NoObstacleInReach for no end in reach
	uint8_t turnsToObstacle;
	/// The heading it now walks at, in radians, when it was turned
	std::optional<float> heading;
	/// Whether the last circle it was handed over to was water, the land's edge or a fence
	bool needsLookahead;
};

/// What a walker going round a circle looks at
struct CircleSweepInput
{
	glm::vec2 position;
	glm::vec2 goal;
	/// The circle the walker hugs
	glm::vec2 centre;
	float radius;
	bool clockwise;
	/// The walker's speed in metres a second
	float speed;
	/// The circles that may block it, in the order the game comes across them
	std::span<const BlockingCircle> blockers;
};

/// A walker going round a circle looks along its arc, as above
[[nodiscard]] CircleSweep SweepCircle(const CircleSweepInput& input);

} // namespace openblack::ecs::wall_hug
