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

#include <glm/vec2.hpp>

#include "Common/GameHeading.h"

// How a creature turns to face a camera, one game turn at a time, as the game does it. The step reads the place of the
// camera again on each turn, so the creature turns to where the camera is now. There are two kinds of the step:
//
// The turn to the camera of its player (Watcher::ItsPlayer):
// 1. The start: a creature with no player, or with the camera right above it, goes to the check at once. Else it
//    stops and asks its body to turn on the spot. While the body refuses, the creature ends a point and asks again on
//    the next turn. The start does not wait for the body.
// 2. The check: a creature with no player is done. It waits while the body moves, turns or plays an action. It is done
//    when the camera is right above it or when it faces the camera. Else it turns again.
//
// The turn to the camera of the player that gave it a thing, before it takes the thing (Watcher::Giver):
// 1. The start: with no giver, the step fails. With the camera right above it, the creature goes to the check at
//    once. Else it asks its body to turn. While the body refuses, it asks again on the next turn. It does not end a
//    point.
// 2. The check: it waits while the body moves, turns or plays an action. Then with no giver the step fails. Else the
//    check is the same as the check of the turn to the camera of its player.
//
// Pure, tested with made-up bodies.

namespace openblack::creature_camera_turn
{

/// Along the ground, a camera nearer than this is right above the creature
inline constexpr float k_OnTopDistance = 0.1f;
/// The creature faces the camera when its heading is less than this many radians from the heading to the camera. The
/// difference does not go round the circle.
inline constexpr float k_FacingRadians = 0.2f;

/// The camera that the creature turns to
enum class Watcher : uint8_t
{
	/// The camera of its own player
	ItsPlayer,
	/// The camera of the player that gave it a thing
	Giver,
};

/// The part of the turn that the creature does now
enum class Phase : uint8_t
{
	Start,
	Check,
};

/// The result of one turn of the step
enum class Outcome : uint8_t
{
	Running,
	Done,
	Failed,
};

/// What the creature finds on this turn
struct View
{
	/// The place of the camera on the ground. None for a creature without a player, or without a giver.
	std::optional<glm::vec2> camera;
	/// Where the creature stands, along the ground
	glm::vec2 creature {0.0f};
	/// The way that its body faces
	GameHeading heading {};
	/// Its body moves, turns or plays an action
	bool bodyBusy {false};
};

/// The way from the creature's map position to the camera. The camera's place is not made a map position.
[[nodiscard]] glm::vec2 OffsetToCamera(glm::vec2 creature, glm::vec2 camera);
/// The length of the way along the ground
[[nodiscard]] float GroundLength(glm::vec2 offset);
/// Whether a body with a heading faces a heading that it turns to. The difference does not go round the circle.
[[nodiscard]] bool Facing(GameHeading body, GameHeading target);

/// Does one turn of the turn to a camera. `startTurn` stops the creature and asks its body to turn on the spot to a
/// heading. It gives true when the body starts the turn. `endPoint` ends a point that the body plays.
template <typename StartTurn, typename EndPoint>
[[nodiscard]] Outcome Step(Watcher watcher, Phase& phase, const View& view, StartTurn&& startTurn, EndPoint&& endPoint)
{
	const bool giver = watcher == Watcher::Giver;
	if (phase == Phase::Start)
	{
		if (!view.camera.has_value())
		{
			if (giver)
			{
				return Outcome::Failed;
			}
			phase = Phase::Check;
			return Outcome::Running;
		}
		const auto offset = OffsetToCamera(view.creature, *view.camera);
		if (GroundLength(offset) < k_OnTopDistance || startTurn(GameHeading::Of(offset.x, offset.y)))
		{
			phase = Phase::Check;
			return Outcome::Running;
		}
		if (!giver)
		{
			endPoint();
		}
		return Outcome::Running;
	}
	// The turn to the camera of its player examines the player before the body. The turn to the giver's camera
	// examines the body first.
	if (!giver && !view.camera.has_value())
	{
		return Outcome::Done;
	}
	if (view.bodyBusy)
	{
		return Outcome::Running;
	}
	if (!view.camera.has_value())
	{
		return Outcome::Failed;
	}
	const auto offset = OffsetToCamera(view.creature, *view.camera);
	if (GroundLength(offset) < k_OnTopDistance)
	{
		return Outcome::Done;
	}
	const auto target = GameHeading::Of(offset.x, offset.y);
	if (Facing(view.heading, target))
	{
		return Outcome::Done;
	}
	// The result of the new turn does not change the step
	static_cast<void>(startTurn(target));
	return Outcome::Running;
}

} // namespace openblack::creature_camera_turn
