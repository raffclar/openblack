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

#include "Enums.h"

/// What the right mouse button does on a creature. Pressed on the player's own creature (or, once there are alliances,
/// an ally's) the hand takes hold of it to stroke and slap it; let go within a moment, the press was a click, which works
/// the player's leash key instead.
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

/// Whether the creature itself lets the hand take hold of it: it belongs to a player, is awake and is not frozen. Ogres
/// never can be. Whose hand may hold it is TakesPress's test
[[nodiscard]] bool MayHold(const Holdable& creature);

/// Whether a creature of one player counts as friendly to another, whose hand may then hold it. (pending) allies count
/// too once the game has alliances
[[nodiscard]] bool IsFriendlyTo(PlayerNames owner, PlayerNames player);

/// What an empty hand's press knows about the object it was pressed on
struct PressFacts
{
	bool isCreature {false};
	/// The creature lets the hand hold it (MayHold)
	bool mayHold {false};
	bool cannotBePickedUp {false};
	/// The hand already holds a creature or is starting to
	bool locked {false};
	/// It belongs to the player pressing, or to a friend of theirs (IsFriendlyTo)
	bool friendly {false};
};
/// Whether an empty hand's press on the object takes hold of a creature: only one the player or a friend owns, in or out
/// of the player's influence
[[nodiscard]] bool TakesPress(const PressFacts& facts);

/// The hand let go of a creature sooner than this after it began holding it, in camera time, so the press was a click
constexpr uint32_t k_ClickMaxCameraMs = 450;
/// Whether the hand, holding a creature this long in camera time, was only clicked on it
[[nodiscard]] bool IsClick(uint32_t heldCameraMs);

/// Whether the hand over a creature shows the interact tooltip: over the player's own creature always, over another
/// player's when the hand is in the player's influence, and never over a creature of no player
[[nodiscard]] bool ShowsInteractTip(bool isMine, bool hasPlayer, bool inInfluence);

} // namespace openblack::creature_hand
