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
/// don't replay a demonstration while the player is turning or zooming with the land gripped.
namespace openblack::hand::script_state
{

/// The numbers the scripts compare the hand's state with
enum class State : int32_t
{
	/// Nothing in particular: the hand is out over the land
	Normal = 3,
	/// The hand grips the land
	GripLandscape = 20,
	/// The hand grips the land, the other button is held too, and the camera turns
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
	/// The camera's controls asked for a turn of more than a hundredth in this turn
	bool cameraTurned {false};
	/// What the camera lets the player do
	uint32_t cameraFeatures {camera_drag::k_DefaultFeatures};
};

/// The hand's state. Holding the land gives a grip. With the other button held as well, it gives a turn when the
/// camera turns while turning is allowed, else a zoom while zooming is allowed.
// TODO(advisor-point-things): the hand's other states (over an object, holding one, casting, the leash, the bubble,
// fights) aren't told apart yet and give Normal; the scripts only ask after the land grip, the turn and the zoom
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
		if (!facts.otherButton)
		{
			return State::GripLandscape;
		}
		if (facts.cameraTurned && (facts.cameraFeatures & camera_drag::feature::k_Rotate) != 0)
		{
			return State::RotateLandscape;
		}
		const bool zooming = (facts.cameraFeatures & camera_drag::feature::k_Zoom) != 0;
		return zooming ? State::ZoomLandscape : State::GripLandscape;
	}
	return State::Normal;
}

} // namespace openblack::hand::script_state
