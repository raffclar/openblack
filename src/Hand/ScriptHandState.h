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

#include "Camera/CameraDrag.h"

/// What the hand is doing, as the scripts ask for it. The camera lessons wait for the player to grip the land, and
/// don't replay a demonstration while the player is zooming with the land gripped.
namespace openblack::hand::script_state
{

/// The numbers the scripts compare the hand's state with
enum class State : int32_t
{
	/// Nothing in particular: the hand is out over the land
	Normal = 3,
	/// The hand grips the land
	GripLandscape = 20,
	/// A number the scripts wait for while turning the camera, which the game never gives
	RotateLandscape = 21,
	/// The hand grips the land and the other button is held too, zooming
	ZoomLandscape = 22,
	/// The hand is gone while a script holds the interface, as in a cinema
	Off = 25,
	/// The player is in the temple
	InsideTemple = 30,
};

/// What the hand's state is made from
struct Facts
{
	/// The player has the interface, rather than a script
	bool interfaceActive {true};
	bool inTemple {false};
	/// The hand grips the land with the select button
	bool gripsLand {false};
	/// The other button is held too
	bool otherButton {false};
	/// What the camera lets the player do
	uint32_t cameraFeatures {camera_drag::k_DefaultFeatures};
};

/// The hand's state. Holding the land gives a grip, or a zoom with the other button held as well while zooming is
/// allowed.
// TODO(advisor-point-things): the hand's other states (over an object, holding one, casting, the leash, the bubble,
// fights) aren't told apart yet and give Normal; the scripts only ask after the land grip and the zoom
[[nodiscard]] constexpr State StateOf(const Facts& facts)
{
	if (!facts.interfaceActive)
	{
		return State::Off;
	}
	if (facts.inTemple)
	{
		return State::InsideTemple;
	}
	if (facts.gripsLand)
	{
		const bool zooming = facts.otherButton && (facts.cameraFeatures & camera_drag::feature::k_Zoom) != 0;
		return zooming ? State::ZoomLandscape : State::GripLandscape;
	}
	return State::Normal;
}

} // namespace openblack::hand::script_state
