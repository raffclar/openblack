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

#include "Creature/CreatureFeedback.h"
#include "Enums.h"

/// What the right mouse button does on a creature. Clicked, it puts the leash on the player's own creature. Held, the
/// hand takes hold of the player's own creature to stroke and slap it, inside the player's influence or out. The hand
/// takes hold of the creature of an allied god in the same way. The creature of another god is never held, also inside
/// the influence.
namespace openblack::creature_hand
{

/// What the hand needs to know about a creature to take hold of it
struct Holdable
{
	/// The player it belongs to; nobody's creatures can't be held
	PlayerNames owner {PlayerNames::NEUTRAL};
	CreatureType species {CreatureType::Unknown};
	/// Frozen by the freeze miracle
	bool frozen {false};
	/// Under a script's control
	bool scriptControlled {false};
	/// Out cold in a faint, until it comes round
	bool fainting {false};
	/// A script made it a thing that the hand cannot pick up
	bool cannotBePickedUp {false};
};

/// Whether a creature lets a hand take hold of it at all: one belonging to a player, not an ogre, not frozen, not out
/// cold in a faint and not under a script's control. A creature that sleeps takes hold.
[[nodiscard]] bool TakesHold(const Holdable& creature);
/// Whether a player's hand may take hold of a creature to stroke and slap it: only the player's own creature, or the
/// creature of a god that is the ally of the player (allied), wherever it is. The creature must also take hold at all,
/// and a script must not stop the hand from picking it up.
[[nodiscard]] bool MayHold(const Holdable& creature, PlayerNames handPlayer, bool allied = false);
/// Whether the hand, empty, shows its tooltip for interacting with a creature it is over. Never for a creature of no
/// player or one under a script's control. Out of the player's influence (asked at the creature) only for the player's
/// own creature. Within it for any player's, except one that would take hold while a hand already holds it; one that
/// does not take hold (frozen, for example) still shows it.
[[nodiscard]] bool ShowsInteractTip(PlayerNames player, const Holdable& creature, bool held, bool handInInfluence);

/// How the hand shows its tooltip for interacting with a creature
enum class InteractTip : uint8_t
{
	/// No tooltip
	None,
	/// The words with no mouse
	WordsOnly,
	/// The words with the mouse of the Action button
	WithAction,
};
/// The tooltip for interacting with a creature (see ShowsInteractTip). Within the influence, over a creature that would
/// take hold, the mouse shows the Action button. In all other conditions the tooltip has no mouse.
[[nodiscard]] InteractTip InteractTipOf(PlayerNames player, const Holdable& creature, bool held, bool handInInfluence);

/// A press of the right button let go sooner than this is a click rather than a hold. The hand can't stroke or slap
/// that soon.
constexpr float k_ClickMaxMs = 450.0f;
/// Whether a press of the right button on a creature was a click: let go quickly, having neither stroked nor slapped
[[nodiscard]] bool IsClick(float heldMs, bool strokedOrSlapped);

/// Whether the hand lets go of the creature that it holds. The hand lets go when the right button comes up, or when the
/// mouse moves with the right button up after it was down. A hold that a command started lets go in the same way, so
/// the mouse can always release the creature.
[[nodiscard]] bool LetsGoOfCreature(bool rightLetGo, bool rightButtonUp);

/// What a press of the right button on a creature does, before the hand's own rules (MayHold) apply
enum class CreaturePress : uint8_t
{
	/// The hand tries to take hold of it
	Hold,
	/// The leash of the player's creature takes the press, to tie it to the creature
	Leash,
};
/// The press on the player's own creature always goes to the hand, also while the leash is on: the leash stays on and
/// the hand holds the creature as usual. With the leash on, a press on another creature goes to the leash.
[[nodiscard]] CreaturePress PressOn(bool playersOwnCreature, bool leashOn);

/// What a press of the right button on a creature starts, when the press goes to the hand
enum class PressStart : uint8_t
{
	/// The hand sends the hold of the creature at once
	Hold,
	/// The hand waits: let go soon, the press is a tap; held longer, the hand sends the hold
	WaitForTap,
	/// The press taps the creature at once. The leash held loose takes the tap as an order to act on the creature.
	TapAtOnce,
	/// The press does nothing to the creature
	Nothing,
};
/// The start of a press on a creature. The hand takes hold of a creature that it may hold (see MayHold). With the leash
/// of the player's creature held loose in the hand (not tied), the hand first waits for a tap. A press on a creature
/// that the hand may not hold taps it only while the leash is held loose.
[[nodiscard]] PressStart PressStartOn(bool mayHold, bool leashHeldLoose);

/// A press that waits for a tap sends the hold when it lasts longer than this many milliseconds
constexpr uint32_t k_TapWaitMs = 225;
/// Whether a press that waits for a tap, not let go, now sends the hold. The time of the press is in milliseconds, by the
/// clock but no more than the game turns allow.
[[nodiscard]] bool HoldsAfterWait(uint32_t elapsedMs);

/// Whether the turn that does a hold takes the creature that a press sent. The press asked all the questions of MayHold
/// before it sent the hold. The turn refuses only a creature that is not in the world any more, or one that a hand
/// already holds. So the turn holds a creature that changed after the press: frozen, out cold, under a script's control.
[[nodiscard]] bool TurnTakesHold(bool inWorld, bool heldByAHand);

} // namespace openblack::creature_hand
