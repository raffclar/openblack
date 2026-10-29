/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleCameraModel.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "3D/L3DMesh.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera.h"
#include "CameraLens.h"
#include "Common/MachineClock.h"
#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundId.h"
#include "Temple/TempleHelpScripts.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using input::BindableActionMap;

namespace
{
// The game casts the mouse's rays through the near plane, which the temple's cameras put this far out
constexpr float k_NearClip = 0.2f;
constexpr float k_FieldOfView = 90.0f;

/// Hits within this of the room's axis are on the pool, and doors only beyond it
constexpr float k_PoolRadius = 16.0f;

// The main room camera's views, looking down at the pool and up at the room
constexpr glm::vec3 k_LowOrigin {0.0f, 14.4f, 14.4f};
constexpr glm::vec3 k_HighOrigin {0.0f, 8.0f, 20.0f};
constexpr glm::vec3 k_LowFocus {0.0f, 0.0f, 4.0f};
constexpr glm::vec3 k_HighFocus {0.0f, 12.0f, 0.0f};
/// Where the main room's camera leans when the player takes it over
constexpr float k_StartLean = 0.7f;
constexpr float k_MaxLean = 1.5f;
/// How long the turn and lean take to catch up with the player
constexpr float k_OrbitEaseTime = 0.3f;
/// How far, in pixels, the mouse moves from where it was pressed before the press is a drag rather than a click
constexpr float k_DragDistance = 3.0f;
/// Holding the press on the pool's map: how far from the point pressed the camera goes, how fast it gets there, how far
/// it turns and tilts as the mouse crosses the screen, and between which tilts
constexpr float k_MapDistance = 6.0f;
constexpr float k_MapZoomSpeed = 2.0f;
constexpr float k_MapTurnPerScreen = 6.0f;
constexpr float k_MapTiltPerScreen = 2.0f;
constexpr float k_MapMinPitch = 0.62831855f;
constexpr float k_MapMaxPitch = 1.1780972f;

// The way through a door of the main room, facing the door ahead along x: up to the door, then through it
constexpr TempleCameraModel::Pose k_DoorApproach {{0.0f, 25.0f, 0.0f}, {15.0f, 20.0f, 0.0f}};
constexpr TempleCameraModel::Pose k_DoorEntry {{100.0f, 8.0f, 0.0f}, {120.0f, 10.0f, 0.0f}};
/// The door to the room of scrolls isn't one the camera goes through
constexpr uint32_t k_ScrollWall = 6;
/// The temple starts the doorway the camera walks through this far before the start of its swing, at this rate a
/// second: it waits over a second, then is open as the camera reaches it
constexpr float k_DoorOpenFrom = -0.5f;
constexpr float k_DoorOpenRate = 0.4f;
/// Walking back to the main room from another room, the doorway opens from the start of its swing, at this rate
constexpr float k_DoorBackOpenFrom = 0.0f;
constexpr float k_DoorBackOpenRate = 2.0f;

/// The camera's views of a room of pictures, as the game sets each room up: the camera and where it looks, low and
/// looking up at lean 0 and high and looking down at lean 1, about the room's centre, turned by the camera's yaw and
/// raised by its height
struct PictureRoom
{
	glm::vec3 centre;
	glm::vec3 lowOrigin;
	glm::vec3 highOrigin;
	glm::vec3 lowFocus;
	glm::vec3 highFocus;
	float maxHeight;
	/// The credits room turns about where its path ends instead
	bool aroundPathEnd;
	/// The door of the main room the room is behind, which the camera walks back through
	uint32_t door;
};
constexpr float k_PictureMinHeight = 9.0f;
constexpr PictureRoom k_ChallengeRoom {{87.0f, 0.0f, -87.0f},
                                       {0.0f, -7.0f, 8.0f},
                                       {0.0f, 7.0f, 10.0f},
                                       {0.0f, 6.0f, -4.0f},
                                       {0.0f, -6.0f, -4.0f},
                                       80.0f,
                                       false,
                                       7};
constexpr PictureRoom k_SaveGameRoom {{-87.0f, 0.0f, -87.0f},
                                      {0.0f, -7.0f, 8.0f},
                                      {0.0f, 7.0f, 10.0f},
                                      {0.0f, 6.0f, -4.0f},
                                      {0.0f, -6.0f, -4.0f},
                                      80.0f,
                                      false,
                                      5};
constexpr PictureRoom k_OptionsRoom {{87.0f, 0.0f, 87.0f},
                                     {0.0f, -7.0f, 20.0f},
                                     {0.0f, 7.0f, 20.0f},
                                     {0.0f, 6.0f, -4.0f},
                                     {0.0f, -6.0f, -4.0f},
                                     30.0f,
                                     false,
                                     1};
constexpr PictureRoom k_MultiRoom {{-87.0f, 0.0f, 87.0f},
                                   {0.0f, -7.0f, 20.0f},
                                   {0.0f, 7.0f, 20.0f},
                                   {0.0f, 6.0f, -4.0f},
                                   {0.0f, -6.0f, -4.0f},
                                   30.0f,
                                   false,
                                   3};
constexpr PictureRoom k_CreditsRoom {
    {-123.0f, 0.0f, 0.0f}, {0.0f, -0.1f, 0.1f}, {0.0f, 0.1f, 0.1f}, {0.0f, 0.5f, -1.0f}, {0.0f, -0.5f, -1.0f}, 30.0f, true, 4};
/// The rooms of pictures' walls, about their centres
constexpr float k_PictureRoomRadius = 37.0f;
/// The lean and height a room of pictures' camera starts at
constexpr float k_PictureStartLean = 0.6f;
constexpr float k_PictureStartHeight = 6.0f;
/// How long the turn, lean and height take to catch up with the player
constexpr float k_PictureYawEaseTime = 0.2f;
constexpr float k_PictureLeanEaseTime = 0.3f;
constexpr float k_PictureHeightEaseTime = 0.15f;
/// The lean follows the height, from this at the lowest the camera goes, rising by this for each unit up
constexpr float k_PictureLowLean = 0.2f;
constexpr float k_PictureLeanPerHeight = 0.0084507046f;
/// How fast the arrow keys lean and raise the camera
constexpr float k_PictureLeanSpeed = 1.85f;
constexpr float k_PictureRiseSpeed = 30.0f;
/// How fast a room of pictures turns by itself under a dialog, in radians a second, once it has sped up over a second
constexpr float k_PictureDialogTurnSpeed = 0.25f;
/// The ring the rooms' doors back to the main room are on, about the main room's centre
constexpr float k_PictureDoorRadius = 87.0f;
/// The creature's room's ring, and the main room's door the creature's room is behind
constexpr float k_CreatureDoorRadius = 78.0f;
constexpr uint32_t k_CreatureDoor = 0;
// The way back to the main room through a room's door, facing the main room along x: halfway from the camera to the
// door, then through it
constexpr TempleCameraModel::Pose k_DoorBack {{40.0f, 16.0f, 0.0f}, {20.0f, 10.0f, 0.0f}};

// The creature's room: its walls, and its camera's look
constexpr TempleCameraModel::Cylinder k_CreatureRoomCylinder {{120.0f, 0.0f, -120.0f}, TempleCameraModel::k_RoomRadius, false};
/// How far ahead the look's point is
constexpr float k_LookDistance = 20.0f;
/// How far up and down the look turns
constexpr float k_LookMaxPitch = 1.4959966f;
/// How fast the arrow keys speed up the look's turn, and how quickly it slows
constexpr float k_LookAcceleration = 0.3f;
constexpr float k_LookSlowing = 10.0f;
/// A look whose sideways parts are both under this is straight up or down, at the pitch below
constexpr double k_StraightLook = 0.01;
constexpr float k_StraightLookPitch = 1.5393804f;
/// A half turn, as the float the game takes a heading from
constexpr float k_HalfTurn = 3.1415927410125732f;
/// How long the camera takes to catch up with the look
constexpr float k_LookEaseTime = 0.15f;

const PictureRoom* PictureRoomOf(TempleRoom room)
{
	switch (room)
	{
	case TempleRoom::Challenge:
		return &k_ChallengeRoom;
	case TempleRoom::SaveGame:
		return &k_SaveGameRoom;
	case TempleRoom::Options:
		return &k_OptionsRoom;
	case TempleRoom::Multi:
		return &k_MultiRoom;
	case TempleRoom::Credits:
		return &k_CreditsRoom;
	case TempleRoom::Main:
	case TempleRoom::CreatureCave:
	case TempleRoom::Unknown:
		break;
	}
	return nullptr;
}

/// The kind of thing the mouse is over: the pool, the floor or a wall
enum class HitKind : uint8_t
{
	Pool,
	Floor,
	Wall,
};

std::optional<HitKind> KindOf(const std::optional<TempleCameraModel::RoomHit>& hit)
{
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	if (hit->distance < k_PoolRadius)
	{
		return HitKind::Pool;
	}
	return hit->floor ? HitKind::Floor : HitKind::Wall;
}

/// Turns a point about the room's axis, as the main room's camera does by its matrix
/// A point turned about the vertical by an angle, as the game turns it: by a full matrix of the FPU's cosine and sine
/// with a place of zero, so a part that comes to zero is a positive zero
glm::vec3 TurnAbout(glm::vec3 point, float angle)
{
	const float cosine = x87::RoundToFloat(x87::Cos(angle));
	const float sine = x87::RoundToFloat(x87::Sin(angle));
	return {((((-sine) * point.z) + (0.0f * point.y)) + (cosine * point.x)) + 0.0f,
	        (((0.0f * point.z) + point.y) + (0.0f * point.x)) + 0.0f,
	        (((0.0f * point.y) + (cosine * point.z)) + (sine * point.x)) + 0.0f};
}

/// The point a share of the way from one point to another, as the game finds it: the way between times the share,
/// then the start added
glm::vec3 Between(glm::vec3 from, glm::vec3 to, float share)
{
	return ((to - from) * share) + from;
}

/// A point turned about the vertical with the sine kept wide: each product with the sine rounds one time
glm::vec3 TurnAboutWithWideSine(glm::vec3 point, float angle)
{
	const float cosine = x87::RoundToFloat(x87::Cos(angle));
	const double sine = x87::Sin(angle);
	return {((cosine * point.x) + x87::Mul(-sine, point.z)) + 0.0f, point.y,
	        ((cosine * point.z) + x87::Mul(sine, point.x)) + 0.0f};
}

/// A pose a share of the way to another, as the game blends the camera: each point is found as `Between` finds it
TempleCameraModel::Pose Mix(const TempleCameraModel::Pose& from, const TempleCameraModel::Pose& to, float amount)
{
	return {Between(from.origin, to.origin, amount), Between(from.focus, to.focus, amount)};
}
} // namespace

TempleCameraModel::Cylinder TempleCameraModel::CylinderOf(Room room)
{
	if (const auto* picture = PictureRoomOf(room); picture != nullptr)
	{
		return {picture->centre, k_PictureRoomRadius};
	}
	if (room == Room::CreatureCave)
	{
		return k_CreatureRoomCylinder;
	}
	return {glm::vec3(0.0f), k_RoomRadius};
}

std::optional<TempleCameraModel::RoomHit> TempleCameraModel::RayCastRoom(glm::vec3 eye, glm::vec3 nearPoint)
{
	return RayCastRoom(eye, nearPoint, Cylinder {glm::vec3(0.0f), k_RoomRadius});
}

std::optional<TempleCameraModel::RoomHit> TempleCameraModel::RayCastRoom(glm::vec3 eye, glm::vec3 nearPoint,
                                                                         const Cylinder& room)
{
	// The game casts into a room about the room's centre: both points are taken about it first, then the way from the
	// eye to the near plane's point
	const auto origin = eye - room.centre;
	const auto direction = (nearPoint - room.centre) - origin;
	const float radius = room.radius;

	// The floor, when the ray points at it enough
	std::optional<float> floorDistance;
	if (room.floor && std::abs(direction.y) > 0.01f)
	{
		if (const float t = -(origin.y / direction.y); t > 0.0f)
		{
			floorDistance = t;
		}
	}

	// The walls, from outside them or within
	const float a = (direction.z * direction.z) + (direction.x * direction.x);
	const float halfB = (direction.z * origin.z) + (origin.x * direction.x);
	const float b = halfB + halfB;
	const float c = ((origin.z * origin.z) + (origin.x * origin.x)) - (radius * radius);
	const float discriminant = (b * b) - ((c * a) * 4.0f);
	std::optional<float> wallDistance;
	if (discriminant > 0.0f)
	{
		const float root = std::sqrt(discriminant);
		const float t = (c >= 0.0f ? (-b - root) : (root - b)) / (a + a);
		if (t > 0.0f)
		{
			wallDistance = t;
		}
	}

	bool onFloor = false;
	float t = 0.0f;
	if (wallDistance.has_value() && (!floorDistance.has_value() || *wallDistance < *floorDistance))
	{
		t = *wallDistance;
	}
	else if (floorDistance.has_value())
	{
		t = *floorDistance;
		onFloor = true;
	}
	else
	{
		return std::nullopt;
	}

	// The wall's normal is back along the ray, made one long with its squares added x, z, y
	glm::vec3 normal {0.0f, 1.0f, 0.0f};
	if (!onFloor)
	{
		normal = -direction;
		if (normal != glm::vec3(0.0f))
		{
			normal = x87::Scaled<x87::SumOrder::Xzy>(normal);
		}
	}
	const glm::vec3 point {(direction.x * t) + origin.x, (t * direction.y) + origin.y, (direction.z * t) + origin.z};
	return RoomHit {
	    .distance = std::sqrt((point.z * point.z) + (point.x * point.x)),
	    .angle = x87::RoundToFloat(x87::Atan2(point.z, point.x)),
	    .height = point.y + room.centre.y,
	    .floor = onFloor,
	    .point = point + room.centre,
	    .normal = normal,
	};
}

std::optional<uint32_t> TempleCameraModel::DoorAt(const RoomHit& hit)
{
	// A door is in the walls between these heights, in the middle of an eighth of the room
	constexpr float k_DoorBottom = 7.0f;
	constexpr float k_DoorTop = 23.0f;
	constexpr float k_DoorStart = 6.4581857f;
	constexpr float k_DoorWidth = 0.35f;
	constexpr float k_DoorMargin = 0.1f;
	if (hit.distance <= k_PoolRadius || hit.floor || hit.height <= k_DoorBottom || hit.height >= k_DoorTop)
	{
		return std::nullopt;
	}
	float within = hit.angle + k_DoorStart;
	uint32_t door = 0;
	while (within > glm::quarter_pi<float>())
	{
		within -= glm::quarter_pi<float>();
		++door;
	}
	if (within >= k_DoorWidth - k_DoorMargin || within <= k_DoorMargin)
	{
		return std::nullopt;
	}
	return door & 7u;
}

std::optional<TempleCameraModel::Room> TempleCameraModel::RoomBehindDoor(uint32_t door)
{
	switch (door)
	{
	case 0:
		return Room::CreatureCave;
	case 1:
		return Room::Options;
	case 3:
		return Room::Multi;
	case 4:
		return Room::Credits;
	case 5:
		return Room::SaveGame;
	case 7:
		return Room::Challenge;
	default:
		// The way out
		return std::nullopt;
	}
}

TempleCameraModel::Pose TempleCameraModel::OrbitPose(float yaw, float lean)
{
	return {TurnAbout(Between(k_LowOrigin, k_HighOrigin, lean), yaw), TurnAbout(Between(k_LowFocus, k_HighFocus, lean), yaw)};
}

glm::vec2 TempleCameraModel::MapYawAndPitch(glm::vec3 away)
{
	const auto direction = away != glm::vec3(0.0f) ? x87::Scaled<x87::SumOrder::Yzx>(away) : away;
	const float across = std::sqrt((direction.z * direction.z) + (direction.x * direction.x));
	return {x87::RoundToFloat(x87::Atan2(direction.z, direction.x)), x87::RoundToFloat(x87::Atan2(direction.y, across))};
}

glm::vec3 TempleCameraModel::MapAround(float yaw, float pitch)
{
	// The FPU's cosines and sines stay wide for the products
	return {x87::Mul(x87::Cos(yaw), x87::Cos(pitch)), x87::RoundToFloat(x87::Sin(pitch)),
	        x87::Mul(x87::Sin(yaw), x87::Cos(pitch))};
}

float TempleCameraModel::DragLeanStep(float mouseDown, float dt, float across)
{
	return ((mouseDown * dt) * 0.06f) * (1.0f - std::abs(across));
}

float TempleCameraModel::EdgeLeanStep(float dt, float ramp, float speed)
{
	// The FPU's cosine stays wide for the subtraction
	return ((dt * 1.5f) * x87::Sub(1.0, x87::Cos(ramp * glm::pi<float>()))) * speed;
}

TempleCameraModel::Pose TempleCameraModel::TurnToDoor(const Pose& pose, uint32_t door)
{
	const float angle = static_cast<float>(door) * glm::quarter_pi<float>();
	return {TurnAbout(pose.origin, angle), TurnAbout(pose.focus, angle)};
}

TempleCameraModel::Pose TempleCameraModel::TurnToDoorBack(const Pose& pose, uint32_t door)
{
	const float angle = static_cast<float>(door) * glm::quarter_pi<float>();
	return {TurnAboutWithWideSine(pose.origin, angle), TurnAboutWithWideSine(pose.focus, angle)};
}

bool TempleCameraModel::IsPictureRoom(Room room)
{
	return PictureRoomOf(room) != nullptr;
}

TempleCameraModel::Pose TempleCameraModel::PictureOrbitPose(Room room, float yaw, float lean, float height, glm::vec3 pathEnd)
{
	// Where a room of pictures' camera is and looks, at a turn, lean and height
	const auto& picture = *PictureRoomOf(room);
	const auto centre = picture.aroundPathEnd ? glm::vec3(pathEnd.x, 0.0f, pathEnd.z) : picture.centre;
	// The height goes on after the centre, and only up
	auto origin = TurnAbout(Between(picture.lowOrigin, picture.highOrigin, lean), yaw) + centre;
	auto focus = TurnAbout(Between(picture.lowFocus, picture.highFocus, lean), yaw) + centre;
	origin.y = height + origin.y;
	focus.y = height + focus.y;
	return {origin, focus};
}

float TempleCameraModel::PictureStartYaw(const Pose& pathEnd)
{
	// A quarter turn on from the way the path ends looking, which the orbit's views look along unturned
	const auto along = pathEnd.focus - pathEnd.origin;
	return std::atan2(along.z, along.x) + glm::half_pi<float>();
}

TempleCameraModel::Pose TempleCameraModel::LookPose(glm::vec3 from, float heading, float pitch)
{
	// The point at the look's distance, heading and pitch from the camera, mirrored through the camera. The cosine of
	// the pitch is stored as a float; the other cosine and the sines stay wide into their first products.
	const float pitchCosine = x87::RoundToFloat(x87::Cos(pitch));
	const glm::vec3 point {(x87::Mul(x87::Sin(heading), pitchCosine) * k_LookDistance) + from.x,
	                       x87::Mul(x87::Sin(pitch), k_LookDistance) + from.y,
	                       (x87::Mul(x87::Cos(heading), pitchCosine) * k_LookDistance) + from.z};
	return {from, (from - point) + from};
}

glm::vec2 TempleCameraModel::HeadingAndPitch(const Pose& look)
{
	const auto behind = look.origin - look.focus;
	if (x87::Less(std::abs(behind.x), k_StraightLook) && x87::Less(std::abs(behind.z), k_StraightLook))
	{
		// Straight up or down
		return {0.0f, k_StraightLookPitch};
	}
	const float across = std::sqrt((behind.z * behind.z) + (behind.x * behind.x));
	return {x87::Sub(k_HalfTurn, x87::GameHeadingOf(behind.x, behind.z)), x87::RoundToFloat(x87::GameAtan2(behind.y, across))};
}

TempleCameraModel::TempleCameraModel(Paths paths, Room room)
    : _paths(std::move(paths))
{
	// Each room of pictures' camera starts at its lean and height
	for (size_t i = 0; i < k_RoomCount; ++i)
	{
		const auto pictureRoom = static_cast<Room>(i);
		if (IsPictureRoom(pictureRoom))
		{
			ResetPictureCamera(pictureRoom);
			auto& picture = _pictures.at(i);
			picture.yaw.Reset(picture.yawTarget);
			picture.lean.Reset(picture.leanTarget);
			picture.height.Reset(picture.heightTarget);
		}
	}

	// The room's camera starts at the start of its path
	if (const auto& path = _paths.at(static_cast<size_t>(room)); path)
	{
		const auto start = path->SampleAt(std::chrono::milliseconds::zero());
		_origin.Reset(start.position);
		_focus.Reset(start.focus);
		_target = {start.position, start.focus};
	}
}

TempleCameraModel::~TempleCameraModel() = default;

TempleCameraModel::Room TempleCameraModel::GetRoom() const
{
	return Locator::temple::value().GetCurrentRoom();
}

void TempleCameraModel::StartIntro(Room room, bool blendFromCurrent)
{
	// The room's camera starts its path in
	Locator::temple::value().SetCurrentRoom(room);
	// Each room has a camera of its own, so a scroll looked at in the room left is left with it, along with its text in
	// front of it, which only that room draws
	_lookingAtSubMesh = false;
	_subMeshZoom = 0.0f;
	_subMeshLook.reset();
	ResetCaveTargets();
	_state = State::Intro;
	_nextState = State::Intro;
	_introTime = 0.0f;
	_blendTime = 0.0f;
	_blendFromPrevious = blendFromCurrent;
	if (blendFromCurrent)
	{
		_previousOrigin = _origin;
		_previousFocus = _focus;
	}
}

void TempleCameraModel::GoToRoom(Room room)
{
	Locator::temple::value().GetDoors().FastClose();
	if (room == GetRoom())
	{
		return;
	}
	// Going straight to a room skips its path: the room's camera starts over, takes over from the camera, and
	// settles where the player has it
	if (IsPictureRoom(room))
	{
		ResetPictureCamera(room);
	}
	StartIntro(room, true);
	_nextState = ControlOf(room);
	ChangeState();
	const Input still {.button = 0,
	                   .mouse = _lastMouse,
	                   .mouseScreen = glm::vec2(0.0f),
	                   .hit = std::nullopt,
	                   .door = std::nullopt,
	                   .doorBack = std::nullopt};
	Step(1.0f, still);
	Step(1.0f, still);
}

bool TempleCameraModel::GoThroughDoorTo(Room room)
{
	if (_state != State::Orbit)
	{
		return false;
	}
	for (uint32_t door = 0; door < 8; ++door)
	{
		// No room is the way out
		const auto behind = RoomBehindDoor(door);
		if (behind == room || (room == Room::Unknown && !behind.has_value() && door != k_ScrollWall))
		{
			_doorStart = TurnToDoor(k_DoorApproach, door);
			_doorEnd = TurnToDoor(k_DoorEntry, door);
			_doorRoom = behind;
			_nextState = State::ThroughDoor;
			ChangeState();
			return true;
		}
	}
	return false;
}

void TempleCameraModel::HandleActions(std::chrono::microseconds /*dt*/)
{
	// The keys for the rooms are handled by the game, as they work outside the temple too
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> TempleCameraModel::Update(std::chrono::microseconds dt,
                                                                                    const Camera& camera)
{
	const float seconds = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
	const auto& actions = Locator::gameActionSystem::value();

	Input input {};
	// A double click comes to the temple's camera once, as its second press
	if (Locator::gameActionSystem::value().TakeDoubleClick())
	{
		input.button = 2;
	}
	else if (actions.GetAny(BindableActionMap::MOVE, BindableActionMap::ACTION))
	{
		input.button = 1;
	}
	input.mouse = glm::vec2(actions.GetMousePosition());
	// A press held since before the player came in isn't one inside, and while a script has the cinema bars in, as when
	// the advisors introduce a room, the temple takes no presses at all
	const bool cinemaBars =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	const auto pressed = static_cast<temple_press::Button>(input.button);
	input.button =
	    static_cast<uint8_t>(cinemaBars ? _heldFromOutside.FilterUnderCinemaBars(pressed) : _heldFromOutside.Filter(pressed));
	// A press on one of the room's controls goes to the control, which the room's camera then doesn't see
	if (Locator::temple::value().HoldControl(input.button != 0, input.mouse.y))
	{
		input.button = 0;
	}
	// Under the temple's fade the camera doesn't take a held press
	input.button = static_cast<uint8_t>(
	    temple_press::UnderFade(static_cast<temple_press::Button>(input.button), Locator::temple::value().GetFadeAmount()));
	if (Locator::windowing::has_value())
	{
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		input.mouseScreen = input.mouse / (size * 0.5f) - 1.0f;
		input.screen = size;

		// The game casts from the camera through the mouse on its near plane
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		camera.DeprojectScreenToWorld(input.mouse / size, rayOrigin, rayDirection);
		const auto forward = camera.GetForward();
		if (const float along = glm::dot(rayDirection, forward); along > 0.0f)
		{
			const auto nearPoint = camera.GetOrigin() + rayDirection * (k_NearClip / along);
			_cursorRay = Pose {camera.GetOrigin(), camera.GetOrigin() + rayDirection};
			input.hit = RayCastRoom(camera.GetOrigin(), nearPoint, CylinderOf(GetRoom()));
			// The doors back to the main room are on a ring about its centre
			if (GetRoom() != Room::Main)
			{
				const float ringRadius = GetRoom() == Room::CreatureCave ? k_CreatureDoorRadius : k_PictureDoorRadius;
				if (const auto ring = RayCastRoom(camera.GetOrigin(), nearPoint,
				                                  Cylinder {glm::vec3(0.0f), ringRadius, CylinderOf(GetRoom()).floor});
				    ring.has_value())
				{
					input.doorBack = DoorAt(*ring);
				}
			}
		}
	}
	if (GetRoom() == Room::CreatureCave && Locator::windowing::has_value())
	{
		// The creature's room finds its targets where their points are projected onto the screen
		using namespace CreatureCaveTargets;
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		// The creature is found by its body under the cursor, as it is drawn
		if (Locator::creatureCaveSystem::has_value() && size.x > 0.0f && size.y > 0.0f)
		{
			const auto [eye, near] = camera.OnNearPlane(input.mouse / size);
			input.overCaveCreature = Locator::creatureCaveSystem::value().CursorOverCaveCreature({
			    .worldToClip = camera.GetViewProjectionMatrix(),
			    .resolution = size,
			    .near = camera.GetNearClip(),
			    .xScale = camera.GetProjectionMatrix()[0][0],
			    .camera = eye,
			    .cursor = input.mouse,
			});
		}
		for (size_t i = 0; i < k_Count; ++i)
		{
			const auto target = static_cast<Target>(i);
			const auto place = target == Target::Exit       ? std::optional<glm::vec3>(k_ExitPlace)
			                   : target == Target::Creature ? std::nullopt
			                                                : CaveMeshPoint(k_FirstLookPoint + static_cast<uint32_t>(i));
			glm::vec3 screen;
			if (place.has_value() && camera.ProjectWorldToScreen(*place, glm::vec4(0.0f, 0.0f, size), screen))
			{
				input.caveTargets.at(i) = glm::vec2(screen);
			}
		}
	}
	if (input.hit.has_value())
	{
		input.door = DoorAt(*input.hit);
	}
	// This frame's, none where the cursor meets nothing of the room's cylinder
	_cursorHit = input.hit;

	Step(seconds, input);
	FollowTemple();
	Locator::temple::value().GetDoors().Update(seconds);

	auto origin = _origin.GetValue();
	auto focus = _focus.GetValue();
	// In the creature's room, it goes over to the target zoomed to in a straight line
	if (_caveZoom > 0.0f)
	{
		origin = Between(origin, _caveLook.origin, _caveZoom);
		focus = Between(focus, _caveLook.focus, _caveZoom);
	}
	// In every room, the camera then goes over to the scroll it looks at along a cosine
	if (_subMeshLook.has_value() && _subMeshZoom > 0.0f)
	{
		// The FPU's cosine stays wide for the subtraction
		const float along = x87::Sub(1.0, x87::Cos(std::min(_subMeshZoom, 1.0f) * glm::pi<float>())) * 0.5f;
		origin = Between(origin, _subMeshLook->origin, along);
		focus = Between(focus, _subMeshLook->focus, along);
	}
	return CameraInterpolationUpdateInfo {origin, focus, std::chrono::microseconds::zero()};
}

void TempleCameraModel::Step(float dt, const Input& input)
{
	// The room's camera aims, then the camera eases after its aim
	_originTime = 0.5f;
	_focusTime = 0.5f;
	_easeToTarget = true;
	if (input.hit.has_value())
	{
		_lastHitAngle = input.hit->angle;
	}
	if (input.button != 0 && !_wasPressed)
	{
		_pressHit = input.hit;
		_lastMouse = input.mouse;
		_pressMouse = input.mouse;
	}
	if (IsPictureRoom(GetRoom()))
	{
		UpdatePictureCamera(dt);
	}

	// Looking at a scroll, the room's camera keeps still, and a press elsewhere or the arrow keys send it back, as in
	// the creature's room
	const bool inControl = _state == ControlOf(GetRoom());
	if (_lookingAtSubMesh)
	{
		const auto& actions = Locator::gameActionSystem::value();
		const bool turning = actions.GetAny(BindableActionMap::MOVE_LEFT, BindableActionMap::MOVE_RIGHT,
		                                    BindableActionMap::MOVE_FORWARDS, BindableActionMap::MOVE_BACKWARDS);
		const bool leftByPlayer = (input.button != 0 && !_wasPressed) || turning;
		if (leftByPlayer || !inControl)
		{
			_lookingAtSubMesh = false;
		}
		// The player taking the camera away from the scroll stops the scroll's help
		if (leftByPlayer)
		{
			temple_help::GameScripts scripts;
			temple_help::LeaveScroll(scripts, GetRoom());
		}
	}
	_subMeshZoom = std::clamp(_subMeshZoom + ((_lookingAtSubMesh ? 0.75f : -0.7f) * dt), 0.0f, 1.0f);
	if (!_lookingAtSubMesh && _subMeshZoom <= 0.0f)
	{
		_subMeshLook.reset();
	}
	_hoveredDoor = _state == State::Orbit ? input.door : std::nullopt;
	_overPool = _state == State::Orbit && KindOf(input.hit) == HitKind::Pool;
	_pressingPool = _state == State::Orbit && input.button != 0 && KindOf(_pressHit) == HitKind::Pool;
	_overWayBack = false;
	if (input.doorBack.has_value() && _state == ControlOf(GetRoom()))
	{
		const auto* picture = PictureRoomOf(GetRoom());
		_overWayBack = GetRoom() == Room::CreatureCave ? *input.doorBack == k_CreatureDoor
		                                               : picture != nullptr && *input.doorBack == picture->door;
	}

	if (GetRoom() == Room::CreatureCave)
	{
		UpdateCaveTargets(dt, input);
	}

	if (!_lookingAtSubMesh)
	{
		switch (_state)
		{
		case State::Intro:
			UpdateIntro(dt, input);
			break;
		case State::Orbit:
			UpdateOrbit(dt, input);
			break;
		case State::Look:
			UpdateLook(dt, input);
			break;
		case State::PictureOrbit:
			UpdatePictureOrbit(dt, input);
			break;
		case State::ThroughDoor:
			UpdateThroughDoor(dt, input);
			break;
		}
	}
	ChangeState();

	if (_easeToTarget)
	{
		_origin.SetDestination(_target.origin, _originTime);
		_focus.SetDestination(_target.focus, _focusTime);
	}
	_origin.Update(dt);
	_focus.Update(dt);
	_wasPressed = input.button != 0;
}

void TempleCameraModel::UpdateIntro(float dt, const Input& input)
{
	const auto control = ControlOf(GetRoom());
	_introTime += dt;
	_blendTime += dt;
	const auto& path = _paths.at(static_cast<size_t>(GetRoom()));
	if (!path)
	{
		_nextState = control;
		return;
	}

	const auto key = path->SampleAt(std::chrono::milliseconds(static_cast<int64_t>(_introTime * 1000.0f)));
	_target = {key.position, key.focus};
	const float duration = static_cast<float>(path->GetDuration().count()) * 0.001f;
	// The player takes over half a second after the path ends, or with a click, which covers the cut with the
	// temple's fade
	if (_introTime > duration + 0.5f)
	{
		_nextState = control;
	}
	if (input.button != 0 && !_wasPressed)
	{
		_nextState = control;
		Locator::temple::value().CoverSkippedFlight();
	}

	// The path eases into where the player takes over over its last second
	if (const float intoControl = _introTime - (duration - 1.0f); intoControl > 0.0f)
	{
		_target = Mix(_target, ControlPose(GetRoom()), std::min(intoControl, 1.0f));
	}
	// Coming through a door, the camera eases off the way it was going over the first two seconds
	if (const float fromPrevious = (2.0f - _blendTime) * 0.5f; fromPrevious > 0.0f && _blendFromPrevious)
	{
		_previousOrigin.Update(dt);
		_previousFocus.Update(dt);
		const float amount = std::min(fromPrevious, 1.0f);
		_target = Mix(_target, {_previousOrigin.GetValue(), _previousFocus.GetValue()}, amount);
		_originTime *= 1.0f - amount;
		_focusTime *= 1.0f - amount;
	}
}

void TempleCameraModel::UpdateOrbit(float dt, const Input& input)
{
	const auto& actions = Locator::gameActionSystem::value();
	const auto pressKind = KindOf(_pressHit);

	// Dragging the floor or the walls turns the room to keep them under the mouse, and up and down leans
	if (input.button != 0 && (pressKind == HitKind::Floor || pressKind == HitKind::Wall))
	{
		float turn = _pressHit->angle - _lastHitAngle;
		if (turn < -glm::pi<float>())
		{
			turn += glm::two_pi<float>();
		}
		if (turn > glm::pi<float>())
		{
			turn -= glm::two_pi<float>();
		}
		if (_wasPressed)
		{
			_yawTarget = _yaw.GetValue() + turn;
			_leanTarget += DragLeanStep(input.mouse.y - _lastMouse.y, dt, input.mouseScreen.x);
		}
		_lastMouse = input.mouse;
	}
	// Holding the press on the pool draws the camera to the point pressed on the island's map, 6 units from it, and
	// dragging turns about it and tilts. A double click there leaves the temple for that place.
	if (input.button != 0 && pressKind == HitKind::Pool && _pressHit.has_value())
	{
		if (!_wasPressed)
		{
			// The turn and tilt from the point to where the camera is
			_mapFocus = _pressHit->point;
			const auto angles = MapYawAndPitch(_target.origin - _mapFocus);
			_mapYaw = angles.x;
			_mapPitch = angles.y;
		}
		const auto moved = (input.mouse - _pressMouse) / glm::max(input.screen, glm::vec2(1.0f));
		const float yaw = _mapYaw - (moved.x * k_MapTurnPerScreen);
		const float pitch = std::clamp(_mapPitch + (moved.y * k_MapTiltPerScreen), k_MapMinPitch, k_MapMaxPitch);
		_subMeshLook = Pose {_mapFocus + (MapAround(yaw, pitch) * k_MapDistance), _mapFocus};
		_subMeshZoom = std::min(_subMeshZoom + (dt * k_MapZoomSpeed), 1.0f);

		if (input.button == 2 && !_leavingByMap)
		{
			_mapDoubleClick = _mapFocus;
			_leavingByMap = true;
		}
	}

	// With the mouse free, the screen's top and bottom edges lean, and the pool slowly draws the camera down to it
	float leanSpeed = 0.0f;
	if (input.button == 0)
	{
		const float edge = input.mouseScreen.y;
		if (edge > 0.8f)
		{
			leanSpeed = (edge - 0.8f) * -0.8f;
			_edgeRamp += dt * 0.2f;
		}
		else if (edge < -0.8f)
		{
			leanSpeed = (-edge - 0.8f) * 0.8f;
			_edgeRamp += dt * 0.2f;
		}
		else if (KindOf(input.hit) == HitKind::Pool)
		{
			leanSpeed = _leanTarget < 0.333f ? _leanTarget * -0.3f : -0.1f;
			_edgeRamp += dt * 0.1f;
		}
		else
		{
			_edgeRamp = 0.0f;
		}
		_edgeRamp = std::clamp(_edgeRamp, 0.0f, 1.0f);
	}
	_leanTarget += EdgeLeanStep(dt, _edgeRamp, leanSpeed);

	// The arrow keys turn and lean
	if (actions.Get(BindableActionMap::MOVE_LEFT))
	{
		_yawTarget += dt * 2.0f;
	}
	if (actions.Get(BindableActionMap::MOVE_RIGHT))
	{
		_yawTarget -= dt * 2.0f;
	}
	if (actions.Get(BindableActionMap::MOVE_FORWARDS))
	{
		_leanTarget += dt * 0.85f;
	}
	if (actions.Get(BindableActionMap::MOVE_BACKWARDS))
	{
		_leanTarget -= dt * 0.85f;
	}
	_leanTarget = std::clamp(_leanTarget, 0.0f, k_MaxLean);

	_yaw.SetDestination(_yawTarget, k_OrbitEaseTime);
	_lean.SetDestination(_leanTarget, k_OrbitEaseTime);
	_yaw.Update(dt);
	_lean.Update(dt);
	_target = OrbitPose(_yaw.GetValue(), _lean.GetValue());
	_originTime = 0.0f;
	_focusTime = 0.0f;

	// Clicking a door goes through it
	if (input.button != 0 && !_wasPressed && input.door.has_value() && *input.door != k_ScrollWall)
	{
		// The door of a room that is not open leaves the camera where it is, and the help tells the player so. The
		// way out is always open.
		if (const auto room = RoomBehindDoor(*input.door); room.has_value())
		{
			auto& temple = Locator::temple::value();
			if (!temple.IsRoomAvailable(*room))
			{
				temple.TellRoomNotAvailable();
				return;
			}
		}
		_doorStart = TurnToDoor(k_DoorApproach, *input.door);
		_doorEnd = TurnToDoor(k_DoorEntry, *input.door);
		_doorRoom = RoomBehindDoor(*input.door);
		_nextState = State::ThroughDoor;
	}
}

void TempleCameraModel::UpdateLook(float dt, const Input& input)
{
	// The creature's room: the camera stays where the room's path ends, and dragging turns the look with the
	// mouse, by as much as the lens sees across the screen
	const auto& actions = Locator::gameActionSystem::value();
	if (input.button != 0 && !_wasPressed)
	{
		_lookAtPress = _look;
	}
	if ((input.button == 0 && _wasPressed) || input.button == 2)
	{
		_lookAtPress = _look;
	}
	if (input.button != 0 && Locator::windowing::has_value())
	{
		const auto size = glm::vec2(Locator::windowing::value().GetSize());
		const float width = size.x;
		// What the lens sees across this screen: wider on a screen wider than 4:3, so the look still follows the mouse
		const float across =
		    2.0f * std::tan(camera_lens::HorizontalFieldOfView(glm::radians(k_FieldOfView), size.x / size.y) * 0.5f);
		_look = (across * ((_pressMouse - input.mouse) / width)) + _lookAtPress;
	}

	// The arrow keys speed up its turn, which slows as they're let go
	if (actions.Get(BindableActionMap::MOVE_LEFT))
	{
		_lookSpeed.x -= dt * k_LookAcceleration;
	}
	if (actions.Get(BindableActionMap::MOVE_RIGHT))
	{
		_lookSpeed.x += dt * k_LookAcceleration;
	}
	if (actions.Get(BindableActionMap::MOVE_FORWARDS))
	{
		_lookSpeed.y -= dt * k_LookAcceleration;
	}
	if (actions.Get(BindableActionMap::MOVE_BACKWARDS))
	{
		_lookSpeed.y += dt * k_LookAcceleration;
	}
	_look += _lookSpeed;
	_lookAtPress += _lookSpeed;
	// It never looks straight up or down
	_look.y = std::clamp(_look.y, -k_LookMaxPitch, k_LookMaxPitch);
	_lookAtPress.y = std::clamp(_lookAtPress.y, -k_LookMaxPitch, k_LookMaxPitch);
	// The headings come back into one turn a whole turn at a time
	for (auto* heading : {&_lookAtPress.x, &_look.x})
	{
		while (*heading < 0.0f)
		{
			*heading += glm::two_pi<float>();
		}
		while (*heading > glm::two_pi<float>())
		{
			*heading -= glm::two_pi<float>();
		}
	}
	_lookSpeed *= x87::Exp(-k_LookSlowing * dt);

	_target = LookPose(_lookFrom, _look.x, _look.y);
	_originTime = k_LookEaseTime;
	_focusTime = k_LookEaseTime;
	// Clicking the room's door walks back through it, from halfway between the camera and the door
	// TODO(raffclar): the creature's room also lets a press held from the door walk back once it is near, and zooms to
	//                 the scrolls of the creature's attributes
	if (input.button != 0 && !_wasPressed && input.doorBack == k_CreatureDoor)
	{
		WalkBackThrough(k_CreatureDoor);
	}
}

std::optional<glm::vec3> TempleCameraModel::CaveMeshPoint(uint32_t index)
{
	// The room's object gives the place of its mesh's point
	const entt::id_type creatureRoom = entt::hashed_string("temple/interior/creature_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(creatureRoom) || index >= meshes.Handle(creatureRoom)->GetExtraMetrics().size())
	{
		return std::nullopt;
	}
	return glm::vec3(meshes.Handle(creatureRoom)->GetExtraMetrics()[index][3]);
}

void TempleCameraModel::UpdateCaveTargets(float dt, const Input& input)
{
	using namespace CreatureCaveTargets;
	_caveTarget = !(_state == State::Look && !_lookingAtSubMesh) ? std::nullopt
	              : input.overCaveCreature                       ? std::optional(Target::Creature)
	                                                             : TargetAt(input.caveTargets, input.mouse, input.screen.y);

	// A press anywhere zooms back, but from the creature, and a click on a target, while the camera is all the way back,
	// zooms to it
	if (input.button != 0 && !_wasPressed)
	{
		if (_caveZoomedTo != Target::Creature)
		{
			_caveZoomTarget = 0.0f;
			_caveZoomedTo.reset();
		}
		_pressDragged = false;
	}
	if (input.button != 0 && glm::any(glm::greaterThan(glm::abs(input.mouse - _pressMouse), glm::vec2(k_DragDistance))))
	{
		_pressDragged = true;
	}
	const bool clicked = (input.button == 0 && _wasPressed) || input.button == 2;
	if (clicked && !_pressDragged && _caveZoom == 0.0f && _caveTarget.has_value())
	{
		ZoomToCaveTarget(*_caveTarget);
	}
	const auto& actions = Locator::gameActionSystem::value();
	if (actions.GetAny(BindableActionMap::MOVE_LEFT, BindableActionMap::MOVE_RIGHT, BindableActionMap::MOVE_FORWARDS,
	                   BindableActionMap::MOVE_BACKWARDS))
	{
		LeaveCaveCreature();
		_caveZoomTarget = 0.0f;
		_caveZoomedTo.reset();
	}
	// Zoomed onto the creature, the camera looks at it as the tattoo editor turns the view, and goes back once the editor
	// has closed
	if (_caveZoomedTo == Target::Creature)
	{
		const bool open = Locator::tattooEditorSystem::has_value() && Locator::tattooEditorSystem::value().IsOpen();
		if (open)
		{
			const auto& view = Locator::tattooEditorSystem::value().GetView();
			_caveLook = {view.eye, view.focus};
		}
		else
		{
			_caveZoomTarget = 0.0f;
			_caveZoomedTo.reset();
		}
	}

	// It zooms in over a second and back in half that, and zoomed to the exit, the player leaves the temple
	if (_caveZoom < _caveZoomTarget)
	{
		_caveZoom = std::min(_caveZoom + dt, _caveZoomTarget);
		if (_caveZoom == 1.0f && _caveZoomedTo == Target::Exit)
		{
			Locator::temple::value().RequestLeave();
		}
	}
	else if (_caveZoom > _caveZoomTarget)
	{
		_caveZoom = std::max(_caveZoom - 2.0f * dt, _caveZoomTarget);
	}
}

void TempleCameraModel::ZoomToCaveTarget(CreatureCaveTargets::Target target)
{
	using namespace CreatureCaveTargets;
	switch (target)
	{
	case Target::Exit:
		// The camera goes to the exit as the temple fades to white
		_caveLook = {k_ExitEye, k_ExitPlace};
		Locator::temple::value().FadeToWhite();
		break;
	case Target::Belts:
	case Target::Medals:
	{
		// The belts and the medals start their help
		temple_help::GameScripts scripts;
		temple_help::ZoomToCaveTarget(scripts, target);
		const auto offset = static_cast<uint32_t>(target);
		const auto eye = CaveMeshPoint(k_FirstEyePoint + offset);
		const auto look = CaveMeshPoint(k_FirstLookPoint + offset);
		if (!eye.has_value() || !look.has_value())
		{
			return;
		}
		_caveLook = {*eye, *look};
		break;
	}
	case Target::Creature:
	{
		// The creature opens the tattoo editor on the player's creature, the camera taking the editor's view of it: from
		// in front of it and above
		if (!Locator::creatureCaveSystem::has_value() || !Locator::tattooEditorSystem::has_value())
		{
			return;
		}
		const auto& cave = Locator::creatureCaveSystem::value();
		const auto creature = cave.GetCreature();
		const auto view = cave.GetCaveCreatureView();
		if (!creature.has_value() || !view.has_value())
		{
			return;
		}
		Locator::tattooEditorSystem::value().Open(*creature, *view);
		_caveLook = {view->eye, view->focus};
		break;
	}
	default:
		// The fourth place does nothing
		return;
	}
	_caveZoomTarget = 1.0f;
	_caveZoomedTo = target;
}

void TempleCameraModel::LeaveCaveCreature()
{
	// Leaving the creature hides the tattoo editor as it is
	if (_caveZoomedTo == CreatureCaveTargets::Target::Creature && Locator::tattooEditorSystem::has_value() &&
	    Locator::tattooEditorSystem::value().IsOpen())
	{
		Locator::tattooEditorSystem::value().Close();
	}
}

void TempleCameraModel::ResetCaveTargets()
{
	LeaveCaveCreature();
	_caveTarget.reset();
	_caveZoomedTo.reset();
	_caveZoom = 0.0f;
	_caveZoomTarget = 0.0f;
}

void TempleCameraModel::ResetPictureCamera(Room room)
{
	// Looking along the end of the room's path, half leaning and low
	auto& picture = _pictures.at(static_cast<size_t>(room));
	picture.yawTarget = PictureStartYaw(PathEnd(room));
	picture.leanTarget = k_PictureStartLean;
	picture.heightTarget = k_PictureStartHeight;
}

void TempleCameraModel::UpdatePictureCamera(float dt)
{
	const auto room = GetRoom();
	const auto& pictureRoom = *PictureRoomOf(room);
	auto& picture = _pictures.at(static_cast<size_t>(room));
	picture.leanTarget = std::clamp(picture.leanTarget, 0.0f, 1.0f);
	picture.heightTarget = std::clamp(picture.heightTarget, k_PictureMinHeight, pictureRoom.maxHeight);
	picture.yaw.SetDestination(picture.yawTarget, k_PictureYawEaseTime);
	picture.lean.SetDestination(picture.leanTarget, k_PictureLeanEaseTime);
	picture.height.SetDestination(picture.heightTarget, k_PictureHeightEaseTime);
	picture.yaw.Update(dt);
	picture.lean.Update(dt);
	picture.height.Update(dt);

	// The higher the camera, the more it looks down. Under a dialog it levels out, other than in the library, where the
	// story's history is read in a dialog while the player looks about.
	float leanForHeight = (picture.heightTarget - k_PictureMinHeight) * k_PictureLeanPerHeight + k_PictureLowLean;
	if (_dialogOpen && !pictureRoom.aroundPathEnd)
	{
		leanForHeight = 0.0f;
	}
	picture.leanTarget += (leanForHeight - picture.leanTarget) * (1.0f - std::exp(-dt));
}

void TempleCameraModel::UpdatePictureOrbit(float dt, const Input& input)
{
	// A room of pictures, with the player in control
	const auto room = GetRoom();
	const auto& pictureRoom = *PictureRoomOf(room);
	auto& picture = _pictures.at(static_cast<size_t>(room));
	const auto pressKind = KindOf(_pressHit);

	// Dragging the floor or the walls turns the room and raises the camera to keep them under the mouse. A dialog takes
	// the mouse.
	if (!_dialogOpen && input.button != 0 && (pressKind == HitKind::Floor || pressKind == HitKind::Wall) &&
	    input.hit.has_value())
	{
		float turn = _pressHit->angle - input.hit->angle;
		if (turn < -glm::pi<float>())
		{
			turn += glm::two_pi<float>();
		}
		if (turn > glm::pi<float>())
		{
			turn -= glm::two_pi<float>();
		}
		picture.yawTarget = picture.yaw.GetValue() + turn;
		picture.heightTarget = _pressHit->height - input.hit->height + picture.height.GetValue();
	}

	if (const auto turn = PictureTurnByItself(_pictureOrbitTime, dt, _dialogOpen, pictureRoom.aroundPathEnd,
	                                          room == Room::Multi && _futureWordsShown);
	    turn.has_value())
	{
		picture.yawTarget += *turn;
	}
	else
	{
		UpdatePictureKeys(dt, picture);
	}

	_target = PictureOrbitPose(room, picture.yaw.GetValue(), picture.lean.GetValue(), picture.height.GetValue(),
	                           PathEnd(room).origin);
	_originTime = 0.0f;
	_focusTime = 0.0f;

	// Clicking the room's door walks back through it, from halfway between the camera and the door
	if (input.button != 0 && !_wasPressed && input.doorBack == pictureRoom.door)
	{
		WalkBackThrough(pictureRoom.door);
	}
	// TODO(raffclar): a room of pictures can also look at its pictures
}

std::optional<float> TempleCameraModel::PictureTurnByItself(float& turnTime, float dt, bool dialogOpen, bool aroundPathEnd,
                                                            bool futureWordsShown)
{
	// Under a dialog the room turns slowly by itself, speeding up over a second from when it starts, rather than with
	// the arrow keys. The library keeps still. With no dialog, the future room turns by itself in the same way while
	// it shows its words.
	if (!dialogOpen && !futureWordsShown)
	{
		return std::nullopt;
	}
	const float speedUp = std::clamp(turnTime, 0.0f, 1.0f);
	turnTime += dt;
	if (dialogOpen && aroundPathEnd)
	{
		return 0.0f;
	}
	return speedUp * dt * k_PictureDialogTurnSpeed;
}

void TempleCameraModel::UpdatePictureKeys(float dt, PictureCamera& picture)
{
	// The arrow keys turn, and lean and raise or lower the camera, which the lean then follows
	const auto& actions = Locator::gameActionSystem::value();
	if (actions.Get(BindableActionMap::MOVE_LEFT))
	{
		picture.yawTarget += dt;
	}
	if (actions.Get(BindableActionMap::MOVE_RIGHT))
	{
		picture.yawTarget -= dt;
	}
	float tilt = picture.leanTarget * 2.0f - 1.0f;
	if (actions.Get(BindableActionMap::MOVE_FORWARDS))
	{
		tilt -= dt * k_PictureLeanSpeed;
		picture.heightTarget -= tilt * dt * k_PictureRiseSpeed;
	}
	if (actions.Get(BindableActionMap::MOVE_BACKWARDS))
	{
		tilt += dt * k_PictureLeanSpeed;
		picture.heightTarget -= tilt * dt * k_PictureRiseSpeed;
	}
	picture.leanTarget = tilt * 0.5f + 0.5f;
}

void TempleCameraModel::WalkBackThrough(uint32_t door)
{
	// The rooms' cameras head halfway from where they are to the door, at its height, then through it
	const auto through = TurnToDoorBack(k_DoorBack, door);
	_doorStart = {(through.origin + _target.origin) * 0.5f, (through.focus + _target.focus) * 0.5f};
	_doorStart.origin.y = through.origin.y;
	_doorEnd = through;
	_doorRoom = Room::Main;
	_nextState = State::ThroughDoor;
}

TempleCameraModel::State TempleCameraModel::ControlOf(Room room) const
{
	if (room == Room::Main)
	{
		return State::Orbit;
	}
	if (room == Room::CreatureCave)
	{
		return State::Look;
	}
	return State::PictureOrbit;
}

TempleCameraModel::Pose TempleCameraModel::PathEnd(Room room) const
{
	const auto& path = _paths.at(static_cast<size_t>(room));
	if (!path)
	{
		return {_origin.GetValue(), _focus.GetValue()};
	}
	const auto end = path->SampleAt(path->GetDuration());
	return {end.position, end.focus};
}

TempleCameraModel::Pose TempleCameraModel::ControlPose(Room room) const
{
	switch (ControlOf(room))
	{
	case State::Orbit:
		// The main room's camera eases into its start, unturned
		return OrbitPose(0.0f, k_StartLean);
	case State::PictureOrbit:
	{
		const auto& picture = _pictures.at(static_cast<size_t>(room));
		return PictureOrbitPose(room, picture.yaw.GetValue(), picture.lean.GetValue(), picture.height.GetValue(),
		                        PathEnd(room).origin);
	}
	default:
		// The creature's room's camera takes over where its path ends
		return PathEnd(room);
	}
}

void TempleCameraModel::UpdateThroughDoor(float dt, const Input& input)
{
	if (input.button != 0 && !_wasPressed)
	{
		_doorStage = DoorStage::Skipped;
	}
	// The camera waits by the door, then goes through it
	if (_doorStage == DoorStage::Approaching && _doorTime > 1.5f)
	{
		_target = _doorEnd;
		_origin.SetDestination(_doorEnd.origin, 7.0f);
		_focus.SetDestination(_doorEnd.focus, 7.0f);
		_doorStage = DoorStage::Entering;
	}
	_originTime = 3.0f;
	_focusTime = 3.0f;
	_doorTime += dt;
	// Halfway along, with the doorway near open, the camera is into the room
	if (_doorTime > 5.0f)
	{
		_doorStage = DoorStage::Through;
	}
	_easeToTarget = false;
}

void TempleCameraModel::ChangeState()
{
	if (_nextState == _state)
	{
		return;
	}
	switch (_nextState)
	{
	case State::Intro:
		_introTime = 0.0f;
		_blendTime = 0.0f;
		break;
	case State::Orbit:
		// The main room's camera starts the player facing the scroll, leaning a little up
		_yawTarget = 0.0f;
		_leanTarget = k_StartLean;
		_yaw.Reset(0.0f);
		_lean.Reset(k_StartLean);
		break;
	case State::Look:
	{
		ResetCaveTargets();
		// The creature's room: the look starts along the end of the room's path
		const auto end = PathEnd(GetRoom());
		_lookFrom = end.origin;
		_look = HeadingAndPitch(end);
		_lookAtPress = _look;
		_lookSpeed = glm::vec2(0.0f);
		break;
	}
	case State::PictureOrbit:
		// A room of pictures starts its orbit afresh
		_pictureOrbitTime = 0.0f;
		break;
	case State::ThroughDoor:
		_target = _doorStart;
		_origin.SetDestination(_doorStart.origin, 3.0f);
		_focus.SetDestination(_doorStart.focus, 3.0f);
		_doorTime = 0.0f;
		_doorStage = DoorStage::Approaching;
		_easeToTarget = false;
		break;
	}
	_state = _nextState;
}

void TempleCameraModel::LookAtSubMesh(glm::vec3 position, glm::vec3 lookAt)
{
	// Looking at a scroll wooshes unless the camera is looking at something already
	if (!_lookingAtSubMesh && Locator::audio::has_value())
	{
		constexpr std::array k_Wooshes {audio::SoundId::G_Woosh_01, audio::SoundId::G_Woosh_02, audio::SoundId::G_Woosh_03,
		                                audio::SoundId::G_Woosh_04};
		const auto ticks = machine_clock::Ticks();
		audio::PlayGameSoundEffect(static_cast<entt::id_type>(k_Wooshes.at(static_cast<size_t>(ticks & 3))), std::nullopt);
	}
	_subMeshLook = Pose {position, lookAt};
	_lookingAtSubMesh = true;
}

void TempleCameraModel::FollowTemple()
{
	// The temple draws the room the camera is heading into, and takes the player into it
	auto& temple = Locator::temple::value();
	auto& doors = temple.GetDoors();
	if (_state != State::ThroughDoor)
	{
		temple.SetTransitionRoom(std::nullopt);
		if (_state == State::Orbit || _state == State::Look || _state == State::PictureOrbit)
		{
			// Any door the player has the room's camera back by is shut
			doors.FastClose();
		}
		else if (_state == State::Intro && IntroClosesDoors(_introTime))
		{
			doors.Close(k_IntroDoorCloseRate);
		}
		return;
	}
	switch (_doorStage)
	{
	case DoorStage::Approaching:
		temple.SetTransitionRoom(std::nullopt);
		doors.FastClose();
		break;
	case DoorStage::Entering:
		temple.SetTransitionRoom(InMainRoom() ? _doorRoom : std::optional<Room>(Room::Main));
		if (InMainRoom())
		{
			// The doorway waits a moment, then swings open ahead of the camera
			doors.Open(_doorRoom.has_value() ? TempleDoors::LeafOf(*_doorRoom) : std::nullopt, k_DoorOpenFrom, k_DoorOpenRate);
		}
		else
		{
			// Back into the main room, its doorway to this room swings open quickly ahead of the camera, and the main room's
			// camera turns it round to close behind it
			doors.Open(TempleDoors::LeafOf(GetRoom()), k_DoorBackOpenFrom, k_DoorBackOpenRate);
		}
		break;
	case DoorStage::Through:
	case DoorStage::Skipped:
		// Through, the doorway swings on open behind the camera, until the next room's camera turns it round to close.
		// Skipping cuts to the room, which shuts it.
		temple.SetTransitionRoom(std::nullopt);
		if (!_doorRoom.has_value())
		{
			temple.RequestLeave();
		}
		else if (_doorStage == DoorStage::Through)
		{
			// The door's path runs on into the room's own, unbroken
			StartIntro(*_doorRoom, true);
		}
		else
		{
			// Clicking on the way through cuts to the room, behind the temple's fade
			temple.GoToRoom(*_doorRoom);
		}
		break;
	}
}

void TempleCameraModel::SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) {}

glm::vec3 TempleCameraModel::GetTargetOrigin() const
{
	return _target.origin;
}

glm::vec3 TempleCameraModel::GetTargetFocus() const
{
	return _target.focus;
}

std::optional<CameraModel::Lens> TempleCameraModel::GetLens() const
{
	// The temple's cameras' lens
	return Lens {.horizontalFieldOfView = k_FieldOfView, .nearClip = k_NearClip};
}
