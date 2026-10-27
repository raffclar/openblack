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

#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureHandRules.h"
#include "Enums.h"

namespace openblack::player_commands
{
struct CreatureBruise;
struct CreatureReaction;
} // namespace openblack::player_commands

namespace openblack::ecs::systems
{

/// The player's hand on a creature (see components::HandOnCreature). The hand can't pick a creature up: holding the
/// right button on one holds the hand to it, if it is the player's own and awake (creature_hand::MayHold).
/// Let go quickly, the press was a click, which puts the leash on instead. While the button holds it, the camera
/// turns to look at the creature, and gives way back when the hand lets go. Resting on its body the hand strokes it, swept fast
/// across it the hand slaps it, and the creature reacts to each. When the hand lets go the creature's mind is told how it was
/// treated, from -1 to 1.
class CreatureHandSystemInterface
{
public:
	/// Where the hand is drawn while held to a creature, and how it is posed
	struct HandPose
	{
		glm::vec3 position;
		/// Resting on the body rather than beside it
		bool onBody;
		/// The hand's cycle: wiggling beside the body, caressing it or slapping
		creature_feedback::TouchCycle cycle;
		/// How much larger the hand is drawn, with the creature's size
		float scale;
		/// The way the back of the hand faces, turned to the body or upright
		glm::vec3 up;
	};

	virtual ~CreatureHandSystemInterface() = default;

	/// The hand's button was pressed over the creature the interface picked under the cursor: whether it is now held to a
	/// creature. A creature the player's hand may not touch is refused.
	virtual bool Grab() = 0;
	/// Whether the player's hand may take hold of the creature to stroke and slap it
	[[nodiscard]] virtual bool MayHold(entt::entity creature) const = 0;
	/// How the empty hand over the creature offers to interact with it, given whether the hand is within the player's
	/// influence (asked at the creature)
	[[nodiscard]] virtual creature_hand::InteractTip InteractTipOver(entt::entity creature, bool handInInfluence) const = 0;
	/// Whether the hand held to a creature by the button would, let go now, have been clicked on it rather than held
	[[nodiscard]] virtual bool IsClick() const = 0;
	/// Once a frame while held to a creature, with the line of sight through the cursor, the cursor on the screen, the
	/// screen's width and the frame's seconds: strokes and slaps, and where the hand is
	virtual std::optional<HandPose> Update(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::vec2 cursor,
	                                       float screenWidth, float seconds) = 0;
	/// The button was let go: the creature is told how it was treated
	virtual void Release() = 0;

	/// As if the hand rested on a part of a creature's body, or swept across it at a height (as a share of its height)
	/// hard or gently: it takes hold of the creature, held by a command rather than the button, and strokes or slaps it
	/// as the button would. Returns whether the creature reacted, which is when the stroke or slap counts.
	virtual bool Stroke(entt::entity creature, creature_feedback::BodyPart part) = 0;
	virtual bool Slap(entt::entity creature, float heightShare, bool gentle, bool sweepsRight) = 0;

	/// The creature the hand is held to, if any
	[[nodiscard]] virtual std::optional<entt::entity> GetCreature() const = 0;
	/// The creature that the world has the hand of a player hold, from the turn that did the lock of the press to the
	/// turn that does its end. The turns read only this, never the creature that the frames hold the hand to.
	[[nodiscard]] virtual std::optional<entt::entity> GetLockedCreature([[maybe_unused]] PlayerNames player) const
	{
		return std::nullopt;
	}
	/// Whether the world has the hand of a player hold the creature (see GetLockedCreature). Only the hand of the
	/// creature's own player can hold it.
	[[nodiscard]] virtual bool IsLocked([[maybe_unused]] entt::entity creature) const { return false; }
	/// At the turn: the hand of the player locks on to a creature, which stops and turns to the player's camera
	/// (player_commands::HandLock)
	virtual void DoLock(PlayerNames /*player*/, entt::entity /*creature*/) {}
	/// At the turn: the lock on the creature ends (player_commands::HandUnlock)
	virtual void DoUnlock(PlayerNames /*player*/, entt::entity /*creature*/) {}
	/// At the turn: a hard slap bruised a creature (player_commands::CreatureBruise)
	virtual void DoBruise(PlayerNames /*player*/, const player_commands::CreatureBruise& /*bruise*/) {}
	/// The turn of a stroke or a slap that a player sent in a network game: the creature plays its reaction
	virtual void DoReaction(PlayerNames /*player*/, const player_commands::CreatureReaction& /*reaction*/) {}
	/// Whether the hand was put on the creature by a command, so the button letting go leaves it held
	[[nodiscard]] virtual bool IsHeldByCommand() const = 0;
	/// The creature the interface picked under the cursor at the last frame drawn, which the hand would take hold of
	[[nodiscard]] virtual std::optional<entt::entity> CreatureUnderCursor() const = 0;
	/// How the creature has been treated since the hand took hold, from -1 to 1
	[[nodiscard]] virtual float GetFeedbackSum() const = 0;
	/// While held to a creature the same, and after letting go the sum it let go with, until it takes hold again
	[[nodiscard]] virtual float GetLastFeedbackSum() const = 0;
	/// Whether letting go just told the creature something too slight to count, which forces the hand's "Focus Creature"
	/// tooltip once; asking takes it
	virtual bool TakeFocusTip() = 0;
	/// At the turn of a network game: each creature that a hand holds looks at the hand that its player last reported,
	/// so that every computer has the same look. A single game sets the look from the hand in each frame.
	virtual void LookAtReportedHand() {}
};

} // namespace openblack::ecs::systems
