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

#include <array>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>

#include <glm/vec3.hpp>

#include "CameraModel.h"
#include "CameraZoomer.h"

namespace openblack
{

namespace edt
{
struct EDTTrack;
} // namespace edt

namespace camera_track
{
class WayRunner;
} // namespace camera_track

namespace script_camera
{

/// The camera moves on by no more than a tenth of a second a frame
inline constexpr float k_MaxFrameSeconds = 0.1f;
/// Closer than this, squared, to where it is going, the camera has arrived
inline constexpr float k_ArrivedDistanceSquared = 0.001f;
/// The camera is kept within a sphere of 3500 around the middle of the world, at (2560, 0, 2560)
inline constexpr float k_WorldCentre = 2560.0f;
inline constexpr float k_WorldRadiusSquared = 1.225e7f;
/// Sent beyond it, it is sent back onto it instead, over 3 seconds
inline constexpr float k_WorldScale = 0.000285795948f;
inline constexpr float k_WorldReturnSeconds = 3.0f;
/// The camera is drawn at least a metre above the land
inline constexpr float k_GroundClearance = 1.0f;

/// A camera following a thing looks down on it at least this steeply, a thirteenth of a half turn
inline constexpr float k_FollowLowestPitch = std::numbers::pi_v<float> / 13.0f;
/// It stays between 2 and 1500 from it
inline constexpr float k_FollowNearest = 2.0f;
inline constexpr float k_FollowFarthest = 1500.0f;
/// Its glides take two seconds while the following begins, easing to one second after two seconds
inline constexpr float k_FollowStartSeconds = 2.0f;
inline constexpr float k_FollowEndSeconds = 1.0f;
inline constexpr float k_FollowBlendSeconds = 2.0f;
/// A thing is first followed from eight times its height
inline constexpr float k_FollowHeights = 8.0f;
/// Looking straight down from directly above, the pitch is this
inline constexpr float k_StraightDownPitch = 1.5393804f;
/// A script's camera follows from this far behind until a script says otherwise
inline constexpr float k_ScriptFollowSeconds = 0.2f;

/// Where the camera is drawn from and looks at
struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// The point a camera sent beyond the world's sphere is sent to instead: back onto the sphere along the same line from
/// its middle; nothing when it is within it
[[nodiscard]] std::optional<glm::vec3> PullIntoWorld(const glm::vec3& destination);
/// The camera as it is drawn: a metre aside and up when it is where it looks, then lifted, with what it looks at, to stay
/// a metre above the land under it
[[nodiscard]] View Drawn(const View& camera, const std::function<float(float x, float z)>& groundAt);

/// The way a camera at one point looks at another: its heading about the upright and how steeply it looks down. Over a
/// point that is all but straight below, the heading is 0 and the pitch the straight-down one.
struct HeadingAndPitch
{
	float heading;
	float pitch;
};
[[nodiscard]] HeadingAndPitch HeadingAndPitchFromPoints(const glm::vec3& origin, const glm::vec3& focus);
/// The point at a distance from another, the way a heading and pitch look back at it
[[nodiscard]] glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& point, float distance, float heading, float pitch);

/// What a camera following a thing needs to know of it
struct FollowedThing
{
	/// Where it is drawn, raised by half its height
	glm::vec3 point;
	/// The way it faces in game angles, for a thing that walks
	std::optional<uint16_t> gameAngle;
	/// Its height, from which the camera first follows it
	float height {0.0f};
};

/// How a camera follows a thing
struct FollowSettings
{
	float heading {0.0f};
	float pitch {0.0f};
	float viewingDistance {0.0f};
	/// How long its glides take, as a share of the following's own glide time; nothing for no glide
	float zoomTimeScale {k_ScriptFollowSeconds};
	/// The heading turns with the way the thing faces
	bool relativeHeading {true};
};

/// The seconds a following camera's glides take, some seconds after it began
[[nodiscard]] float FollowSeconds(float secondsSinceBegun, float zoomTimeScale);
/// Where a camera following a thing is sent: its heading and pitch from the thing, the pitch at least the lowest; the
/// heading turned with the thing when it is relative
[[nodiscard]] glm::vec3 FollowOrigin(const FollowedThing& thing, FollowSettings& settings);

} // namespace script_camera

/// The camera while a script has taken control of it: each of the six coordinates of where it is and what it looks at
/// glides to where the script sends it, and the player can't move it. The script hands it back when it is done, or
/// when its task stops.
class ScriptCameraModel final: public CameraModel
{
public:
	using GroundHeight = std::function<float(float x, float z)>;

	/// It takes over from where the camera is
	ScriptCameraModel(const glm::vec3& origin, const glm::vec3& focus, GroundHeight groundAt);
	~ScriptCameraModel() override;
	ScriptCameraModel(const ScriptCameraModel&) = delete;
	ScriptCameraModel& operator=(const ScriptCameraModel&) = delete;
	ScriptCameraModel(ScriptCameraModel&&) = delete;
	ScriptCameraModel& operator=(ScriptCameraModel&&) = delete;

	/// The camera is put somewhere, still
	void SetOrigin(const glm::vec3& origin);
	void SetFocus(const glm::vec3& focus);
	/// The camera glides somewhere from where it is, arriving still after some seconds; at once in under a thousandth
	void MoveOrigin(const glm::vec3& origin, float seconds);
	void MoveFocus(const glm::vec3& focus, float seconds);
	/// Whether both where it is and what it looks at have arrived where they were sent; on a track, whether the track's
	/// time has run out
	[[nodiscard]] bool Arrived() const;

	/// The camera runs one of the camera editor's tracks from its start: each frame it is put where the track's camera is
	/// after the time run so far, looking where the track's look-at way is at the same point of its curve. It stops
	/// looking at a thing; any following goes on. With no track it only stops looking at a thing.
	void RunTrack(std::shared_ptr<const edt::EDTTrack> track);
	/// Whether it is running a track
	[[nodiscard]] bool OnTrack() const { return _track != nullptr; }
	/// A track runs on the game's time, not the camera's: the frame's game time is passed on before the frame's update,
	/// so that it holds while the game is paused
	void PassGameTime(std::chrono::milliseconds gameTime);

	/// Finds a followed thing now; nothing once it is gone
	using ThingLookup = std::function<std::optional<script_camera::FollowedThing>()>;
	/// The camera is placed behind a thing at a distance, the heading and pitch it looks at it now, and follows it and
	/// looks at it
	void FollowAndLookAt(ThingLookup thing, float distance);
	/// The camera is placed behind a thing, from eight times its height, and follows it
	void Follow(ThingLookup thing);
	/// The camera looks at a thing and keeps looking at it
	void LookAt(ThingLookup thing);
	/// A script's settings for the following: its distance, glide time, heading and whether the heading turns with the
	/// thing
	void SetFollowSettings(float distance, float zoomTimeScale, float heading, bool relativeHeading);

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	void HandleActions(std::chrono::microseconds dt) override;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override;
	[[nodiscard]] glm::vec3 GetTargetFocus() const override;
	[[nodiscard]] std::chrono::seconds GetIdleTime() const override;

private:
	using Zoomers = std::array<camera::Zoomer, 3>;

	/// The camera is moved at once to where following and looking at the things puts it
	void SnapToFollowed();
	/// Sends the camera after the things it follows and looks at
	void UpdateFollowing();
	void StopFollowing();
	void StopLookingAt();
	void StopTrack();
	/// Puts the camera where its track has got to
	void UpdateTrack();

	Zoomers _origin;
	Zoomers _focus;
	GroundHeight _groundAt;
	ThingLookup _followed;
	ThingLookup _lookedAt;
	script_camera::FollowSettings _follow;
	/// Seconds since the script took the camera, or since it last began following
	float _seconds {0.0f};
	/// The track it runs, the runner along the track's camera way, and the game time run on it
	std::shared_ptr<const edt::EDTTrack> _track;
	std::unique_ptr<camera_track::WayRunner> _trackRunner;
	std::chrono::milliseconds _trackTime {0};
};

} // namespace openblack
