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
/// hand takes hold of the creature to stroke and slap it: the player's own creature, or the creature of the god who
/// guides them (Khazar), but no other god's.
namespace openblack::creature_hand
{

/// Whether the player's hand may take hold of a creature to stroke and slap it: one the player owns, or one belonging
/// to the god who guides the player. Nobody's creatures and other gods' creatures are left alone.
[[nodiscard]] bool MayTouch(PlayerNames player, PlayerNames owner, bool guidesCreature);

/// A press of the right button let go sooner than this, before the hand had time to stroke, is a click rather than a
/// hold. It is as long as the hand must rest on the body before it strokes, so a press can't be both.
constexpr float k_ClickMaxMs = creature_feedback::k_StrokeHoldMs;
/// Whether a press of the right button on a creature was a click: let go quickly, having neither stroked nor slapped
[[nodiscard]] bool IsClick(float heldMs, bool strokedOrSlapped);

} // namespace openblack::creature_hand
