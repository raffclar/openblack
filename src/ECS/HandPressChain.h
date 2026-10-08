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

/// The hand's action press: which of its branches a press takes. The hand tries them in a fixed order and the first that
/// holds wins, so the order is the whole precedence. Wiki: docs/bw1-notes/hand-and-interface.md, "The action press".
namespace openblack::ecs::hand_press
{

/// The branch an action press takes, in the order the hand tries them
enum class Branch : uint8_t
{
	None,
	/// A bubble on the screen: the hand grips it
	ScreenObject,
	/// A field under the hand: its scooping
	Field,
	/// Any other object under the hand: a pile's scooping, a tap, a tug or a pick-up
	Hovered,
	/// An object that can only be tapped: an abode, a temple's entrance or a "Did you know?" sign
	TapOnly,
	/// The player's own creature: the hand takes hold of it
	Creature,
	/// The fish of a fish farm at the hand's point, with no creature under the cursor: their scooping
	FishFarm,
	/// A spell seed in the hand
	Seed,
	/// Anything else in the hand
	Held,
};

/// What the hand knows at an action press. None of them changes anything as it is read
struct Facts
{
	/// A screen object (a bubble) is under the empty hand, which is not gripping the land
	bool screenPress {false};
	bool held {false};
	bool hovered {false};
	bool hoveredIsField {false};
	bool gripping {false};
	/// The object under the cursor is available and can only be tapped
	bool tapOnlyCursorObject {false};
	/// The object under the cursor is an available creature, whoever's
	bool cursorIsCreature {false};
	/// The object under the cursor is a creature the hand takes hold of (creature_hand::TakesPress)
	bool creatureTakesPress {false};
	/// The hand's point is in the influence, over the shown fish of a fish farm. Not looked for over a creature
	bool fishFarmInInfluence {false};
	bool holdingSeed {false};
	/// The press that picked up what the hand holds is still down
	bool pickPressHeld {false};
	/// The hand holds a creature or is letting one go: no other press is taken until it is done
	bool creatureLockBusy {false};
};

/// The first branch whose conditions hold, in the hand's order; None when none does
[[nodiscard]] Branch Choose(const Facts& facts);

/// What the tap-only, creature and fish farm branches share: an empty hand over nothing, not gripping the land. Their own
/// facts need reading only when this holds, as the hand did before it chose this way
[[nodiscard]] constexpr bool EmptyHandOverNothing(const Facts& facts)
{
	return !facts.screenPress && !facts.held && !facts.hovered && !facts.gripping && !facts.creatureLockBusy;
}

} // namespace openblack::ecs::hand_press
