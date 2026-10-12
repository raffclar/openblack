/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Where a creature can walk, and the routes it takes there.
///
/// The land is a grid of 512 by 512 cells of 10 units, sorted once a land is loaded: cells steeper than about 45
/// degrees and deep sea are blocked, shallow water can be waded through, and land cut off from the main part of the
/// land is out of reach. Routes are chains of straight legs and arcs round what stands in the way (see
/// CreatureRouteSearch.h).
namespace openblack::creature_route
{
constexpr int32_t k_CellsPerSide = 512;
constexpr float k_CellSize = 10.0f;
/// A cell whose corners differ in height by more than this is too steep to walk on
constexpr float k_SteepRise = 10.0f;
/// A creature stands clear of cells it can't walk on by this much, and a destination by a little more
constexpr float k_Clearance = 7.05f;
constexpr float k_DestinationClearance = 7.1f;
/// Where a creature can't stand at a point, it looks this far round for the nearest place it can, which is only any use
/// nearer than the second
constexpr float k_ValidPointSearch = 1000.0f;
constexpr float k_ValidPointReach = 30.0f;

enum class Ground : uint8_t
{
	/// Land a creature can walk on
	Open = 0,
	/// Too steep, or deep sea
	Blocked = 1,
	/// Land cut off from the main part of the land
	CutOff = 4,
	/// Shallow water, waded through on the bottom
	Water = 6,
};

/// The land's cells, sorted by where creatures can walk
class WalkableLand
{
public:
	/// The height of a cell corner, x and z from 0 to 512, in world units
	using CornerHeight = std::function<float(int32_t x, int32_t z)>;
	/// Whether a cell has water in it, or nothing when the land has no such cell
	using CellWater = std::function<std::optional<bool>(int32_t x, int32_t z)>;

	WalkableLand();
	/// Sorts every cell: blocked where too steep or deep sea (all four corners at the bottom), then the largest stretch
	/// of land joined up side by side is open and the rest cut off, and open cells with water in them or none at all
	/// are water
	[[nodiscard]] static WalkableLand Build(const CornerHeight& height, const CellWater& water);

	[[nodiscard]] Ground At(int32_t x, int32_t z) const;
	[[nodiscard]] Ground AtPoint(glm::vec2 point) const;
	/// Whether a creature may stand at a point: its cell is open or water, and no cell it may not stand on has its
	/// centre within the radius
	[[nodiscard]] bool IsValid(glm::vec2 point, float radius) const;
	/// The nearest point where a creature may stand, searching outwards in 16 directions half a unit at a time
	[[nodiscard]] std::optional<glm::vec2> NearestValid(glm::vec2 point, float radius, float maxDistance) const;

private:
	std::vector<Ground> _cells;
};

/// Something in the way, walked round at its radius, which includes as much of the creature's own as it keeps clear of
/// it by (see CreatureObstacles.h)
struct Circle
{
	glm::vec2 centre;
	float radius;
};

/// The shortest distance from a point to a segment
[[nodiscard]] float DistanceToSegment(glm::vec2 point, glm::vec2 from, glm::vec2 to);

/// A creature's side step towards a catch is checked against one block of 8 by 8 cells, 80 units a side
constexpr int32_t k_StepBlockCells = 8;
constexpr float k_StepBlockSize = 80.0f;
/// The block a step ending at a point is checked against: its column from the point's x and, as the game has it, its row
/// from the point's height rather than its z, so that for ordinary heights it is a block of the land's first rows
[[nodiscard]] glm::ivec2 StepBlock(glm::vec3 end);
/// The circles a side step must not end in, gathered over a block as the game gathers them: for each cell of the block,
/// a circle of the destination clearance at the centre of each of the cell and its eight neighbours that may not be
/// stood on. The game reads each neighbour's sort from the cell one row further on, and only blocked cells count (water
/// is waded). A circle about a point within a tenth of a unit of where the creature stands is left out, and one reaching
/// further than it is shrunk to reach it; one reaching off the land's last 80 units is left out.
[[nodiscard]] std::vector<Circle> StepBlockCircles(const WalkableLand& land, glm::ivec2 block, glm::vec2 standing);
/// Whether a point lies strictly inside any of the circles
[[nodiscard]] bool InsideAny(glm::vec2 point, const std::vector<Circle>& circles);

/// Which way round a circle a route goes
enum class Way : uint8_t
{
	None,
	Clockwise,
	Anticlockwise,
};

/// The other way round, none staying none
[[nodiscard]] constexpr Way Opposite(Way way)
{
	switch (way)
	{
	case Way::Clockwise:
		return Way::Anticlockwise;
	case Way::Anticlockwise:
		return Way::Clockwise;
	default:
		return Way::None;
	}
}

/// Anticlockwise for clockwise, and clockwise for anything else
[[nodiscard]] constexpr Way Flip(Way way)
{
	return way == Way::Clockwise ? Way::Anticlockwise : Way::Clockwise;
}

/// One part of a route: straight, or round the circle the part before headed for
struct Leg
{
	glm::vec2 from;
	glm::vec2 to;
	bool arc {false};
	/// The circle it heads for and which way round it goes, none when it heads for no circle
	std::optional<uint32_t> heading {};
	Way way {Way::None};
	/// It begins on a circle the route starts on
	bool onStartCircle {false};
	/// The cost of the route up to its end
	float cost {0.0f};
	/// The circle an arc goes round and which way, kept once a route is followed
	std::optional<Circle> round {};
	Way roundWay {Way::None};
};

/// How far round a circle it is from one point on it to another, going one way
[[nodiscard]] float ArcLength(const Circle& circle, Way way, glm::vec2 from, glm::vec2 to);

/// A route being followed: along each leg in turn, straight or round its circle
struct Route
{
	std::vector<Leg> legs;
	/// The leg it is on, and how far along it the creature is
	size_t segment {0};
	float travelled {0.0f};

	[[nodiscard]] bool Finished() const { return segment >= legs.size(); }
	[[nodiscard]] bool OnLastSegment() const { return segment + 1 >= legs.size(); }
	[[nodiscard]] float SegmentLength() const;
	/// How far is left of this leg, and of the whole route
	[[nodiscard]] float Remaining() const;
	[[nodiscard]] float RemainingTotal() const;
	[[nodiscard]] glm::vec2 Position() const;
	/// Where the route ends
	[[nodiscard]] glm::vec2 End() const;
	/// The heading along this leg where the creature is (see creature_locomotion::HeadingOf)
	[[nodiscard]] float Heading() const;
	/// How far the route turns at the end of this leg, 0 at the last
	[[nodiscard]] float TurnAtEnd() const;
	/// Points along the route, arcs in steps of at most an angle, for drawing it
	[[nodiscard]] std::vector<glm::vec2> Points(float maxStepAngle) const;
};

/// A leg's length: an arc's round its circle, else straight
[[nodiscard]] float LengthOf(const Leg& leg);
/// Where along a leg a distance from its start is, and the heading there
[[nodiscard]] glm::vec2 PointAlong(const Leg& leg, float distance);
[[nodiscard]] float HeadingAlong(const Leg& leg, float distance);

struct Advanced
{
	glm::vec2 position;
	float heading;
	/// Whether it went on to a new leg
	bool segmentChanged;
	bool finished;
};
/// Moves a distance along the route
Advanced Advance(Route& route, float distance);
} // namespace openblack::creature_route
