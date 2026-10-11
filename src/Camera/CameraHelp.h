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

#include <optional>

#include "CameraDrag.h"
#include "Input/BindableActions.h"

/// What the camera lets the player do, as the game's scripts allow it: the camera help's features. The tutorials take
/// most of them away and give them back one by one, and can have the camera tilt itself to a set pitch and keep to a
/// set height over the land. Pure, tested on its own.
namespace openblack::camera_help
{

namespace feature
{
using camera_drag::feature::k_Pitch;
using camera_drag::feature::k_Rotate;
using camera_drag::feature::k_Strafe;
using camera_drag::feature::k_Zoom;
/// A double click flies the camera to the thing or place clicked (near a creature fight's arena, to watch the fight).
/// While it is taken away, a double click waits for it.
constexpr uint32_t k_DoubleClick = 0x10;
/// Turning the camera around the mouse with the middle button, and with both buttons
constexpr uint32_t k_RotateAroundMouse = 0x20;
/// The camera tilts itself to a set pitch and keeps to a set height
constexpr uint32_t k_AutoPitch = 0x40;
} // namespace feature

/// The pitch the camera tilts itself to, and its height over the land, until a script sets them
constexpr float k_DefaultAutoPitch = 0.523599f;
constexpr float k_DefaultAutoPitchHeight = 75.0f;
/// How far from the camera the hand reaches, until a script's interface level sets it nearer
constexpr float k_DefaultHandReach = 1800.0f;

/// The camera help's state
struct CameraHelp
{
	uint32_t features {camera_drag::k_DefaultFeatures};
	float autoPitch {k_DefaultAutoPitch};
	float autoPitchHeight {k_DefaultAutoPitchHeight};
	/// The camera's keys and buttons (zooming, moving, tilting, turning), and the keys that fly the camera to the temple,
	/// the creature and the realm
	bool cameraKeys {true};
	bool placeKeys {true};
	/// How far from the camera the hand reaches
	float handReach {k_DefaultHandReach};

	/// The features under the mask become those of the value
	void Enable(uint32_t value, uint32_t mask) { features = (features & ~mask) | value; }
	/// The camera tilts itself to the pitch and keeps to the height, or stops doing so
	void SetAutoPitch(float pitch, float height, bool on);
	/// As a new land opens: every feature, and the height of the self-tilting camera back to its first
	void ResetForNewLand();
	/// The features, keys and the hand's reach as a script's interface level sets them; nothing for a level the game
	/// doesn't know
	bool SetInterfaceLevel(int32_t level);
	/// The options screen's actions the keys allowed leave out
	[[nodiscard]] input::BindableActionMap BlockedActions() const;
};

/// The features in a creature fight the camera watches: a double click flies nowhere, and waits until the fight is over
[[nodiscard]] constexpr uint32_t DuringFight(uint32_t features)
{
	return features & ~feature::k_DoubleClick;
}

/// How strongly the self-tilting camera tilts this frame towards its pitch, as pitch input: nothing within a hundredth
/// of the way there
[[nodiscard]] std::optional<float> AutoPitchInput(float targetPitch, float pitch, float deltaSeconds);

/// Whether the self-tilting camera goes back over the place it stood at the start of the frame, at its height over the
/// land, keeping the way it now looks: when it tilted itself this frame, or when the land isn't gripped and nothing is
/// asked of the camera (no moving, turning, tilting or zooming, and no turning round the mouse)
[[nodiscard]] constexpr bool KeepsToItsPlace(bool tiltedItself, bool landGripped, bool asked)
{
	return tiltedItself || (!landGripped && !asked);
}

} // namespace openblack::camera_help
