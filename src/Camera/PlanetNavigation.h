/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// EXPERIMENT (not for merge): the rules that move the camera over the world's ball. Each rule is pure and works in
/// doubles relative to the eye or the ball's middle, so it keeps its precision millions of units from the map.
///
/// - Low down, the hand drags the curved sea: the point that the hand grips stays under the cursor.
/// - In space, the camera turns round the ball's middle as a globe turns under the hand.
/// - Between the two, the view blends by the height over the ball.
namespace openblack::planet_nav
{

/// The orbit view starts to blend in at this height over the ball, and is complete at k_OrbitFull. Under
/// k_OrbitFrom the world's flat cap can move with the eye, so only above it is the ball fixed under the map.
inline constexpr float k_OrbitFrom = 200000.0f;
inline constexpr float k_OrbitFull = 400000.0f;

/// Over the sea past the island, the camera can go across the ground at most k_FarSeaLimit from the map's middle. There
/// the local camera keeps a precision of a few centimetres. Far round the planet (see Settle) the camera keeps to
/// k_NearSeaRadius round the place where it came down.
inline constexpr float k_NearSeaRadius = 20000.0f;
inline constexpr float k_FarSeaLimit = 1.0e6f;

/// The world's ball: its middle under the middle of the map, a planet's radius under the sea's level
struct Ball
{
	glm::vec3 centre {0.0f};
	float radius {1.0f};
};

/// The ball for a sea at `seaLevel`, under the map's middle `mapMiddle` (x and z), with the radius `radius`
[[nodiscard]] Ball WorldBall(float seaLevel, glm::vec2 mapMiddle, float radius);

/// Over the planet a notch of the wheel (60 units of zoom) changes the height over the ball by this factor
inline constexpr float k_OrbitZoomPerNotch = 1.25f;

/// The height over the ball after a zoom over the planet: `zoom` is in the wheel's units, positive away from the ball
[[nodiscard]] float OrbitZoomHeight(float height, float zoom);

/// The camera goes no higher over the ball than this: four of the planet's radii, where the planet is about 28 degrees
/// across
inline constexpr float k_OrbitHighest = 2.4e7f;

/// The eye kept at most k_OrbitHighest over the ball, moved straight towards the ball's middle. None when it is
/// lower.
[[nodiscard]] std::optional<glm::vec3> KeepUnderOrbitTop(const Ball& ball, glm::vec3 eye);

/// The height of a point over the ball, with no loss of the small terms when the point is near the ball
[[nodiscard]] float HeightOver(const Ball& ball, glm::vec3 point);

/// How much of the orbit view shows at a height over the ball: 0 under k_OrbitFrom, 1 over k_OrbitFull, and a smooth
/// blend between
[[nodiscard]] float OrbitShare(float height);

/// Where a ray from the eye meets the ball, as the distance along the ray. None when the ray does not meet it or the
/// eye is in the ball.
[[nodiscard]] std::optional<float> BallDistance(const Ball& ball, glm::vec3 eye, glm::vec3 direction);

/// The direction from the ball's middle to the point that a ray from the eye grips. A ray that does not meet the ball
/// grips the point on the ray nearest to the middle, so the grip moves on smoothly past the edge of the ball.
[[nodiscard]] glm::vec3 GripOnBall(const Ball& ball, glm::vec3 eye, glm::vec3 direction);

/// The shortest turn that moves the unit direction `from` to the unit direction `to`
[[nodiscard]] glm::quat GreatCircleTurn(glm::vec3 from, glm::vec3 to);

/// A camera: where the eye is, the point that it looks at, and the direction of the top of the screen
struct Pose
{
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	glm::vec3 up {0.0f, 1.0f, 0.0f};
};

/// The camera turned about the ball's middle
[[nodiscard]] Pose TurnAbout(const Ball& ball, const glm::quat& turn, const Pose& pose);

/// The globe drag: the turn about the ball's middle that puts the gripped point (a direction from the middle) under
/// the cursor. `rayNow` is the ray through the cursor now, from the camera as it was when the hand gripped.
[[nodiscard]] glm::quat GlobeDrag(const Ball& ball, glm::vec3 eyeAtGrip, glm::vec3 gripped, glm::vec3 rayNow);

/// The turn about the line from the ball's middle through the eye, by `angle` radians: the view turns on the screen
/// and the eye stays where it is
[[nodiscard]] glm::quat SpinAbout(const Ball& ball, glm::vec3 eye, float angle);

/// The orbit view from a camera: the same eye, looking at the ball's middle. The top of the screen is the camera's own
/// top made square to the new line of sight, so the view does not roll when it changes.
[[nodiscard]] Pose OrbitPose(const Ball& ball, const Pose& ground);

/// The top of the screen of a camera that looks from `eye` at `focus` with the world's up as a guide
[[nodiscard]] glm::vec3 ScreenUp(glm::vec3 eye, glm::vec3 focus, glm::vec3 guide);

/// The camera between the ground view (share 0) and the orbit view (share 1). The eye stays where it is, and the way
/// that the camera faces turns by the shortest turn.
[[nodiscard]] Pose Blend(const Pose& ground, const Pose& orbit, float share);

/// Where the camera comes down out of the orbit view. On the way down the turn of the world stays, so the camera comes
/// down to the point under the middle of the screen. When the orbit share gets to 0, the camera settles there:
///
/// - Near the island (the point is at most k_FarSeaLimit from the map's middle across the ball), the local camera
///   takes the world's camera as it is and the turn is gone. The camera's top then eases to the world's up.
/// - Farther round the planet, the turn stays: the frame of that point is the camera's own frame, and the island is
///   out of sight. The local camera moves to `spot`, k_AnchorSpotDistance from the map's middle, so that nothing of the
///   island's maps is under it. The world's camera does not change.
struct Settled
{
	Pose local;
	glm::quat turn {1.0f, 0.0f, 0.0f, 0.0f};
	/// The place of the local camera across the ground, far round the planet; none near the island
	std::optional<glm::vec2> spot;
};
inline constexpr float k_AnchorSpotDistance = 70000.0f;
[[nodiscard]] Settled Settle(const Ball& ball, const glm::quat& turn, const Pose& local);

/// A camera that a command puts in the world's frame, as Settle puts it: far round the planet and under k_OrbitFrom,
/// the local camera near the map with the frame of that point. None near the island or high up, where the world's frame
/// is the camera's frame.
[[nodiscard]] std::optional<Settled> SettleWorldPose(const Ball& ball, const Pose& world);

/// A point turned about `centre` by `turn`, in doubles, so it keeps its precision far from the map
[[nodiscard]] glm::vec3 TurnPointAbout(glm::vec3 centre, const glm::quat& turn, glm::vec3 point);

/// A point turned about the ball's middle by `frame`: from the frame that the camera works in to the world's frame
[[nodiscard]] glm::vec3 ToWorldFrame(const Ball& ball, const glm::quat& frame, glm::vec3 point);

/// The distance across the ball from the map's middle to the point under the eye
[[nodiscard]] float ArcFromMap(const Ball& ball, glm::vec3 eye);

/// Where a ray from `eye` along `direction` meets the curved sea, as the distance along the ray; none when it does not
using SeaDistance = std::function<std::optional<float>(glm::vec3 eye, glm::vec3 direction)>;

/// The drag of the curved sea low down: the move across the ground of the eye that puts the gripped point under the
/// cursor. The view's turn stays the same. `ray` is the ray through the cursor now, from the eye where it was when the
/// hand gripped. The curved world moves its flat cap with the eye, so the rule moves the eye and measures again.
/// None when the ray does not meet the sea.
[[nodiscard]] std::optional<glm::vec2> CurvedSeaDrag(const SeaDistance& sea, glm::vec3 eyeAtGrip, glm::vec3 gripped,
                                                     glm::vec3 ray);

/// The distance along a ray from the eye to the curved sea as the frame draws it (the flat cap and the ball, see
/// graphics::atmosphere::MapCap), for a map `mapSize` units on each side. `islandFar` tells that the frame is far
/// round the planet (Settle). None when the ray does not meet it.
[[nodiscard]] std::optional<float> DrawnSeaDistance(glm::vec3 eye, glm::vec3 direction, float seaLevel, float mapSize,
                                                    bool islandFar = false);

/// A line that the picks follow, from far out round the planet: it starts where it comes into the ball of `reach`
/// round the map's middle (at the sea's level), so the line tests do not cross millions of cells. A line from inside
/// the ball stays as it is. None when the line does not come into the ball.
struct Line
{
	glm::vec3 from {0.0f};
	glm::vec3 to {0.0f};
};
inline constexpr float k_PickReach = 30000.0f;
[[nodiscard]] std::optional<Line> LineNearMap(glm::vec3 from, glm::vec3 to, glm::vec3 mapMiddle, float reach);

/// The camera across the ground kept inside a disc of `radius` round `middle`; none when it is inside
[[nodiscard]] std::optional<glm::vec2> KeepInsideDisc(glm::vec2 point, glm::vec2 middle, float radius);

} // namespace openblack::planet_nav
