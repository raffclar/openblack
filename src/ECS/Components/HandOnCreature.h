/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "Creature/CreatureFeedback.h"

namespace openblack::ecs::components
{

/// On the player's hand while it is held to a creature: how it has stroked and slapped it so far
struct HandOnCreature
{
	entt::entity creature {entt::null};
	/// The running sum of strokes and slaps, from -1 to 1
	float sum {0.0f};
	/// The clocks that strokes and slaps wait on
	creature_feedback::StrokeClocks clocks;
	/// The part last stroked, forgotten when the hand comes off the body
	std::optional<creature_feedback::BodyPart> lastPart;
	/// Strokes and slaps are weighed in steps (see creature_feedback::Pace): the time towards the next, and where the
	/// cursor was and whether the hand was on the body at the last
	float sinceStepMs {0.0f};
	glm::vec2 lastCursor {0.0f};
	/// Held by a command, the hand stays at this place
	std::optional<glm::vec3> lastPoint;
	bool wasOnBody {false};
	/// How fast the hand moved across the land at the last step, in units a second
	float speed {0.0f};
	/// Hard slaps in a row at one height
	creature_feedback::HardSlaps hardSlaps;
	/// The hand's cycle: taking hold starts it caressing
	creature_feedback::TouchCycle cycle {creature_feedback::TouchCycle::Tickle};
	/// Held by a command, such as a testbed scenario's, rather than by the button, which lets go only when told to
	bool byCommand {false};
	/// How long the button has been held, and whether the hand has stroked or slapped since, which tell a click on the
	/// creature from a hold
	float heldMs {0.0f};
	bool strokedOrSlapped {false};
	/// The hand's up, easing round to the body or upright (see creature_feedback::HandUpGlideTo), set going on the first
	/// frame held
	Zoomer3 up {glm::vec3(0.0f, 1.0f, 0.0f)};
	bool upStarted {false};
	/// The cursor's place at the last frame. It tells whether the cursor moved across this frame, and it gives the
	/// second line that looks for the body.
	std::optional<glm::ivec2> frameCursor;
	/// The distance of the hand from the camera along the line of sight, which starts when the hand takes hold
	Zoomer distance {0.0f};
	bool distanceStarted {false};
	/// The place where the hand showed at the last frame, which gives the height of a slap
	std::optional<glm::vec3> shownAt;
};

/// On the player's hand once it has let go of a creature: the sum of strokes and slaps it let go with, which the
/// creature's status panel goes on showing until the hand next takes hold of a creature
struct HandLastFeedback
{
	float sum {0.0f};
};

} // namespace openblack::ecs::components
