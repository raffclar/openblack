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

#include <functional>
#include <numbers>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "CameraLimits.h"
#include "Editor/EditorMath.h"

/// The maths of Creature Mode's camera, which locks onto a creature and follows it, free of the game so it can be tested
/// on its own. The camera keeps a heading round the creature, a pitch above it and a distance from it, looking at the
/// middle of its body. Each frame it sets off afresh for where that puts it, arriving a second later (two as it starts),
/// so it eases after the creature as it moves.
///
/// The keys turn it: the cursor keys with Shift turn it round the creature and up and down, with Ctrl round it and in
/// and out, and the wheel draws it in and out. The cursor keys alone hand the camera back. Ctrl and Shift together
/// swing it round to where the land falls away, so the view of the creature is clear of the hills.
namespace openblack::creature_follow
{

/// The heading round the creature (from +z, as an orbit's yaw), the pitch above it and the distance from its middle
using View = editor::Orbit;

/// The camera keeps this far from the creature, and the keys bring it no further out than the second
constexpr float k_MinDistance = 2.0f;
constexpr float k_MaxDistance = 1500.0f;
constexpr float k_MaxKeyDistance = 1000.0f;
/// It looks down on the creature at least this steeply, about 14 degrees
constexpr float k_MinPitch = 0.2416609824f;
/// Shift and the cursor keys tilt it between these, though it never goes lower than the least pitch above
constexpr float k_MinKeyPitch = -std::numbers::pi_v<float> / 4.0f;
constexpr float k_MaxKeyPitch = 1.3744468689f;
/// It starts this many of the creature's heights away, unless it is close to the creature already
constexpr float k_ViewingDistancePerHeight = 8.0f;
/// The camera counts as close when it is this near the creature across the land
constexpr float k_CloseAcross = 80.0f;
/// How long the camera takes to arrive where it sets off for: two seconds as it starts, down to one over two seconds
constexpr float k_StartEaseSeconds = 2.0f;
constexpr float k_EaseSeconds = 1.0f;
constexpr float k_EaseSettleSeconds = 2.0f;
/// The cursor keys move it as a mouse would move this many pixels a second
constexpr float k_KeyPixelsPerSecond = 400.0f;
/// A notch of the wheel, as the mouse counts it
constexpr float k_WheelNotch = 120.0f;

/// The middle of a creature of a height standing at a position, which the camera looks at
[[nodiscard]] glm::vec3 Focus(glm::vec3 position, float height);
/// How far from a creature of a height the camera starts
[[nodiscard]] float ViewingDistance(float height);

/// The heading and pitch of a point seen from a centre, as the camera keeps them; straight above it is looked down on
/// with no heading
struct HeadingPitch
{
	float heading {0.0f};
	float pitch {0.0f};
};
[[nodiscard]] HeadingPitch HeadingPitchOf(glm::vec3 point, glm::vec3 centre);

/// How far away the camera starts from the creature
enum class StartDistance : uint8_t
{
	/// The creature's viewing distance, or as far as the camera already is when it is close
	KeepWhenClose,
	/// Always the creature's viewing distance
	Viewing,
};
/// The points of a follow camera as offsets from the place of its thing: the point that it looks at and the eye. While
/// the camera has offsets, it keeps these two points fixed on the thing, and its heading and pitch have no effect.
struct Offsets
{
	glm::vec3 focus {0.0f};
	glm::vec3 eye {0.0f};
};

/// A follow camera as a saved game of the original game keeps it: its view, the share of its glide time, and whether
/// Ctrl and Shift together swing it clear. A script's camera can also turn the heading with its thing, and can keep the
/// camera at offsets from the thing.
struct Loaded
{
	View view;
	float timeScale {1.0f};
	bool autoReposition {true};
	bool relativeHeading {false};
	std::optional<Offsets> offsets;
};

/// The heading of a camera that turns with its thing, from the heading of the camera and the game angle of the thing
/// (2048 to the circle). The game takes the angle off the heading, less a quarter turn. Each step rounds to a float.
[[nodiscard]] float RelativeHeading(float heading, uint16_t gameAngle);
/// The distance of the camera from the point that it looks at, from its offsets. Each step rounds to a float, z first.
[[nodiscard]] float OffsetDistance(const Offsets& offsets);
/// The offsets with the eye moved along its line from the point that the camera looks at, to a distance from it. An
/// eye on that point stays on it.
[[nodiscard]] Offsets OffsetsAtDistance(const Offsets& offsets, float distance);

/// The view the camera starts with on a creature: as it is turned and tilted now, and the creature's viewing distance
/// away. With KeepWhenClose, it is as far as it already is when it is close, but no nearer than twice the creature's
/// height.
[[nodiscard]] View Start(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 creature, float height,
                         StartDistance start = StartDistance::KeepWhenClose);
/// The view kept within the follow's bounds: the distance between the least and the most, and no lower than the least
/// pitch
[[nodiscard]] View Clamped(View view);
/// How long the camera takes to arrive, some seconds after Creature Mode starts
[[nodiscard]] float EaseSeconds(float secondsInMode);
/// Where the camera stands for a view of a focus
[[nodiscard]] glm::vec3 Origin(glm::vec3 focus, const View& view);

/// The keys held this frame: across is the left and right cursor keys and along the up and down, each as pixels the
/// mouse would have moved (left and up negative); the wheel is in the mouse's counts, negative to draw the camera out
struct Keys
{
	float across {0.0f};
	float along {0.0f};
	float wheel {0.0f};
	bool shift {false};
	bool ctrl {false};

	[[nodiscard]] bool Moving() const { return across != 0.0f || along != 0.0f; }
};
/// The cursor keys' pixels for a frame of some seconds: -1, 0 or 1 a way
[[nodiscard]] Keys KeysFor(int across, int along, float seconds);

enum class KeyOutcome : uint8_t
{
	Stay,
	/// The cursor keys were pressed with neither Shift nor Ctrl: the player takes the camera back
	Leave,
};
/// Turns the view by the keys on a screen so many pixels wide. The wheel's zoom is applied twice, before and after the
/// cursor keys, as the game does.
[[nodiscard]] KeyOutcome Apply(View& view, const Keys& keys, float screenWidth);
/// As the other Apply, for a camera that can have offsets. The cursor keys stop the offsets: the view then takes the
/// heading, the pitch and the distance of the offsets, and the keys turn it. Without the cursor keys, the eye moves
/// along its line to the distance of the view.
[[nodiscard]] KeyOutcome Apply(View& view, std::optional<Offsets>& offsets, const Keys& keys, float screenWidth);

/// The height of the land at a point on it, x and z
using GroundHeight = std::function<float(glm::vec2)>;
/// The thing of a follow camera, as the camera reads it in a frame: its place, its height (the camera looks at the middle
/// of it), the game angle of a thing that walks (2048 to the circle) and whether the thing has a model in the world.
/// Offsets keep the camera on a thing with a model. On a thing with no model, such as a flock, the camera takes only the
/// distance of its offsets and follows the thing by its heading and pitch.
struct Target
{
	glm::vec3 position {0.0f};
	float height {0.0f};
	std::optional<uint16_t> gameAngle;
	bool hasModel {true};
};

/// The leader of a flock, as a follow camera reads it: its place in the world and its height
struct FlockLeader
{
	glm::vec3 position {0.0f};
	float height {0.0f};
};
/// The thing of a follow camera on a flock. The camera looks at the map place of the leader (x and z as a map position
/// holds them, the height of the leader's place), raised by half the height of the leader. A flock with no leader gives
/// its own place on the land, raised by its altitude. A flock has no model and does not walk.
[[nodiscard]] Target FlockTarget(const std::optional<FlockLeader>& leader, const map_coords::MapCoords& place,
                                 const GroundHeight& ground);

/// The distance the camera looks for a clear view at: nearer ones are taken most of the way out to 50 and further ones a
/// little way in towards 100
[[nodiscard]] float ClearingDistance(float distance);
/// The view swung round and tilted for a clear view of the creature, keeping its distance. The camera finds the heading
/// and the pitch as a double click flight finds them, at the distance for a clear view.
[[nodiscard]] View ClearView(View view, glm::vec3 focus, const camera_limits::Ground& ground);

} // namespace openblack::creature_follow
