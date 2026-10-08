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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The player's hand on a creature (see components::HandOnCreature). The hand can't pick a creature up: an empty hand
/// pressed on the player's own creature takes hold of it (creature_hand::TakesPress) once the next turn locks it for the
/// hand. Resting on its body the hand strokes it, swept fast across it the hand slaps it, and the creature reacts to each
/// at once. When the hand lets go, how the creature was treated, from -1 to 1, goes to its mind at the next turn.
class CreatureHandSystemInterface
{
public:
	/// Where the hand is drawn while held to a creature, and how it is posed
	struct HandPose
	{
		glm::vec3 position;
		/// Resting on the body rather than beside it
		bool onBody;
		/// Just slapped
		bool slapping;
	};
	/// A creature a line of sight meets, and how far along it
	struct CreatureHit
	{
		entt::entity creature;
		float distance;
	};
	/// How the hand treated the creature it let go of, for its mind
	struct Feedback
	{
		entt::entity creature;
		/// From -1 to 1
		float feedback;
	};

	virtual ~CreatureHandSystemInterface() = default;

	/// The hand's locked select reached the creature: whether the hand now holds it. A creature that does not let the
	/// hand hold it (MayHold) is refused.
	virtual bool Grab(entt::entity creature) = 0;
	/// Whether the creature lets the player's hand take hold of it to stroke and slap it
	[[nodiscard]] virtual bool MayHold(entt::entity creature) const = 0;
	/// Once a frame while held to a creature, with the line of sight through the cursor, the cursor on the screen and the
	/// hand's seconds of camera time: strokes and slaps, and where the hand is (kept for Pose)
	virtual std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                                       float seconds) = 0;
	/// The hand lets go: the creature it held and how it was treated, which the hand sends on to its mind; nothing when it
	/// held none
	[[nodiscard]] virtual std::optional<Feedback> Release() = 0;
	/// Where the hand was drawn by the last Update while held to a creature; nothing when it is not held to one
	[[nodiscard]] virtual std::optional<HandPose> Pose() const = 0;

	/// The creature the hand is held to, if any
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	/// The nearest creature along a line of sight, by its posed body
	[[nodiscard]] virtual std::optional<CreatureHit> CreatureAlong(const glm::vec3& rayOrigin,
	                                                               const glm::vec3& rayDirection) const = 0;
	/// The creature under the hand this frame, from the hand's own pick, as the hover and the press see it
	[[nodiscard]] virtual std::optional<entt::entity> CreatureUnderHand() const = 0;
	/// The hand's update stores the creature its pick found under it, or none
	virtual void SetCreatureUnderHand(std::optional<entt::entity> creature) = 0;
	/// How the creature has been treated since the hand took hold, from -1 to 1
	[[nodiscard]] virtual float GetFeedbackSum() const = 0;
	/// While held to a creature the same, and after letting go the sum it let go with, until it takes hold again
	[[nodiscard]] virtual float GetLastFeedbackSum() const = 0;
};

} // namespace openblack::ecs::systems
