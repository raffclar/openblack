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
/// hand takes hold of the creature to stroke and slap it, which the game allows for any god's creature, not only the
/// player's own: Khazar's, Lethys's or another player's.
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
};

/// Whether the hand may take hold of a creature to stroke and slap it. The game lets the hand hold any creature that
/// belongs to a player, whichever player, unless it is asleep or frozen. Ogres never can be.
[[nodiscard]] bool MayHold(const Holdable& creature);
/// Whether the hand shows its tooltip for interacting with a creature it is over, awake: the player's own creature
/// anywhere, another player's only while the hand is within the player's influence (asked at the creature)
[[nodiscard]] bool ShowsInteractTip(PlayerNames player, PlayerNames owner, bool asleep, bool handInInfluence);

/// A press of the right button let go sooner than this is a click rather than a hold. The hand can't stroke or slap
/// that soon.
constexpr float k_ClickMaxMs = 450.0f;
/// Whether a press of the right button on a creature was a click: let go quickly, having neither stroked nor slapped
[[nodiscard]] bool IsClick(float heldMs, bool strokedOrSlapped);

} // namespace openblack::creature_hand
