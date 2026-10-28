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

#include <algorithm>
#include <deque>

#include <entt/entity/entity.hpp>

#include "Creature/CreatureFeedback.h"

/// The reactions to strokes and slaps that the hand of this computer sent, which the world did not do yet. The hand
/// counts a stroke or a slap only when the creature takes the reaction. A reaction that waits for its turn plays at the
/// start of that turn, so the hand counts it as a reaction that plays from its start.
namespace openblack::creature_hand
{

/// The reactions that the hand of this computer sent, in the order that the turns do them. This is input of this
/// computer only: the world does not read it.
class SentReactions
{
public:
	/// The hand sent a reaction. `counted` is true when the hand counted the stroke or the slap.
	void Sent(entt::entity creature, bool counted) { _sent.push_back({.creature = creature, .counted = counted}); }

	/// A turn did the oldest reaction that the hand sent
	void Done()
	{
		if (!_sent.empty())
		{
			_sent.pop_front();
		}
	}

	/// Forget all the reactions, when no command waits for a turn
	void Clear() { _sent.clear(); }

	/// Whether a counted reaction of the creature waits for its turn
	[[nodiscard]] bool Waiting(entt::entity creature) const
	{
		return std::ranges::any_of(_sent, [creature](const Entry& sent) { return sent.counted && sent.creature == creature; });
	}

	[[nodiscard]] bool Empty() const { return _sent.empty(); }

private:
	struct Entry
	{
		entt::entity creature {entt::null};
		bool counted {false};
	};
	std::deque<Entry> _sent;
};

/// Whether the creature takes a new reaction now. `bodyTakes` tells whether its body takes the reaction now. A reaction
/// that waits plays from its start. The new reaction interrupts it only if the new reaction interrupts an action at its
/// start.
[[nodiscard]] constexpr bool TakesReaction(bool reactionWaiting, double interruptsAfter, bool bodyTakes)
{
	return bodyTakes && (!reactionWaiting || !creature_feedback::RefusesInterrupt(0.0f, interruptsAfter));
}

/// What the hand sends for a stroke or a slap
enum class ReactionSend : uint8_t
{
	/// The hand sends nothing
	None,
	/// The turn plays the action and pulls the face, if the reaction has a face
	Whole,
	/// The turn pulls only the face
	FaceOnly,
};

/// What the hand sends for a reaction. A reaction that the creature takes plays whole. If the creature refuses it, the
/// creature pulls only its face. A refused reaction with no face does nothing in the world, so the hand sends nothing.
[[nodiscard]] constexpr ReactionSend ReactionToSend(bool takes, bool pullsFace)
{
	if (takes)
	{
		return ReactionSend::Whole;
	}
	return pullsFace ? ReactionSend::FaceOnly : ReactionSend::None;
}

} // namespace openblack::creature_hand
