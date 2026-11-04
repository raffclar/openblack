/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/vec3.hpp>

#include "Camera/Camera.h"
#include "Camera/CameraZoomer.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The camera of a world file of the game. The game reads the whole camera when it loads: the eye, the point that it
// looks at and the lens, each with the glide that moves it, then the mode of the camera. So a glide that the save
// stopped continues after the load.
namespace openblack::ecs::vanilla_camera_import
{

/// The file keeps each glide as 12 numbers of 4 bytes
inline constexpr size_t k_GlideBytes = 48;

/// One coordinate of the camera as it glides to where it was sent, as the file keeps it
struct Glide
{
	float value {0.0f};
	float target {0.0f};
	float targetSpeed {0.0f};
	float speed {0.0f};
	/// The seconds of the glide that went by, and the seconds that the glide takes
	float elapsed {0.0f};
	float duration {0.0f};
	/// Where the glide started and its speed at the start
	float start {0.0f};
	float startSpeed {0.0f};
	/// The acceleration at the start, and how it changes and how that changes
	float c2 {0.0f};
	float c3 {0.0f};
	float c4 {0.0f};
};

/// The glide of one coordinate from the 48 bytes of the file. The game does not read the fifth number.
[[nodiscard]] Glide GlideOf(std::span<const uint8_t, k_GlideBytes> bytes);

/// The modes of the camera that the file keeps. The file keeps only the mode on top of the camera's modes.
enum class Mode : uint8_t
{
	/// The player's world camera. The file keeps no values for it.
	World,
	/// A camera that follows a thing. A script's camera is kept as this mode too.
	Follow,
	/// A camera that follows a thing and turns round it
	FollowAndTurn,
	/// A free camera that the keys move
	Free,
};

/// The values of a camera that follows a thing
struct Follow
{
	float heading {0.0f};
	float pitch {0.0f};
	float distance {0.0f};
	/// The share of the following's own glide time, for the camera that follows a thing
	float timeScale {0.0f};
	/// The speed of the turn, for the camera that turns round the thing
	float turnSpeed {0.0f};
	bool relativeHeading {false};
	bool useOffsets {false};
	glm::vec3 focusOffset {0.0f};
	glm::vec3 eyeOffset {0.0f};
	/// The number of the object of the thing in the file, 0 for none
	uint32_t target {0};
	bool autoReposition {false};
};

/// The camera of the file
struct ImportedCamera
{
	/// The x, y and z glides of the point that the camera looks at and of the eye
	std::array<Glide, 3> focus;
	std::array<Glide, 3> eye;
	Glide lens;
	/// The world camera lets the eye go below the land
	bool belowLand {false};
	Mode mode {Mode::World};
	/// The values of the mode, for the modes that follow a thing
	std::optional<Follow> follow;
};

/// The camera of the file. None when the file has no camera, or when its camera has values that do not read.
[[nodiscard]] std::optional<ImportedCamera> ReadCamera(const gamesave::VanillaWorld& world);

/// The mode of the camera of a mode number of the file. None for a number that the game does not write.
[[nodiscard]] std::optional<Mode> ModeOf(uint32_t number);

/// How the eye and the point that the camera looks at move, from the glides of the file. Each point glides from its
/// start, with its speed at the start, to its target, where it arrives at its target speed. The glide has gone on for
/// its seconds so far. None when the three coordinates of a point do not share the seconds of their glide, because the
/// camera times the three coordinates of a point together.
[[nodiscard]] std::optional<Camera::Motion> MotionOf(const std::array<Glide, 3>& eye, const std::array<Glide, 3>& focus);

/// The glide of the lens of the file
[[nodiscard]] camera::Zoomer LensOf(const Glide& lens);

/// Where the eye and the point that the camera looks at are now
[[nodiscard]] glm::vec3 ValueOf(const std::array<Glide, 3>& glides);

} // namespace openblack::ecs::vanilla_camera_import
