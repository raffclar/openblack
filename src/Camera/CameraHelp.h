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

/// The camera features the player may use (EnabledFeatures) and the auto-pitch parameters. SET_INTERFACE_INTERACTION,
/// the script reboot clean-up and clearing the map write them.
///
/// (pending, camera owner: on its list) only the storage is ported: DefaultWorldCameraModel does not read the bits
/// yet. The readers in the original: the hand's camera state (bits 0x1, 0x2, 0x4), the hand's holding state (0x1, 0x2),
/// the interface's lock selection (0x1), two other interface paths (0x4, 0x2) and the player camera's features.
/// (pending) their save and load with the game.
namespace openblack::camera_help
{

/// The bits by the SCRIPT_INTERFACE_LEVEL names that set them (inferred: consistent over the 16 levels; pending until
/// the readers are read)
enum class Feature : int32_t
{
	Pitch = 0x01,          ///< JUST_PITCH
	Rotate = 0x02,         ///< JUST_ROTATE
	Zoom = 0x04,           ///< JUST_ZOOM (with 0x20; pending which zoom path each is)
	GrabLand = 0x08,       ///< JUST_GRAB
	DoubleClickFly = 0x10, ///< JUST_DOUBLE_CLICK_AND_DRAG (with 0x08)
	JustZoom = 0x20,       ///< JUST_ZOOM (with 0x04)
	AutoPitch = 0x40,      ///< SetAutoPitch
};
/// The bit of a Feature in EnabledFeatures
[[nodiscard]] constexpr int32_t Bit(Feature feature)
{
	return static_cast<int32_t>(feature);
}
/// EnabledFeatures' initial value, the cleared map's and SET_INTERFACE_INTERACTION(NORMAL)'s (0x80 / 0x100: only here,
/// pending)
constexpr int32_t k_NormalFeatures = 0x1BF;
/// The auto-pitch parameters' initial values (pi / 6, 75)
constexpr float k_DefaultAutoPitchAngle = 0.523599f;
constexpr float k_DefaultAutoPitchDistance = 75.0f;

/// EnabledFeatures = (EnabledFeatures & ~mask) | features. Every
/// SET_INTERFACE_INTERACTION level passes mask -1: the whole set is replaced (auto-pitch included)
void EnableCameraFeatures(int32_t features, int32_t mask);
/// EnableCameraFeatures(on ? 0x40 : 0, on ? 0 : 0x40), then the auto-pitch angle and distance
void SetAutoPitch(float angle, float distance, bool on);
[[nodiscard]] int32_t GetEnabledFeatures();
[[nodiscard]] bool IsFeatureEnabled(Feature feature);
[[nodiscard]] float GetAutoPitchAngle();
[[nodiscard]] float GetAutoPitchDistance();
/// The original's initial values (openblack: a new game, the tests)
void Reset();

} // namespace openblack::camera_help
