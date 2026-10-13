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
#include <vector>

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

/// A camera sent to face a thing stands this far up from level, looking at it
inline constexpr float k_FacePitch = 0.1f;
/// A full turn, as the game's float holds it
inline constexpr float k_FullTurn = 6.28318548f;
/// A creature's facing, for the camera, is its body's turn about the upright plus a full turn less this
inline constexpr float k_CreatureFacingOffset = 2.5f;
/// Facings this many turns round or more are wrong, though the camera still uses them
inline constexpr float k_HighestFacing = 4.0f * k_FullTurn;

/// What a camera sent to face a thing needs to know of it
struct FacedThing
{
	/// Where the game holds it
	glm::vec3 position;
	float height;
	/// The way it faces, as the camera reads it: the heading the camera is put at from it
	float facing;
};
/// The facing of a creature whose body is turned by an angle about the upright, the way the drawn body turns
[[nodiscard]] float CreatureFacing(float bodyTurn);
/// Whether a thing's facing is too far round to be right
[[nodiscard]] bool IsWrongFacing(float facing);
/// Where a camera facing a thing from a distance stands and looks: at the middle of its height, from the heading it faces
/// (brought within a turn) and a little above level
[[nodiscard]] View FaceView(const FacedThing& thing, float distance);

/// The two-thing camera's glides take two seconds as it begins, easing to one after a second and a half
inline constexpr float k_DualStartSeconds = 2.0f;
inline constexpr float k_DualEndSeconds = 1.0f;
inline constexpr float k_DualBlendSeconds = 1.5f;
/// It begins an eighth of a turn round and an eighth of a half turn up; it looks down at least a thirteenth of a half
/// turn
inline constexpr float k_DualHeading = std::numbers::pi_v<float> / 4.0f;
inline constexpr float k_DualPitch = std::numbers::pi_v<float> / 8.0f;
inline constexpr float k_DualLowestPitch = std::numbers::pi_v<float> / 13.0f;
/// It stands as far back as the things are apart plus their radii, then 1.4 times the taller one's height further
inline constexpr float k_DualZoom = 1.0f;
inline constexpr float k_DualHeightFactor = 1.4f;
/// Things this close along both axes have no direction between them: the heading is kept as it is
inline constexpr double k_DualSameSpot = 0.01;
/// A thing with no model of its own (a marker, a group) counts as this wide
inline constexpr float k_DualNoModelRadius = 30.0f;

/// What the two-thing camera needs to know of each thing
struct DualThing
{
	/// Where the game holds it
	glm::vec3 position;
	float height;
	/// Its radius on the ground
	float radius;
};
/// How the two-thing camera is turned and how far it is zoomed out
struct DualSettings
{
	float heading {k_DualHeading};
	float pitch {k_DualPitch};
	float zoom {k_DualZoom};
};
/// The seconds the two-thing camera's glides take, some seconds after it began
[[nodiscard]] float DualSeconds(float secondsSinceBegun);
/// Where the two-thing camera is sent: it looks at the middle of the two things, raised by a quarter of their heights,
/// from its heading turned with the line between them; the pitch is first held at least the lowest
[[nodiscard]] View DualView(const DualThing& first, const DualThing& second, DualSettings& settings);
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
	/// A thing the camera follows or looks at: which it is, and how to find it
	struct Subject
	{
		uint32_t id;
		ThingLookup lookup;
	};
	/// The camera is placed behind a thing at a distance, the heading and pitch it looks at it now, and follows it and
	/// looks at it
	void FollowAndLookAt(Subject thing, float distance);
	/// The camera is placed behind a thing, from eight times its height, and follows it
	void Follow(Subject thing);
	/// The camera looks at a thing and keeps looking at it
	void LookAt(Subject thing);
	/// The thing the camera is about: the one it looks at, else the one it follows; nothing while a two-thing camera has
	/// taken over
	[[nodiscard]] std::optional<uint32_t> GetFocusedThing() const;
	/// A script's settings for the following: its distance, glide time, heading and whether the heading turns with the
	/// thing
	void SetFollowSettings(float distance, float zoomTimeScale, float heading, bool relativeHeading);

	/// Finds one of the two-thing camera's things now; nothing once it is gone
	using DualLookup = std::function<std::optional<script_camera::DualThing>()>;
	/// One of the two-thing camera's things: which it is, and how to find it
	struct DualSubject
	{
		uint32_t id;
		DualLookup lookup;
	};
	/// The two-thing camera takes over from the script's own: it keeps both things in view until let go. Asked again
	/// for the same two things while it has them, nothing changes. Asked for others, another one takes over on top of it.
	void StartDualView(DualSubject first, DualSubject second);
	/// The two-thing camera on top looks at two other things, its turn and zoom kept; nothing without one
	void UpdateDualView(DualSubject first, DualSubject second);
	/// The two-thing camera on top lets go, and the one below it, or the script's own, takes over again; false without
	/// one
	bool ReleaseDualView();
	/// Whether a two-thing camera has taken over: the script's own commands then don't reach the camera
	[[nodiscard]] bool InDualView() const { return !_duals.empty(); }
	/// Once a game turn, the two-thing cameras whose things have gone let go; until then they frame them where they were
	/// last seen. When the one on top lets go, the one below, or the script's own, takes over again.
	void LetGoOfGoneThings();

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	void HandleActions(std::chrono::microseconds dt) override;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	/// The camera stops at this position and focus. A thing that it follows or looks at, its track and a two-thing
	/// camera continue, and move it again on the next frame.
	void PlaceAt(glm::vec3 origin, glm::vec3 focus) override;
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
	/// Sends the camera after the two things of the two-thing camera on top
	void UpdateDual();

	Zoomers _origin;
	Zoomers _focus;
	GroundHeight _groundAt;
	ThingLookup _followed;
	ThingLookup _lookedAt;
	/// Which things they are
	std::optional<uint32_t> _followedId;
	std::optional<uint32_t> _lookedAtId;
	script_camera::FollowSettings _follow;
	/// Seconds since the script took the camera, or since it last began following
	float _seconds {0.0f};
	/// The track it runs, the runner along the track's camera way, and the game time run on it
	std::shared_ptr<const edt::EDTTrack> _track;
	std::unique_ptr<camera_track::WayRunner> _trackRunner;
	std::chrono::milliseconds _trackTime {0};
	/// The two-thing cameras that have taken over, the last on top
	struct Dual
	{
		DualSubject first;
		DualSubject second;
		script_camera::DualSettings settings;
		/// Where each was last seen
		std::optional<script_camera::DualThing> lastFirst;
		std::optional<script_camera::DualThing> lastSecond;
	};
	std::vector<Dual> _duals;
};

} // namespace openblack
