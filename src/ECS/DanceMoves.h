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

#include <array>
#include <functional>
#include <optional>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Dance.h"
#include "ECS/DanceShapes.h"

namespace openblack::dance
{
struct DanceAction;
}

/// How a dance's groups move: the shapes their dancers stand in, the moves the key frames start and each turn carries
/// on, where each dancer's place is, which way it faces and which clip it plays there. Every float operation is
/// rounded to a float, as the game's are.
namespace openblack::ecs::dance_moves
{

/// What a group's move does, by its number in the dance files
enum class Action : uint32_t
{
	None = 0,
	/// Its dancers go round its shape
	Spin = 1,
	/// They look at the first dancer of another group
	LookAtGroup = 2,
	/// They turn on the spot, an eighth of a turn every two turns
	TurnOnTheSpot = 3,
	/// They dance one of three dances, the men's or the women's
	Dance = 4,
	/// They face the group's centre
	FaceCentre = 7,
	/// They face away from it
	FaceAway = 8,
	/// Its shape turns
	Turn = 9,
	/// They play a clip of the dances' clip table
	Clip = 10,
	/// Its shape grows or shrinks
	Grow = 11,
	/// It moves off its centre
	MoveOff = 12,
	/// They face the way the dance faces, turned
	FaceDance = 13,
	/// They worship
	Worship = 14,
};

/// The shapes the groups stand in, as the game works them out
[[nodiscard]] const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& Shapes();

/// What one of a key frame's actions does to a group that isn't about its membership (the dance rules do those). `now`
/// is the dance's clock in whole beats. True when it starts a move, after which every dancer of the group plays its clip
/// again.
bool ApplyAction(components::Dance& dance, std::size_t group, uint32_t type, const std::array<uint32_t, 14>& arguments,
                 int32_t now);

/// A group starts a move: how much it changes the group each turn, the dance's rate shared over the beats left
void StartMove(components::DanceGroup& group, const components::DanceMove& move, int32_t now, float rate);

/// A turn of a group's move: it spins, turns, grows or moves off, and a move that grows or moves off stops at its end
void ProcessGroup(components::DanceGroup& group, int32_t now);

/// A point (x, z) turned about the centre by an angle in radians: its own angle worked out in radians, the sum made a
/// game angle, and the point put back at its length along it from the game's sine table
[[nodiscard]] glm::vec2 RotatePointByAngle(glm::vec2 point, float angle);

/// A point of a group's shape between the shape's points: `angle` is 0 to 256 round the shape. Turned by the group's
/// rotation when it has one.
[[nodiscard]] glm::vec2 ShapePoint(const components::DanceGroup& group, const dance_shapes::Shape& shape, float angle);

/// Where the dancer in a place of a group stands: round the group's shape, inside each group it is part of, turned the
/// way the dance faces, about the dance's place. Its altitude is the dance place's.
[[nodiscard]] map_coords::MapCoords SlotPosition(const components::Dance& dance, std::size_t group, std::size_t slot,
                                                 const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& shapes);

/// The group's centre: the dance's place moved by its offset and those of the groups it is part of, each in whole
/// metres
[[nodiscard]] map_coords::MapCoords GroupCentre(const components::Dance& dance, std::size_t group);

/// What the facing of a dancer depends on
struct FacingView
{
	Action action {Action::None};
	/// The game angle it faces now, the one from it to its group's centre, and the dance's facing
	uint16_t facing {0};
	uint16_t towardsCentre {0};
	float danceAngle {0.0f};
	float rotation {0.0f};
	/// The move's second argument, as a float
	float second {0.0f};
	/// How far it is from its place, in metres
	float distance {0.0f};
};

/// The game angle a dancer turns to face this turn for its group's move; none when the move doesn't turn it
[[nodiscard]] std::optional<uint16_t> Facing(const FacingView& view);

/// What the clip of a dancer depends on
struct ClipView
{
	/// Whether it is in a group, and whether the dance is stopped
	bool inGroup {false};
	bool stopped {false};
	/// Its group's move, by its number in the dance files, and the move's first argument
	uint32_t action {0};
	uint32_t first {0};
	/// The action of the group its group is part of, if any
	std::optional<Action> parentAction;
	/// The dance's rate
	float rate {0.0f};
	bool female {false};
	/// The clip it plays walking
	int32_t walkClip {0};
};

/// The clip a dancer plays in its place. The dance of the men and women is drawn from the game's random numbers.
[[nodiscard]] int32_t DanceClip(const ClipView& view, const std::function<uint32_t(uint32_t)>& random);

} // namespace openblack::ecs::dance_moves
