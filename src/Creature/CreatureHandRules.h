/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureFeedback.h"
#include "Enums.h"

/// What the right mouse button does on a creature. Clicked, it puts the leash on the player's own creature. Held, the
/// hand takes hold of the player's own creature to stroke and slap it, inside the player's influence or out. Another
/// god's creature is never held, even inside the influence (an allied god's would be, but allies aren't modelled).
namespace openblack::creature_hand
{

/// What the hand needs to know about a creature to take hold of it
struct Holdable
{
	/// The player it belongs to; nobody's creatures can't be held
	PlayerNames owner {PlayerNames::NEUTRAL};
	CreatureType species {CreatureType::Unknown};
	bool asleep {false};
	/// Frozen by the freeze miracle
	bool frozen {false};
	/// Under a script's control
	bool scriptControlled {false};
};

/// Whether a creature lets a hand take hold of it at all: one belonging to a player, not an ogre, not asleep, frozen or
/// under a script's control
[[nodiscard]] bool TakesHold(const Holdable& creature);
/// Whether a player's hand may take hold of a creature to stroke and slap it: only the player's own creature, wherever
/// it is, and only while it takes hold at all
[[nodiscard]] bool MayHold(const Holdable& creature, PlayerNames handPlayer);
/// Whether the hand, empty, shows its tooltip for interacting with a creature it is over. Never for a creature of no
/// player or one under a script's control. Out of the player's influence (asked at the creature) only for the player's
/// own creature. Within it for any player's, except one that would take hold while a hand already holds it; one that
/// wouldn't take hold (asleep, say) still shows it.
[[nodiscard]] bool ShowsInteractTip(PlayerNames player, const Holdable& creature, bool held, bool handInInfluence);

/// A press of the right button let go sooner than this is a click rather than a hold. The hand can't stroke or slap
/// that soon.
constexpr float k_ClickMaxMs = 450.0f;
/// Whether a press of the right button on a creature was a click: let go quickly, having neither stroked nor slapped
[[nodiscard]] bool IsClick(float heldMs, bool strokedOrSlapped);

} // namespace openblack::creature_hand
