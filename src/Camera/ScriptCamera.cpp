/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCamera.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera.h"
#include "CameraShake.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::script_camera
{

State::State()
{
	fov.SetPosition(k_DefaultFov);
}

State::~State() = default;

State& Get()
{
	return Locator::scriptState::value().Get<State>();
}

namespace
{
/// The tests' thing reader (SetThingReaderForTests; debugHooks)
struct TestReaderState
{
	ThingReader reader;
};

ThingReader& TestReader()
{
	return Locator::debugHooks::value().Get<TestReaderState>().reader;
}

/// The game angle of a wall hugger in openblack: an animal's AnimalBrain::angle is the angle itself; a villager keeps
/// it in WallHug::yAngle as ConvertGameAngleTo3D(a) (ECS/Villager/VillagerCore.h), turned back here (approximate:
/// rounded to the nearest 2048th). A creature's is not kept (no creature body nor AI): none (approximate)
std::optional<uint16_t> WallHugAngleOf(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* brain = registry.TryGet<const ecs::components::AnimalBrain>(entity); brain != nullptr)
	{
		return brain->angle;
	}
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		if (const auto* wallHug = registry.TryGet<const ecs::components::WallHug>(entity); wallHug != nullptr)
		{
			const auto turns = std::lround(wallHug->yAngle * static_cast<float>(gutils::k_GameAngleCircle) / k_TwoPi);
			return static_cast<uint16_t>(static_cast<uint32_t>(turns) & static_cast<uint32_t>(gutils::k_GameAngleMask));
		}
	}
	return std::nullopt;
}

/// The thing as the original reads it, from the entities registry: a valid entity (as the script's things in
/// CHLApi.cpp), its availability ecs::IsAvailable; anything but a flock needs a Transform (its map coords). (inferred)
/// An entity with a Transform and a Mesh is an Object with a 3D object, whose matrix is the Transform
std::optional<ThingInfo> RegistryThing(entt::entity entity)
{
	if (!Locator::entitiesRegistry::has_value() || entity == entt::null)
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	const auto infoAvailable = ecs::IsAvailable(entity);
	const auto infoHeight = ecs::object::GetHeight(entity);
	ThingInfo info {
	    .height = infoHeight,
	    .available = infoAvailable,
	};
	if (const auto* flock = registry.TryGet<const ecs::components::Flock>(entity); flock != nullptr)
	{
		// A flock's position: the leader's map coords, else the flock's own (Flock::domainCentre). The leader is the
		// first member (ECS/AnimalAI.cpp)
		info.isFlock = true;
		const auto own = map_coords::ToWorld(map_coords::FromWorld(flock->domainCentre));
		info.mapPoint = own;
		info.flockPoint = own;
		if (!flock->members.empty() && registry.Valid(flock->members.front()))
		{
			const auto leader = flock->members.front();
			info.flockPoint = map_coords::ToWorld(ecs::object::MapCoordsOf(leader));
			info.leaderHeight = ecs::object::GetHeight(leader);
		}
		return info;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	info.mapPoint = map_coords::ToWorld(ecs::object::MapCoordsOf(entity));
	info.radius2d = ecs::object::Get2DRadius(entity); // read by the dual camera
	if (const auto* drawn = registry.TryGet<const ecs::components::DrawPosition>(entity); drawn != nullptr)
	{
		info.drawnPoint = drawn->position; // the villager's / animal's matrix drawn this frame (ECS/MobileDrawing.h)
	}
	else if (registry.AllOf<ecs::components::Mesh>(entity))
	{
		info.drawnPoint = transform->position;
	}
	// A villager is not available while dying
	if (registry.AllOf<ecs::components::Villager>(entity) && ecs::villager::GetFinalState(entity) == VillagerStates::Dying)
	{
		info.available = false;
	}
	info.wallHugAngle = WallHugAngleOf(registry, entity);
	if (info.wallHugAngle.has_value())
	{
		info.facingDirection = gutils::ConvertGameAngleToScawenAngle(*info.wallHugAngle);
	}
	// A creature's facing reads its model's angle, not ported: 0 as a plain thing (approximate)
	return info;
}

std::optional<ThingInfo> ReadThing(entt::entity entity)
{
	if (entity == entt::null)
	{
		return std::nullopt;
	}
	if (const auto& reader = TestReader(); reader)
	{
		return reader(entity);
	}
	return RegistryThing(entity);
}

void DropPath(State& state)
{
	// The scripted camera path freed
	state.positionRunner.reset();
	state.track.reset();
}

/// No position thing; with "behind" the heading is 0
void StopPositionFollow(State& state)
{
	state.positionThing = entt::null;
	if (state.behind)
	{
		state.heading = 0.0f;
	}
}

/// The focus thing, else the position thing
entt::entity FocusThingOf(const State& state)
{
	return state.focusThing != entt::null ? state.focusThing : state.positionThing;
}

/// The follow's script part of the mode, as the constructors leave it
void ResetFollow(State& state)
{
	state.positionThing = entt::null;
	state.focusThing = entt::null;
	state.heading = 0.0f;
	state.pitch = 0.0f;
	state.distance = 0.0f;
	state.timeFactor = k_ScriptFollowTimeFactor;
	state.behind = true;
}

/// Where the position follows a thing to: PointFromDistanceHeadingAndPitch(point, distance, heading, pitch) with the
/// pitch clamped and kept
glm::vec3 PositionFor(State& state, const ThingInfo& thing, bool update)
{
	const auto point = FollowPoint(thing, update);
	state.pitch = FollowPitch(state.pitch);
	const float heading = FollowHeading(state.heading, state.behind, thing.wallHugAngle);
	return PointFromDistanceHeadingAndPitch(point, state.distance, heading, state.pitch);
}

/// The follow update for the script mode: zoom to town and the keys are off, so neither the zoom branch nor the keys
/// run, and it leaves after the inclusion check, whose answer it does not use
void UpdateFollow(State& state)
{
	state.distance = ClampFollowDistance(state.distance);
	const float seconds = FollowSeconds(state.modeSeconds, state.timeFactor);
	// A creature focus thing writes some globals (1 - life, parts of its body) read elsewhere: not ported
	if (const auto focus = ReadThing(FocusThingOf(state)); focus.has_value())
	{
		state.focus.SetDestinationWithTime(FollowPoint(*focus, true), seconds);
	}
	// else the computer player's hand as the focus, not ported
	if (const auto thing = ReadThing(state.positionThing); thing.has_value())
	{
		state.position.SetDestinationWithTime(PositionFor(state, *thing, true), seconds);
	}
	// else from the computer player's hand, not ported
}

/// A new dual camera pushed as the current mode
void PushDual(State& state, const DualMode& mode, const Zoomer3& origin, const Zoomer3& focus)
{
	if (!state.scriptMode && state.duals.empty())
	{
		// Over the player's mode: the camera's zoomers are the same for every mode, so the dual camera goes on from
		// wherever the player's were heading (as BeginFrom)
		state.position = origin;
		state.focus = focus;
	}
	state.duals.push_back(mode);
	state.modeSeconds = 0.0f;
}

/// The last dual camera gone with no script mode under it (released or no longer valid): the player's mode is current
/// again and goes on from the camera's zoomers, so they are handed back to the player's camera (as End). Only when this
/// module drove the camera (`drove`: not with the camera test hooks). (inferred) what the player's mode does on restart
/// was not checked
void HandBackAfterDual(const State& state, bool drove)
{
	if (drove && !state.scriptMode && state.duals.empty() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
}

/// The dual camera's update, once a frame while it is the current mode
void UpdateDualMode(State& state, DualMode& mode)
{
	// The seconds of the destinations, with no time factor
	const float pace =
	    state.modeSeconds > k_DualSettleSeconds ? 1.0f : state.modeSeconds / k_DualSettleSeconds * (1.0f - 2.0f) + 2.0f;
	// (approximate) the original reads a and b with no check (a null thing would crash it before the turn's validity
	// check drops the mode); here a thing that cannot be read leaves the zoomers alone this frame
	const auto a = ReadThing(mode.a);
	if (!a.has_value())
	{
		return;
	}
	std::optional<ThingInfo> b;
	if (mode.twoObjects)
	{
		b = ReadThing(mode.b);
		if (!b.has_value())
		{
			return;
		}
	}
	// The map coords points (x, z x 1/6553.6, the land's altitude + the altitude above it; not the 3D object nor a
	// flock's position); B is the point without the altitude above the land
	const glm::vec3 pointA = a->mapPoint;
	const glm::vec3 pointB = b.has_value() ? b->mapPoint : mode.point;
	// (B + A) x 0.5
	const glm::vec3 middle((pointB.x + pointA.x) * 0.5f, (pointB.y + pointA.y) * 0.5f, (pointB.z + pointA.z) * 0.5f);
	// (A's height + B's, 1 without B) x 0.5
	const float meanHeight = (a->height + (b.has_value() ? b->height : k_DualPointHeight)) * 0.5f;
	// The larger of A's height and B's (0 without B; A only when strictly larger)
	const float otherHeight = b.has_value() ? b->height : 0.0f;
	const float maxHeight = a->height > otherHeight ? a->height : otherHeight;
	// The focus heads for the middle raised by half the mean height (speed 0)
	const glm::vec3 focus(middle.x, meanHeight * 0.5f + middle.y, middle.z);
	state.focus.SetDestinationWithTime(focus, pace);
	mode.pitch = FollowPitch(mode.pitch); // at least k_FollowMinPitch, stored back
	// An Object's 2D radius, else 30
	const float radiusA = a->radius2d.value_or(k_DualDefaultRadius);
	const float radiusB = b.has_value() && b->radius2d.has_value() ? *b->radius2d : k_DualDefaultRadius;
	// ((|A - B| in x / z + B's radius) + A's radius) x the distance factor + the larger height x 1.4
	const float dx = pointA.x - pointB.x;
	const float dz = pointA.z - pointB.z;
	const float apart = std::sqrt(dx * dx + dz * dz);
	const float distance = ((apart + radiusB) + radiusA) * mode.distanceFactor + maxHeight * k_DualHeightFactor;
	// v = B - A; the heading is the mode's heading less v's direction (affine::GetYAngle: 0 when x^2 + z^2 <= 1e-6,
	// else affine::ArcTanOctant(-z, x)), or the mode's heading itself when |v.x| and |v.z| are both <= 0.01
	const glm::vec3 v = pointB - pointA;
	float heading = mode.heading;
	if (static_cast<double>(std::abs(v.x)) > k_DualFlatEpsilon || static_cast<double>(std::abs(v.z)) > k_DualFlatEpsilon)
	{
		// Subtracted in double precision, then rounded to float
		heading = static_cast<float>(static_cast<double>(mode.heading) - affine::GetYAngle(v));
	}
	// The position heads for the point at that distance, heading and pitch from the focus
	const auto position = PointFromDistanceHeadingAndPitch(focus, distance, heading, mode.pitch);
	state.position.SetDestinationWithTime(position, pace);
}

/// The stacked modes' validity check (once a turn) for the dual cameras: one stays while it is alive and a (and, with
/// two things, b) is there and available, else it is deleted. A current one deleted -> the new current restarts
/// (nothing for the script mode nor a dual camera); the mode's seconds are not reset. The script mode's own check is
/// its alive flag, which End keeps. The last one gone over the player's mode hands the zoomers back
/// (HandBackAfterDual)
void CheckDualModes(State& state)
{
	const auto available = [](entt::entity thing) {
		const auto info = ReadThing(thing);
		return info.has_value() && info->available;
	};
	const bool drove = Drives();
	auto& duals = state.duals;
	for (auto it = duals.begin(); it != duals.end();) // from the bottom of the stack
	{
		const bool things = available(it->a) && (!it->twoObjects || available(it->b));
		it = things && it->alive ? std::next(it) : duals.erase(it);
	}
	HandBackAfterDual(state, drove);
}
} // namespace

bool HasMode()
{
	const auto& state = Get();
	return state.scriptMode || !state.duals.empty();
}

bool ScriptModeCurrent()
{
	const auto& state = Get();
	return state.scriptMode && state.duals.empty();
}

bool DualCurrent()
{
	return !Get().duals.empty();
}

void StartDual(entt::entity a, entt::entity b, const Zoomer3& origin, const Zoomer3& focus)
{
	auto& state = Get();
	// The current mode's a and b compared. (approximate) a current point camera's b is whatever the allocation left
	// there (it is not written); here it is null
	if (!state.duals.empty() && state.duals.back().a == a && state.duals.back().b == b)
	{
		return;
	}
	DualMode mode {
	    .a = a,
	    .b = b,
	    .twoObjects = true,
	    .distanceFactor = k_DualDistanceFactor,
	};
	PushDual(state, mode, origin, focus);
}

void StartDualWithPoint(entt::entity a, const glm::vec3& point, const Zoomer3& origin, const Zoomer3& focus)
{
	auto& state = Get();
	// The current mode's a and its point (x, y, z equal) compared. (approximate) a current two things camera's point is
	// whatever the allocation left there; here (0, 0, 0)
	if (!state.duals.empty() && state.duals.back().a == a && state.duals.back().point == point)
	{
		return;
	}
	DualMode mode {
	    .a = a,
	    .point = point,
	    .twoObjects = false,
	    .distanceFactor = k_DualPointDistanceFactor,
	};
	PushDual(state, mode, origin, focus);
}

bool UpdateDual(entt::entity a, entt::entity b)
{
	auto& state = Get();
	if (state.duals.empty())
	{
		return false;
	}
	auto& mode = state.duals.back();
	mode.a = a;
	mode.b = b;
	mode.twoObjects = true;
	return true;
}

bool ReleaseDual()
{
	auto& state = Get();
	if (state.duals.empty())
	{
		return false;
	}
	const bool drove = Drives();
	state.duals.back().alive = false; // deleted
	state.duals.pop_back();           // popped off the mode stack
	state.modeSeconds = 0.0f;
	HandBackAfterDual(state, drove);
	return true;
}

void Reset()
{
	auto& state = Get();
	DropPath(state);
	ResetFollow(state);
	state.scriptMode = false;
	state.modeSeconds = 0.0f;
	state.fov.SetPosition(k_DefaultFov);
	state.duals.clear();
}

bool Begin(const glm::vec3& origin, const glm::vec3& focus)
{
	auto& state = Get();
	if (state.scriptMode) // a script mode cannot be left
	{
		return false;
	}
	DropPath(state);
	ResetFollow(state);
	state.scriptMode = true;
	state.modeSeconds = 0.0f; // a new mode
	state.position.SetPosition(origin);
	state.focus.SetPosition(focus);
	return true;
}

bool BeginFrom(const Zoomer3& origin, const Zoomer3& focus)
{
	if (!Begin(origin.GetCurrentValue(), focus.GetCurrentValue()))
	{
		return false;
	}
	auto& state = Get();
	state.position = origin; // the same zoomers, still heading where they were
	state.focus = focus;
	return true;
}

void HandBack(Zoomer3& origin, Zoomer3& focus)
{
	const auto& state = Get();
	origin = state.position;
	focus = state.focus;
}

bool End()
{
	auto& state = Get();
	ReleaseDual(); // one dual camera
	// Only a current script mode is deleted; with another mode current (a second dual camera, or the player's) "We are
	// in the wrong camera mode! - excep" and the script mode, if any, stays
	const bool wasScript = ScriptModeCurrent();
	if (!wasScript)
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "We are in the wrong camera mode! - excep");
		}
	}
	// The player's mode starts from the camera's zoomers. Only when the script mode drove the camera (with the camera
	// test hooks the player kept it)
	if (wasScript && Drives() && Locator::camera::has_value())
	{
		auto& camera = Locator::camera::value();
		HandBack(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	if (wasScript) // deleted, and a new player's mode
	{
		state.scriptMode = false;
		DropPath(state);
		ResetFollow(state);
		state.modeSeconds = 0.0f; // the new player's mode
	}
	SetFov(help::script_control::k_ScriptEndFov, help::script_control::k_ScriptEndFovTime);
	return wasScript;
}

bool Active()
{
	return Get().scriptMode;
}

bool Drives()
{
	// Test hooks (not original): OPENBLACK_CAMERA_LOCK / OPENBLACK_CAMERA_FLY hold the camera every turn for the team's
	// screenshots (Worship/WorshipDebugHooks.cpp), so the script mode leaves it to them
	static const bool s_testCameraHook =
	    std::getenv("OPENBLACK_CAMERA_LOCK") != nullptr || std::getenv("OPENBLACK_CAMERA_FLY") != nullptr;
	return HasMode() && !s_testCameraHook;
}

void SetPosition(const glm::vec3& position)
{
	auto& state = Get();
	DropPath(state);
	StopPositionFollow(state);
	state.position.SetPosition(position);
}

void SetFocus(const glm::vec3& focus)
{
	auto& state = Get();
	DropPath(state);
	state.focusThing = entt::null;
	state.focus.SetPosition(focus);
}

void MovePosition(const glm::vec3& position, float seconds)
{
	auto& state = Get();
	DropPath(state);
	StopPositionFollow(state);
	state.position.SetDestinationWithTime(position, seconds);
}

void MoveFocus(const glm::vec3& focus, float seconds)
{
	auto& state = Get();
	DropPath(state);
	state.focusThing = entt::null;
	state.focus.SetDestinationWithTime(focus, seconds);
}

void SetPositionAndFocus(const glm::vec3& position, const glm::vec3& focus)
{
	auto& state = Get();
	state.position.SetPosition(position);
	state.focus.SetPosition(focus);
}

void RunPath(int32_t number)
{
	auto& state = Get();
	// The old path and the focus follow dropped; the position follow kept
	DropPath(state);
	state.focusThing = entt::null;
	state.track = LoadCameraTrack(number); // "Cannot load track No %d" when missing
	if (state.track != nullptr)
	{
		state.positionRunner = std::make_unique<CameraWayRunner>(state.track->position);
	}
	state.pathMs = 0;
}

void SetFov(float fov, float seconds)
{
	auto& state = Get();
	// t == 0 or t < 0.001 sets it; else the quartic towards fov with speed 0
	state.fov.SetDestinationWithSpeedAndTime(fov, 0.0f, seconds);
}

// ---- Follow mode -----------------------------------------------------------------------------------------------------

float FollowSeconds(float modeSeconds, float timeFactor)
{
	// <= 2 s takes the slow start
	const float pace = modeSeconds > k_FollowSettleSeconds ? 1.0f : modeSeconds / k_FollowSettleSeconds * (1.0f - 2.0f) + 2.0f;
	return pace * timeFactor;
}

float ClampFollowDistance(float distance)
{
	if (!(distance > k_FollowMinDistance))
	{
		return k_FollowMinDistance;
	}
	return distance < k_FollowMaxDistance ? distance : k_FollowMaxDistance;
}

float FollowPitch(float pitch)
{
	return pitch > k_FollowMinPitch ? pitch : k_FollowMinPitch;
}

float FollowHeading(float heading, bool behind, std::optional<uint16_t> wallHugAngle)
{
	if (!behind || !wallHugAngle.has_value())
	{
		return heading;
	}
	// The doubled game angle in radians, less a quarter turn
	const float angle = static_cast<float>(static_cast<uint32_t>(*wallHugAngle) << 1u) * k_FollowAngleScale - k_HalfPi;
	return heading - angle;
}

float ThingViewingDistance(float height)
{
	return height * k_ThingViewingDistanceFactor;
}

glm::vec3 PointFromDistanceHeadingAndPitch(const glm::vec3& p, float distance, float heading, float pitch)
{
	// cos q kept as a float, then each product in order
	const float cosPitch = std::cos(pitch);
	const float z = std::cos(heading) * cosPitch * distance + p.z;
	const float y = std::sin(pitch) * distance + p.y;
	const float x = std::sin(heading) * cosPitch * distance + p.x;
	return {x, y, z};
}

void HeadingAndPitchFromPoints(const glm::vec3& a, const glm::vec3& b, float& heading, float& pitch)
{
	const glm::vec3 v = a - b;
	if (std::abs(static_cast<double>(v.x)) < k_VerticalEpsilon && std::abs(static_cast<double>(v.z)) < k_VerticalEpsilon)
	{
		heading = 0.0f;
		pitch = k_VerticalPitch;
		return;
	}
	// affine::GetYAngle: 0 when x^2 + z^2 <= 1e-6, else affine::ArcTanOctant(-z, x); subtracted from pi in double
	// precision, then rounded to float
	heading = static_cast<float>(static_cast<double>(k_Pi) - affine::GetYAngle(v));
	// affine::ArcTanOctant(sqrt(z^2 + x^2), y), rounded to float
	pitch = static_cast<float>(affine::ArcTanOctant(std::sqrt(v.z * v.z + v.x * v.x), v.y));
}

glm::vec3 FollowPoint(const ThingInfo& thing, bool update)
{
	if (thing.isFlock)
	{
		auto point = thing.flockPoint; // the land's altitude + its altitude above it
		if (thing.leaderHeight.has_value())
		{
			point.y = *thing.leaderHeight * 0.5f + point.y;
		}
		return point;
	}
	auto point = thing.mapPoint;
	if (update && thing.drawnPoint.has_value())
	{
		point = *thing.drawnPoint; // the 3D object's position
	}
	point.y = thing.height * 0.5f + point.y;
	return point;
}

glm::vec3 FacePoint(const ThingInfo& thing)
{
	auto point = thing.mapPoint;
	point.y = thing.height * 0.5f + point.y;
	return point;
}

glm::vec3 FacePosition(const glm::vec3& focus, float facing, float distance)
{
	float heading = facing;
	if (!(heading < k_FaceMaxHeading))
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "Invalid heading"); // and on
		}
	}
	while (heading > k_TwoPi)
	{
		heading -= k_TwoPi;
	}
	return PointFromDistanceHeadingAndPitch(focus, distance, heading, k_FacePitch);
}

void FocusFollow(entt::entity thing)
{
	auto& state = Get();
	DropPath(state);
	// An available thing is followed, else nothing
	const auto info = ReadThing(thing);
	state.focusThing = info.has_value() && info->available ? thing : entt::null;
}

void PositionFollow(entt::entity thing)
{
	auto& state = Get();
	state.positionThing = thing;
	if (thing != entt::null)
	{
		// From the zoomers' destinations, the position from the focus
		HeadingAndPitchFromPoints(state.position.GetDestination(), state.focus.GetDestination(), state.heading, state.pitch);
		const auto info = ReadThing(thing);
		state.distance = ThingViewingDistance(info.has_value() ? info->height : 0.0f);
	}
	if (state.behind)
	{
		state.heading = 0.0f;
	}
}

void FocusAndPositionFollow(entt::entity thing, float distance)
{
	auto& state = Get();
	state.positionThing = thing;
	if (thing != entt::null)
	{
		HeadingAndPitchFromPoints(state.position.GetDestination(), state.focus.GetDestination(), state.heading, state.pitch);
		state.distance = distance;
	}
}

void PlaceFollowNow()
{
	auto& state = Get();
	// No zoom: the follow's points, set at once
	if (const auto focus = ReadThing(FocusThingOf(state)); focus.has_value())
	{
		state.focus.SetPosition(FollowPoint(*focus, false));
	}
	// else the computer player's hand, not ported
	if (const auto thing = ReadThing(state.positionThing); thing.has_value())
	{
		state.position.SetPosition(PositionFor(state, *thing, false)); // no distance clamp
	}
	// else the computer player's hand, not ported
	state.modeSeconds = k_FollowSettleSeconds;
}

void SetFollowProperties(float distance, float timeFactor, float heading, bool behind)
{
	auto& state = Get();
	state.distance = distance;
	state.timeFactor = timeFactor;
	state.behind = behind;
	state.heading = heading;
}

void Validate()
{
	auto& state = Get();
	CheckDualModes(state); // the stacked modes' check comes just before
	const auto available = [](entt::entity thing) {
		const auto info = ReadThing(thing);
		return info.has_value() && info->available;
	};
	if (state.focusThing != entt::null && !available(state.focusThing))
	{
		state.focusThing = entt::null;
	}
	if (state.positionThing != entt::null && !available(state.positionThing))
	{
		state.positionThing = entt::null;
	}
}

std::optional<FacePoints> FaceObject(entt::entity thing, float distance)
{
	const auto info = ReadThing(thing);
	if (!info.has_value())
	{
		// "no object to face", then the original reads through the null thing (inferred: it would crash; here nothing
		// is done)
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			SPDLOG_LOGGER_ERROR(logger, "no object to face");
		}
		return std::nullopt;
	}
	FacePoints points {};
	points.focus = FacePoint(*info);
	points.position = FacePosition(points.focus, info->facingDirection, distance);
	return points;
}

bool ScriptArrived()
{
	const auto& state = Get();
	// A current dual camera answers with the zoomers' arrival, path or not
	if (state.duals.empty() && state.track != nullptr)
	{
		return state.track->position.duration <= state.pathMs;
	}
	// Both zoomers at their destinations
	const auto dp = state.position.GetCurrentValue() - state.position.GetDestination();
	const auto df = state.focus.GetCurrentValue() - state.focus.GetDestination();
	return glm::dot(dp, dp) < k_ArrivedDistanceSquared && glm::dot(df, df) < k_ArrivedDistanceSquared;
}

namespace
{
void UpdatePath(State& state, uint32_t gameMs)
{
	// The game ms of the frame, the sample clamped to the position way's duration, the focus on the focus way at the
	// position runner's segment and parameter, and both set at once (SetPositionAndFocus)
	state.pathMs += static_cast<int32_t>(gameMs);
	const auto sample = std::clamp(state.pathMs, 0, state.track->position.duration);
	const auto position = state.positionRunner->Get(sample);
	const auto focus = state.track->focus.Bezier(state.positionRunner->Segment(), state.positionRunner->Parameter());
	SetPositionAndFocus(position, focus);
}
} // namespace

void Frame(float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	const float dt = std::min(cameraSeconds, k_MaxFrameSeconds);
	state.modeSeconds += dt; // before the mode's update
	if (!state.duals.empty())
	{
		UpdateDualMode(state, state.duals.back()); // the current mode's update
	}
	else if (state.scriptMode)
	{
		// Validate runs once a turn (Game.cpp); until then a thing that has gone is not read (ReadThing gives nothing
		// for an invalid entity)
		if (state.track != nullptr)
		{
			UpdatePath(state, gameMs);
		}
		UpdateFollow(state);
	}
	state.position.Update(dt);
	state.focus.Update(dt);
	// The position's destination kept inside the disc of the world
	const glm::vec3 centre(k_DiscCentre, 0.0f, k_DiscCentre);
	const auto d = state.position.GetDestination() - centre;
	if (const float d2 = glm::dot(d, d); d2 > k_DiscRadiusSquared)
	{
		state.position.SetDestinationWithTime(d / (std::sqrt(d2) * k_DiscScale) + centre, k_DiscSeconds);
	}
	state.fov.Update(gameSeconds); // game time, not the camera's seconds
}

namespace
{
/// The last drawn point without a NaN (DrawnCamera)
struct DrawnCameraState
{
	glm::vec3 goodOrigin {1000.0f, 0.0f, 1000.0f};
	glm::vec3 goodFocus {1000.0f, 0.0f, 1000.0f};
};
} // namespace

Drawn DrawnCamera(const std::function<float(float, float)>& groundAt)
{
	const auto& state = Get();
	auto& good = Locator::scriptState::value().Get<DrawnCameraState>();
	Drawn drawn {state.position.GetCurrentValue(), state.focus.GetCurrentValue()};
	// A NaN component -> the last good one
	for (int i = 0; i < 3; ++i)
	{
		drawn.origin[i] = std::isnan(drawn.origin[i]) ? good.goodOrigin[i] : drawn.origin[i];
		drawn.focus[i] = std::isnan(drawn.focus[i]) ? good.goodFocus[i] : drawn.focus[i];
	}
	good.goodOrigin = drawn.origin;
	good.goodFocus = drawn.focus;
	// Position and focus never the same point
	if (const auto d = drawn.origin - drawn.focus; glm::dot(d, d) < k_ArrivedDistanceSquared)
	{
		drawn.origin.x -= 1.0f;
		drawn.origin.y += 1.0f;
	}
	// Measured under the nudged position (not in the free camera mode, nor while a camera flag turns it off: neither
	// exists here)
	if (const float lift = groundAt(drawn.origin.x, drawn.origin.z) + k_GroundClearance - drawn.origin.y; lift > 0.0f)
	{
		drawn.origin.y += lift;
		drawn.focus.y += lift;
	}
	return drawn;
}

namespace
{
/// The debug camera (the debug window's free camera; scriptState)
struct DebugCameraState
{
	int32_t mode {0};
	glm::vec3 position {0.0f};
	glm::vec3 focus {0.0f};
};

DebugCameraState& DebugCamera()
{
	return Locator::scriptState::value().Get<DebugCameraState>();
}
} // namespace

void SetDebugCameraMode(int32_t mode)
{
	DebugCamera().mode = mode;
}

void SetDebugCameraPosition(const glm::vec3& position)
{
	DebugCamera().position = position;
}

void SetDebugCameraFocus(const glm::vec3& focus)
{
	DebugCamera().focus = focus;
}

int32_t DebugCameraMode()
{
	return DebugCamera().mode;
}

void ApplyShake(Camera& camera, const glm::vec3& lastDrawn)
{
	// Outside the citadel and not playing back, whatever the mode: the shake moves the drawn camera only, from the
	// camera drawn the frame before; the camera's zoomers keep their values. openblack: Camera::SetDrawOffset, which
	// GetOrigin/GetFocus(Current) and the view add
	auto position = camera.GetOriginZoomer().GetCurrentValue();
	auto focus = camera.GetFocusZoomer().GetCurrentValue();
	const bool insideCitadel = game_clock::IsInsideCitadel();
	if (insideCitadel)
	{
		camera.SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	}
	else
	{
		const auto base = position;
		const auto baseFocus = focus;
		// The debug camera replaces the drawn position (mode 2) and focus (1 and 2) first
		if (DebugCamera().mode == 2)
		{
			position = DebugCamera().position;
		}
		if (DebugCamera().mode == 1 || DebugCamera().mode == 2)
		{
			focus = DebugCamera().focus;
		}
		camera_shake::Adjust(lastDrawn, position, focus);
		camera.SetDrawOffset(position - base, focus - baseFocus); // 0 when no shake moved it (the offset stays otherwise)
	}
	// The shakes' clock with the frame's real time, once a drawn frame (inferred: after the camera, as the graphics
	// engine runs before the draw)
	camera_shake::Tick(game_clock::FrameRealMs());
}

bool UpdateCamera(Camera& camera, float cameraSeconds, uint32_t gameMs, float gameSeconds)
{
	auto& state = Get();
	Frame(cameraSeconds, gameMs, gameSeconds);

	// Inside the citadel the drawn camera stays
	const bool insideCitadel = game_clock::IsInsideCitadel();
	const bool drive = Drives() && !insideCitadel;
	if (drive)
	{
		auto drawn = DrawnCamera([](float x, float z) {
			return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : -1e30f;
		});
		// The shake is ApplyShake's, once a frame for whichever camera is drawn
		camera.SetOrigin(drawn.origin);
		camera.SetFocus(drawn.focus);
	}
	// The shakes tick in ApplyShake, once a drawn frame

	// The field of view changed to the zoomer's value (not inside the citadel); openblack keeps it in the config's
	// degrees, which every rebuild of the projection reads
	if (!insideCitadel && state.fov.value != state.appliedFov && Locator::config::has_value() &&
	    Locator::windowing::has_value())
	{
		state.appliedFov = state.fov.value;
		auto& config = Locator::config::value();
		config.cameraXFov = state.fov.value == k_DefaultFov ? 70.0f : glm::degrees(state.fov.value);
		camera.SetProjectionMatrixPerspective(config.cameraXFov, Locator::windowing::value().GetAspectRatio(),
		                                      config.cameraNearClip, config.cameraFarClip);
	}
	return drive;
}

namespace detail
{
void SetThingReaderForTests(ThingReader reader)
{
	TestReader() = std::move(reader);
}
} // namespace detail

} // namespace openblack::script_camera
