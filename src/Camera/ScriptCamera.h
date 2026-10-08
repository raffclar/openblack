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
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"

namespace openblack
{
class Camera;
struct CameraTrack;
class CameraWayRunner;
} // namespace openblack

/// The script camera (docs/bw1-notes/script-camera.md): the part of the game camera the script moves, and the script
/// camera mode.
///
/// - The game camera keeps its state in zoomers (Common/Zoomer.h): focus, position and the field of view (radians),
///   moved once a frame.
/// - START_CAMERA_CONTROL pushes a script mode; while it lives the player's mode does not move the camera and no other
///   task gets it. Releasing it gives the player a new mode that starts where the script left the camera.
/// - The script mode is a follow mode (no thing, time factor 0.2, "behind" on, no zoom): it can follow a thing with the
///   focus (FOCUS_FOLLOW) and with the position (POSITION_FOLLOW) once a frame.
/// - openblack has no mode stack: the player's mode is Camera + DefaultWorldCameraModel, and this module stands for
///   the camera's zoomers while a script mode lives. The player's Camera keeps its own Zoomer3 (GetOriginZoomer /
///   GetFocusZoomer): Begin takes them as they are and End hands them back, so the two sets behave as one.
namespace openblack::script_camera
{

/// The default field of view: 70 degrees
constexpr float k_DefaultFov = 1.2217305f;
/// The camera's seconds of a frame are at most 0.1
constexpr float k_MaxFrameSeconds = 0.1f;
/// A squared distance: closer than this to the destination counts as arrived
constexpr float k_ArrivedDistanceSquared = 0.001f;
/// The world's disc around (2560, 0, 2560); a destination farther than sqrt(1.225e7) = 3500 is pulled back to
/// d / (|d| * 0.000285796) (about 1/3499) in 3 s
constexpr float k_DiscCentre = 2560.0f;
constexpr float k_DiscRadiusSquared = 1.225e7f;
constexpr float k_DiscScale = 0.000285795948f;
constexpr float k_DiscSeconds = 3.0f;
/// The drawn camera stays 1 m above the landscape
constexpr float k_GroundClearance = 1.0f;
/// SET_CAMERA_LENS / MOVE_CAMERA_LENS / CAMERA_PROPERTIES: degrees to radians
constexpr float k_DegreesToRadians = 0.0174532924f;

/// The script mode's follow time factor
constexpr float k_ScriptFollowTimeFactor = 0.2f;
/// The follow distance is kept in 2 .. 1500
constexpr float k_FollowMinDistance = 2.0f;
constexpr float k_FollowMaxDistance = 1500.0f;
/// The follow pitch is at least this
constexpr float k_FollowMinPitch = 0.241660982f;
/// The seconds since the last mode change that end the slower start
constexpr float k_FollowSettleSeconds = 2.0f;
/// A thing's viewing distance is its height x 8
constexpr float k_ThingViewingDistanceFactor = 8.0f;
/// A wall hugger's game angle (2048 per circle) x 2 x pi / 2048 - pi / 2
constexpr float k_FollowAngleScale = 0.00153398083f;
constexpr float k_HalfPi = 1.57079637f;
/// HeadingAndPitchFromPoints: |dx| and |dz| below 0.01 (a double) give heading 0 and this near-vertical pitch
constexpr double k_VerticalEpsilon = 0.01;
constexpr float k_VerticalPitch = 1.53938043f;
/// HeadingAndPitchFromPoints: pi
constexpr float k_Pi = 3.14159274f;
/// Facing an object: the heading's limit 8 pi, 2 pi and the pitch 0.1
constexpr float k_FaceMaxHeading = 25.1327419f;
constexpr float k_TwoPi = 6.28318548f;
constexpr float k_FacePitch = 0.1f;

/// The dual camera (START_DUAL_CAMERA, CREATE_DUAL_CAMERA_WITH_POINT): heading pi / 4, pitch pi / 8 and the distance
/// factor 1 with two things, 1.2 with a point
constexpr float k_DualHeading = 0.785398185f;
constexpr float k_DualPitch = 0.392699093f;
constexpr float k_DualDistanceFactor = 1.0f;
constexpr float k_DualPointDistanceFactor = 1.20000005f;
/// The seconds of a dual camera's destination are 2 right after the mode change, down to 1 at 1.5 s and on
constexpr float k_DualSettleSeconds = 1.5f;
/// The second height of the mean when B is a point
constexpr float k_DualPointHeight = 1.0f;
/// The radius of a thing that is not an Object
constexpr float k_DualDefaultRadius = 30.0f;
/// The larger height is scaled by this
constexpr float k_DualHeightFactor = 1.39999998f;
/// A double: B straight over A keeps the heading
constexpr double k_DualFlatEpsilon = 0.0099999997764825821;

/// A dual camera ("Dual Cam") on the camera's mode stack
struct DualMode
{
	entt::entity a = entt::null;
	entt::entity b = entt::null;   ///< Not written for a point camera: only read with twoObjects
	glm::vec3 point {0.0f};        ///< The point of a point camera
	bool twoObjects = true;        ///< Set by UpdateDual
	float heading = k_DualHeading; ///< Radians
	float pitch = k_DualPitch;     ///< Radians, kept >= 0.241661 by the update
	float distanceFactor = k_DualDistanceFactor;
	bool alive = true; ///< Cleared when deleted
};

/// The state: the camera's zoomers, the script mode and its camera path
struct State
{
	Zoomer3 position;
	Zoomer3 focus;
	Zoomer fov; ///< Radians
	/// Seconds since the last mode change (0 on a mode switch, += the frame's camera seconds, 2 after PlaceFollowNow)
	float modeSeconds = 0.0f;
	/// The script mode is alive
	bool scriptMode = false;
	/// RUN_CAMERA_PATH: the track, null = none, and the runner of its position way. The focus way's runner is never
	/// evaluated by the path update
	std::shared_ptr<const CameraTrack> track;
	std::unique_ptr<CameraWayRunner> positionRunner;
	/// The game ms run along the path
	int32_t pathMs = 0;
	/// The follow mode's state. The pitch and the distance are not set by the constructors: (inferred) never read
	/// before PositionFollow / FocusAndPositionFollow / CAMERA_PROPERTIES write them, so 0 here
	entt::entity positionThing = entt::null;
	entt::entity focusThing = entt::null; ///< When null, the position thing is the focus thing
	float heading = 0.0f;                 ///< Radians
	float pitch = 0.0f;                   ///< Radians
	float distance = 0.0f;
	float timeFactor = k_ScriptFollowTimeFactor;
	bool behind = true; ///< The heading is relative to a wall hugger's angle
	/// STORE_CAMERA_DETAILS: the drawn position and the drawn focus; not the FOV
	glm::vec3 storedPosition {0.0f};
	glm::vec3 storedFocus {0.0f};
	/// The FOV last given to the renderer (radians). The config's FOV is only rewritten when the zoomer leaves it, so a
	/// player's own FOV stays until a script changes the lens
	float appliedFov = k_DefaultFov;

	/// The dual cameras above the script mode on the camera's stack, the last the current one.
	/// (approximate) openblack has no stack: they always sit above the script mode (a dual camera started over the
	/// player's mode stays above a later script mode here, where the original would put that one on top), and the
	/// stack's 12 places are not counted
	std::vector<DualMode> duals;

	State();
	~State();
};

/// The one game camera of the running game
State& Get();

/// A new map (the FOV at 70 degrees at once, no script mode)
void Reset();

/// START_CAMERA_CONTROL: false when the current mode cannot be left, i.e. a script mode is alive (the player's mode can
/// always be left). The new mode does not touch the zoomers: they start from the drawn camera `origin` / `focus`
/// (approximate: the original's zoomers keep heading for the destination the player had). It follows nothing, with time
/// factor 0.2 and "behind" on, heading 0, and the mode's seconds from 0
bool Begin(const glm::vec3& origin, const glm::vec3& focus);
/// The same with the player's zoomers taken as they are (value, speed, destination and time): the camera's zoomers are
/// the same for every mode, so the script mode goes on from wherever the player's was heading
bool BeginFrom(const Zoomer3& origin, const Zoomer3& focus);
/// The script mode deleted when it is the current one; the FOV back to 70 degrees in 0.5 s either way. Returns true
/// when a script mode was deleted (the player's mode is then created from where the camera is)
bool End();
/// The zoomers handed back to the player's camera after End (the player's mode starts from the camera's zoomers):
/// position and focus as the script left them, with their speed and destination
void HandBack(Zoomer3& origin, Zoomer3& focus);
[[nodiscard]] bool Active();
/// A script mode drives the camera: alive, and not under a camera test hook
[[nodiscard]] bool Drives();

/// SetPosition / SetFocus: the path dropped, the follow of that part dropped (which with "behind" also sets the
/// heading to 0 for the position) and the zoomers set (Zoomer::SetPosition)
void SetPosition(const glm::vec3& position);
void SetFocus(const glm::vec3& focus);
/// MovePosition / MoveFocus: the same drops, then each zoomer heads for the point with speed 0 in `seconds` (camera
/// seconds: the wall clock)
void MovePosition(const glm::vec3& position, float seconds);
void MoveFocus(const glm::vec3& focus, float seconds);
/// All six zoomers set (no matter the mode). (approximate) The original does nothing when both are already the drawn
/// camera (it only clears its moving flag); here they are set anyway
void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus);
/// The path and the focus follow dropped (the position follow stays), then the track `number` of camera.edt, from its
/// start
void RunPath(int32_t number);
/// The FOV zoomer (`fov` radians, `seconds` of game time)
void SetFov(float fov, float seconds);

// ---- Follow mode (the script mode's follows) ---------------------------------------------------------------------

/// What the follow mode and FaceObject read of a game thing (things are entt::entity)
struct ThingInfo
{
	/// Its map coords as a point: x, z / 6553.6, y = the land's altitude + the altitude above the land
	glm::vec3 mapPoint {0.0f};
	float height = 0.0f;
	/// An Object with a 3D object: its matrix's position, which the follow update uses instead of the map coords (not
	/// PlaceFollowNow). For villagers and animals that matrix is the one drawn between turns: openblack's DrawPosition
	std::optional<glm::vec3> drawnPoint;
	/// Whether the thing is available: false when flagged so, and for a villager whose final state is DYING. Read only
	/// by FocusFollow and Validate
	bool available = true;
	/// A flock: its position as a point (the leader's map coords, else the flock's own) and the leader's height, none
	/// without a leader
	bool isFlock = false;
	glm::vec3 flockPoint {0.0f};
	std::optional<float> leaderHeight;
	/// A wall hugger: its game angle (2048 per circle)
	std::optional<uint16_t> wallHugAngle;
	/// The facing direction (0 for a plain thing; a wall hugger's angle in radians; a creature's model angle + 2 pi
	/// - 2.5)
	float facingDirection = 0.0f;
	/// An Object (read by the dual camera): its 2D radius; none for a container (a flock or a town).
	/// (inferred) every thing with a Transform but a flock stands for an Object
	std::optional<float> radius2d;
};
/// nullopt: no such thing, or it is not available
using ThingReader = std::function<std::optional<ThingInfo>(entt::entity)>;

/// The pure parts, exposed for the tests

/// T = (modeSeconds > 2 ? 1 : (modeSeconds / 2) x (1 - 2) + 2) x timeFactor: twice the factor right after a mode
/// change, the factor itself from 2 s on
[[nodiscard]] float FollowSeconds(float modeSeconds, float timeFactor);
/// <= 2 -> 2; < 1500 -> itself; else 1500
[[nodiscard]] float ClampFollowDistance(float distance);
/// <= 0.241661 -> 0.241661 (the caller stores it back)
[[nodiscard]] float FollowPitch(float pitch);
/// With "behind" and a wall hugger, heading - (float(2 a) x pi / 2048 - pi / 2); else heading
[[nodiscard]] float FollowHeading(float heading, bool behind, std::optional<uint16_t> wallHugAngle);
/// height x 8
[[nodiscard]] float ThingViewingDistance(float height);
/// p + d (sin h cos q, sin q, cos h cos q)
[[nodiscard]] glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& p, float distance, float heading, float pitch);
/// v = a - b; |v.x| and |v.z| < 0.01 -> heading 0, pitch 1.5393804; else heading = pi - affine::GetYAngle(v) (0 when
/// v.x^2 + v.z^2 <= 1e-6, else affine::ArcTanOctant(-v.z, v.x)) and pitch = affine::ArcTanOctant(sqrt(v.x^2 +
/// v.z^2), v.y)
void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch);
/// The point the follow mode aims at for a thing: a flock's position with half its leader's height; anything else its
/// map coords point (with `update`, the per-frame follow, the 3D object's position when it has one) and half its height
[[nodiscard]] glm::vec3 FollowPoint(const ThingInfo& thing, bool update);
/// FaceObject's focus: the map coords point and half the height, no flock nor 3D object
[[nodiscard]] glm::vec3 FacePoint(const ThingInfo& thing);
/// heading = facing (>= 8 pi: "Invalid heading", and on), less 2 pi while > 2 pi;
/// PointFromDistanceHeadingAndPitch(focus, distance, heading, 0.1)
[[nodiscard]] glm::vec3 FacePosition(const glm::vec3& focus, float facing, float distance);

/// FOCUS_FOLLOW, SET_FOCUS_FOLLOW: the path dropped, the focus follows the thing if it is available, else nothing
void FocusFollow(entt::entity thing);
/// POSITION_FOLLOW, SET_POSITION_FOLLOW: with a thing the heading and pitch of the zoomers' destinations (position from
/// focus, HeadingAndPitchFromPoints) and the distance ThingViewingDistance; then with "behind" the heading is 0. The
/// path is not dropped
void PositionFollow(entt::entity thing);
/// FOCUS_AND_POSITION_FOLLOW, SET_FOCUS_AND_POSITION_FOLLOW: as PositionFollow, with `distance` as the distance and the
/// heading kept even with "behind"
void FocusAndPositionFollow(entt::entity thing, float distance);
/// SET_POSITION_FOLLOW, SET_FOCUS_AND_POSITION_FOLLOW: the zoomers placed at once (Zoomer::SetPosition) on the follow's
/// points (map coords, not the 3D object; the distance not clamped), the pitch clamped and kept, and the mode's seconds
/// at 2: the next frames follow at the factor's pace
void PlaceFollowNow();
/// CAMERA_PROPERTIES: distance, time factor, heading (radians) and behind (no zoom, already so)
void SetFollowProperties(float distance, float timeFactor, float heading, bool behind);
/// A followed thing that is no longer available is dropped (once a turn)
void Validate();

/// SET/MOVE_CAMERA_TO_FACE_OBJECT: nullopt when the thing is not there ("no object to face")
struct FacePoints
{
	glm::vec3 position;
	glm::vec3 focus;
};
[[nodiscard]] std::optional<FacePoints> FaceObject(entt::entity thing, float distance);

/// With a path, its duration <= the ms run; else position and focus both within sqrt(0.001) of their zoomers'
/// destinations
[[nodiscard]] bool ScriptArrived();

/// One frame of the camera update for the zoomers: the mode's seconds, the mode's update (the path with the frame's
/// game ms, then the follow), then the six zoomers (camera seconds, at most 0.1), the disc of the world, and the FOV
/// zoomer (game seconds)
void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds);

/// The drawn camera: position and focus of the zoomers, the position nudged when they meet, and both lifted so that the
/// (nudged) position stays `k_GroundClearance` above `groundAt(x, z)`. A NaN in the zoomers gives the last good drawn
/// camera (first (1000, 0, 1000))
struct Drawn
{
	glm::vec3 origin;
	glm::vec3 focus;
};
[[nodiscard]] Drawn DrawnCamera(const std::function<float(float, float)>& groundAt);

/// Game.cpp, once a frame instead of the player's Camera::Update when a script mode drives (Drives) and the citadel is
/// not shown (the original keeps the drawn camera there): Frame + DrawnCamera written to the camera. Always: the FOV to
/// the projection when it changed. Returns true when the script drove the camera (the player's model must not)
bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds);
/// Game.cpp, once a frame after the camera moved (script or player): SHAKE_CAMERA on the drawn camera only (every
/// mode), as Camera's draw offset; `lastDrawn` is the camera drawn the frame before. Then the shakes' clock
void ApplyShake(Camera& camera, const glm::vec3& lastDrawn);

/// The debug camera mode (read before the shake): 2 draws the camera from the debug position towards the debug focus, 1
/// only turns the focus to it, 0 (or any other value) neither. The camera's zoomers are not touched: ApplyShake puts it
/// in the draw offset, so it holds only where the drawn camera is updated (not in the citadel). Written by
/// PLAY_JC_SPECIAL 1 / 2 (ecs/IntroSpecial.h), and 0 when the intro is released and on a script reboot; the camera
/// editor's writers are debug only. (pending) the focus object and the falling spell's copy of the test are not ported
void SetDebugCameraMode(int32_t mode);
void SetDebugCameraPosition(const glm::vec3& position);
void SetDebugCameraFocus(const glm::vec3& focus);
[[nodiscard]] int32_t DebugCameraMode();

// ---- Dual camera ----------------------------------------------------------------------------------------------------

/// A mode of this module is the camera's current one: the script mode, or a dual camera (Drives, HAS_CAMERA_ARRIVED)
[[nodiscard]] bool HasMode();
/// The camera's current mode is the script mode (as the script's camera opcodes check): alive and no dual camera on top
/// of it
[[nodiscard]] bool ScriptModeCurrent();
/// The camera's current mode is a dual camera
[[nodiscard]] bool DualCurrent();
/// START_DUAL_CAMERA: nothing when the current mode is one of the same a and b (the new one deletes itself); else
/// pushed (the mode's seconds from 0). No mode nor citadel check. When no mode of this module was current the zoomers
/// are the player's `origin` / `focus` as they are (value, speed, destination and time, as BeginFrom: the camera's
/// zoomers are the same for every mode)
void StartDual(entt::entity a, entt::entity b, const Zoomer3& origin, const Zoomer3& focus);
/// CREATE_DUAL_CAMERA_WITH_POINT: nothing when the current mode is a dual camera with the same a and the same point;
/// else a point camera with distance factor 1.2
void StartDualWithPoint(entt::entity a, const glm::vec3& point, const Zoomer3& origin, const Zoomer3& focus);
/// UPDATE_DUAL_CAMERA: a and b set on the current mode when it is a dual camera (a point camera becomes a two things
/// one). False when it is not
bool UpdateDual(entt::entity a, entt::entity b);
/// RELEASE_DUAL_CAMERA (and End): the current mode, when it is a dual camera, deleted and popped (the mode under it
/// restarts: nothing for the script mode, and the mode's seconds = 0). With the player's mode under it, the zoomers go
/// back to the player's camera (as End: HandBack). False when it is not
bool ReleaseDual();

namespace detail
{
/// Test hook: when set, the things are read from here instead of the entities registry (nullptr: the registry again)
void SetThingReaderForTests(ThingReader reader);
} // namespace detail

} // namespace openblack::script_camera
