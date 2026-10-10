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
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

// The rules of the wall-hugging walk that villagers (and other walkers) use to get round the circles of the things in
// their way.
//
// Speeds: a walker's speed is held in metres a second, as the game's tables give it. The game keeps it as a whole
// number of map units (a 65536th of ten metres) walked each turn, ten turns a second, so a walker goes a tenth of its
// speed in metres each turn.
//
// The walk itself is in whole map units at whole game angles, as the game's: a walker faces one of 2048 directions, and
// its step is worked out from the game's sine table, so it is a little shorter than its speed.
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

/// A map position (whole map units, a 65536th of ten metres) in metres, as the walk's geometry takes it
[[nodiscard]] glm::vec2 ToPoint(glm::ivec2 whole);
/// A point in metres as a map position, truncated
[[nodiscard]] glm::ivec2 ToWhole(glm::vec2 metres);
/// The square of the distance between two map positions, in metres
[[nodiscard]] float MetresDistanceSq(glm::ivec2 a, glm::ivec2 b);

/// The step a walker makes in a turn along a game angle (2048 to the circle, 0 along x, 512 along z), in whole map
/// units: a sixteenth of its speed (whole units a turn) times the sine table's 65536ths, over 4096
[[nodiscard]] glm::ivec2 StepAlong(uint16_t angle, int32_t wholeSpeed);
/// Whether a walker is within its step (its speed in whole units) of a point
[[nodiscard]] bool WithinStep(glm::ivec2 position, glm::ivec2 point, int32_t wholeSpeed);
/// How many game angles a walker turns each turn going round a circle of this radius (metres): its step over the
/// radius in game angles, truncated, and one more
[[nodiscard]] uint16_t OrbitTurn(int32_t wholeSpeed, float radius);
/// Which way a walker heading for a circle goes round it on reaching it: clockwise (x to the right, z up) when the
/// circle's middle lies to the right of its step
[[nodiscard]] bool GoesRoundClockwise(glm::ivec2 position, glm::ivec2 centre, glm::ivec2 step);

/// When a walker starts to go round a circle it notes how far from its goal it is, in 128ths of a metre less one; it
/// may only leave the circle once nearer the goal than that.
[[nodiscard]] uint32_t EntryDistance(glm::ivec2 position, glm::ivec2 goal);
/// Whether a walker going round a circle leaves it now: it is nearer its goal than when it started round, and the goal
/// lies ahead of it, off to the side away from the way it turns.
[[nodiscard]] bool LeavesCircle(glm::ivec2 position, glm::ivec2 goal, glm::ivec2 step, uint32_t entryDistance, bool clockwise);

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

/// How a thing on the map stands in the walkers' way
enum class ThingShape : uint8_t
{
	/// Not at all: forests as a whole, fields (walked over), and things that can be carried, like pots
	None,
	/// A small circle round its trunk, whatever its size: a tree
	Trunk,
	/// The outline of its model's box: one circle, or the row of circles along a long box (buildings, features, rocks,
	/// gates and the like)
	ModelBox,
	/// A temple: a wide circle over its middle and seven spokes of small circles round it, two of them bent towards
	/// each other where its way in is, the same whatever its model and size
	TempleRing,
};

/// The radius of a tree's trunk to the walkers, in metres
constexpr float k_TreeTrunkRadius = 0.3f;

/// A thing as the walkers see it: how it stands in their way, where it is placed, and its model's box (unscaled)
struct ThingOnMap
{
	ThingShape shape;
	glm::vec3 position;
	glm::mat3 rotation;
	float scale;
	glm::vec3 boxCentre;
	glm::vec3 boxHalfSize;
	/// A fence, which the walkers take as they take water or the land's edge
	bool fence;
};

/// The circles a thing stands in the walkers' way with, in the order they come across them
[[nodiscard]] std::vector<BlockingCircle> CirclesOf(const ThingOnMap& thing);

/// The circles of a temple whose middle stands at a point on the land, turned by an angle about the vertical (radians,
/// the direction its model's x axis points across the land, x towards z): the wide one over its middle, then each
/// spoke's circles from the inside out, the spokes in turn round the temple
[[nodiscard]] std::vector<BlockingCircle> TempleRingCircles(glm::vec2 centre, float yAngle);

/// Whether a model is a fence's: the American fence and the Celtic short and tall fences
[[nodiscard]] bool IsFenceModel(MeshId model);

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
	/// The game angle it now walks at, when it was turned
	std::optional<uint16_t> heading;
	/// Whether the last circle it was handed over to was water, the land's edge or a fence
	bool needsLookahead;
};

/// What a walker going round a circle looks at
struct CircleSweepInput
{
	/// The walker's map position and its goal's, in whole map units
	glm::ivec2 position;
	glm::ivec2 goal;
	/// The circle the walker hugs, in metres
	glm::vec2 centre;
	float radius;
	bool clockwise;
	/// The walker's speed in whole map units a turn
	int32_t wholeSpeed;
	/// The circles that may block it, in the order the game comes across them
	std::span<const BlockingCircle> blockers;
};

/// A walker going round a circle looks along its arc, as above
[[nodiscard]] CircleSweep SweepCircle(const CircleSweepInput& input);

/// What a walker heading straight on finds ahead of it
struct LineScan
{
	/// The turns until it reaches the circle ahead; k_NoObstacleInReach when nothing is in reach
	uint8_t turnsToObstacle;
	/// The circle it will reach, of those it was given
	std::optional<size_t> circle;
};

/// A walker heading straight on from its map position along its step looks for the nearest circle its line meets, of
/// those given in the order the game comes across them: the first circle whose near edge comes before every other's
/// along the line, where a circle the walker has gone more than a fifth of a metre into doesn't count. It reaches it
/// after the distance to that edge over its step, in whole turns; more than 255 turns away is out of reach.
[[nodiscard]] LineScan ScanLine(glm::ivec2 position, glm::ivec2 step, std::span<const BlockingCircle> circles);

} // namespace openblack::ecs::wall_hug
