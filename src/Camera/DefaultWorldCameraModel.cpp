/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DefaultWorldCameraModel.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <ranges>
#include <tuple>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/polar_coordinates.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "Camera.h"
#include "CameraLimits.h"
#include "CameraZones.h"
#include "Common/MachineClock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "Help/HelpProfile.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace std::chrono_literals;

// TODO(#708): Add to global configurations
// Vanilla reads DirectInput's wheel, 120 per notch, and zooms by half of it. Zoom keys without a wheel turn count as one
// notch every frame.
constexpr auto k_WheelZoomPerNotch = 60.0f;
// Zoom and keyboard movement steps are scaled by three times the camera's height above its focus, within these bounds.
// Below the focus the zoom scale stops at k_ZoomScaleMaxBelowFocus.
constexpr auto k_StepScaleHeightFactor = 3.0f;
constexpr auto k_StepScaleMin = 60.0f;
constexpr auto k_ZoomScaleMax = 2000.0f;
constexpr auto k_ZoomScaleMaxBelowFocus = 4.0f * k_StepScaleMin;
constexpr auto k_InteractionSpeedMultiplier = 400.0f;
// Vanilla black and white uses a pretty bad PI/2 approximation
constexpr auto k_CameraModelHalfPi = 1.53938043f;
constexpr auto k_RotateOnSpeedMultiplier = glm::vec2(1.9f, -1.7f);
constexpr auto k_TwoButtonZoomFactor = 1.9f;
// Both buttons turn the camera by the same amount for the mouse's movement across, once it has moved far enough
constexpr auto k_TwoButtonTurnFactor = 1.9f;
// The tilt of a unit of pitch input, in radians
constexpr auto k_PitchPerInput = 0.002f;
// The clear view eases in and out over this long, and the hand grips while it is further in than this
constexpr auto k_ClearViewSeconds = 0.5f;
constexpr auto k_ClearViewGrips = 0.01f;
constexpr auto k_MinimalCameraAnimationDuration = 1'500'000us;
constexpr auto k_FlightHeightFactor = 0.1f;
// The self-tilting camera eases to its place over a second
constexpr auto k_SelfTiltEaseSeconds = 1.0f;

glm::vec3 EulerFromPoints(glm::vec3 p0, glm::vec3 p1)
{
	const auto diff = p0 - p1;
	// If the camera is directly above the focus point, set pitch to 90 degrees.
	if (glm::all(glm::lessThan(glm::abs(glm::xz(diff)), glm::vec2(0.01f, 0.01f))))
	{
		return {0.0f, k_CameraModelHalfPi, 0.0f};
	}
	// Otherwise, calculate yaw and pitch based on the direction to the focus point.
	return {glm::pi<float>() - glm::atan(diff.x, -diff.z), glm::atan(diff.y, glm::length(glm::xz(diff))), 0.0f};
}

/// Calculates the projection length of a vector onto another vector.
///
/// Given three points `p1`, `p2`, and `p3`, this function computes the projection length
/// of the vector from `p1` to `p3` onto the direction defined by the vector from `p1` to `p2`.
/// The result represents how much `p3` is "along" the direction from `p1` to `p2`, scaled
/// by the magnitude of the vector from `p1` to `p3`.
///
/// # Arguments
///
/// * `p1` - The origin point.
/// * `p2` - The point defining the direction vector.
/// * `p3` - The point whose projection onto the direction from `p1` to `p2` is calculated.
///
/// # Returns
///
/// The projection length of `p3` onto the direction from `p1` to `p2`.
float PointDistanceAlongLineSegment(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3)
{
	const glm::vec3 v12 = p2 - p1; // Vector from p1 to p2
	const glm::vec3 v13 = p3 - p1; // Vector from p1 to p3

	// Normalize v12 to get its direction
	const glm::vec3 u12 = glm::normalize(v12);

	// Project v13 onto u12 to get the projection length from p1
	return glm::abs(glm::dot(v13, u12)); // Using glm::abs to ensure the distance is non-negative
}

DefaultWorldCameraModel::DefaultWorldCameraModel() = default;

DefaultWorldCameraModel::DefaultWorldCameraModel(glm::vec3 origin, glm::vec3 focus)
    : _currentOrigin(origin)
    , _currentFocus(focus)
    , _targetOrigin(origin)
    , _targetFocus(focus)
    , _originFocusDistanceAtInteractionStart(glm::distance(origin, focus))
    , _originAtClick(origin)
    , _focusAtClick(focus)
{
}

DefaultWorldCameraModel::~DefaultWorldCameraModel() = default;

void DefaultWorldCameraModel::TiltZoom(glm::vec3& eulerAngles, float scalingFactor, float zoomDelta)
{
	// Update the camera's yaw if there's significant horizontal movement.
	if (glm::abs(_rotateAroundDelta.y) > glm::epsilon<float>())
	{
		const auto windowSize = Locator::windowing::value().GetSize();
		eulerAngles.x += _rotateAroundDelta.y * glm::pi<float>() / windowSize.x;
	}

	// Update the camera's pitch if there's significant vertical movement.
	if (glm::abs(_rotateAroundDelta.x) > glm::epsilon<float>())
	{
		const auto pitchStep = _rotateAroundDelta.x * k_PitchPerInput;
		eulerAngles.y -= pitchStep;
		// Clamp the pitch angle to keep the camera within between -30 and 78.75 degrees.
		eulerAngles.y = glm::clamp(eulerAngles.y, -1.0f / 6.0f * glm::pi<float>(), 7.0f / 16.0f * glm::pi<float>());
		if (_mode == Mode::ArcBall)
		{
			const auto distanceFromBound = (2.0f * camera_limits::k_Clearance) - _distanceFromBoundY;
			if (distanceFromBound > glm::epsilon<float>())
			{
				const auto verticalStep = distanceFromBound * pitchStep * _focusDistance * 0.051f;
				_targetFocus += verticalStep;
				_focusAtClick += verticalStep;
			}
		}
	}

	{
		// Compute a keyboard input offset based on the current pitch and yaw.
		const auto clampedTan = glm::clamp(glm::tan(eulerAngles.y), 0.2f, 2.0f);
		const auto localMovement = _keyBoardMoveDelta * glm::vec2(1.0f / clampedTan, -1.0f) * scalingFactor * 0.001f;
		const auto planarMovement = glm::vec2(glm::cos(-eulerAngles.x), glm::sin(-eulerAngles.x));
		const auto offset = glm::vec3(localMovement.y * planarMovement.x - localMovement.x * planarMovement.y, 0.0,
		                              glm::dot(localMovement, planarMovement));
		_targetOrigin += offset;
		_targetFocus += offset;
		_focusAtClick += offset;
	}

	_averageIslandDistance += zoomDelta;
	_averageIslandDistance = glm::max(_averageIslandDistance, camera_limits::k_ClosestZoom);
}

float DefaultWorldCameraModel::GetVerticalLineInverseDistanceWeighingRayCast(const Camera& camera) const
{
	std::vector<float> inverseHitDistances;
	inverseHitDistances.reserve(0x10);

	for (int i = 0; i < 0x10; ++i)
	{
		const glm::vec2 coord = glm::vec2(0.5f, i / 16.0f);

		if (const auto hit = camera.RaycastScreenCoordToLand(coord, true, Camera::Interpolation::Drawn))
		{
			inverseHitDistances.push_back(1.0f / glm::length(hit->position - _targetOrigin));
		}
	}

	// Default distance of 50 if no hits happen, therefore divide by size + 1
	const auto average = std::accumulate(inverseHitDistances.cbegin(), inverseHitDistances.cend(), 1.0f / 50.0f) /
	                     (inverseHitDistances.size() + 1);
	return 1.0f / average;
}

void DefaultWorldCameraModel::ComputeDistanceFromBoundY()
{
	if (_targetOrigin.y < _targetFocus.y)
	{
		_distanceFromBoundY = 0.0f;
	}
	else
	{
		const auto groundAltitude = Locator::terrainSystem::value().GetHeightAt(glm::xz(_targetOrigin));
		const auto boundsMinY = groundAltitude + camera_limits::k_Clearance;
		const auto boundsMaxY = camera_zones::HeightLimit(CurrentZones(), groundAltitude);
		_distanceFromBoundY = glm::min(glm::abs(_targetOrigin.y - boundsMinY), glm::abs(_targetOrigin.y - boundsMaxY));
	}
}

camera_zones::Zones DefaultWorldCameraModel::CurrentZones()
{
	return Locator::cameraZoneSystem::has_value() ? Locator::cameraZoneSystem::value().GetZones() : camera_zones::Zones {};
}

DefaultWorldCameraModel::LimitsMoved DefaultWorldCameraModel::ApplyLimits(glm::vec3 originAtFrameStart, glm::vec3 listener,
                                                                          std::chrono::microseconds dt, float zoomDelta,
                                                                          glm::vec3 eulerAngles, const Camera& camera)
{
	if (!Locator::terrainSystem::has_value())
	{
		return {};
	}
	const auto& terrain = Locator::terrainSystem::value();
	const auto zones = CurrentZones();
	// The floor and the ceiling are those over where the camera was at the start of the frame
	const auto groundAtStart = terrain.GetHeightAt(glm::xz(originAtFrameStart));
	const auto nearby = camera_limits::NearbyExclusions(zones.exclusions, originAtFrameStart);
	LimitsMoved moved;
	const auto originBefore = _targetOrigin;
	const auto focusBefore = _targetFocus;

	// Out of the cylinders the zones keep it from
	if (zones.exclusionsOn)
	{
		if (const auto pushed = camera_limits::PushOutOfCylinders(nearby, _targetOrigin); pushed.has_value())
		{
			_targetOrigin = *pushed;
			moved.pushed = true;
		}
	}

	// Off the land and the domes round it
	const camera_limits::Ground ground {
	    .height = [&terrain](glm::vec2 point) { return terrain.GetHeightAt(point); },
	    .normal = [&terrain](glm::vec2 point) { return terrain.GetNormalAt(point); },
	};
	const auto push = camera_limits::PushOffTheGround(ground, nearby, zones.exclusionsOn, _targetOrigin, _targetFocus);
	_targetOrigin = push.origin;
	_targetFocus = push.focus;
	moved.pushed = moved.pushed || push.pushed;
	moved.focusMoved = moved.focusMoved || push.focusMoved;

	if (KeepInsideFence(zones, originAtFrameStart, listener))
	{
		moved.pushed = true;
		moved.focusMoved = true;
	}

	// Pushed on a flight's first frame, what it looks at keeps where it was from the camera, and is then brought to the
	// distance of the land ahead
	bool refocus = false;
	if (_flewThisFrame && moved.pushed)
	{
		_targetFocus = focusBefore + (_targetOrigin - originBefore);
		refocus = true;
		moved.focusMoved = true;
	}

	// Never below the clearance over the ground. Turning the camera about a point recomputes its height with a little
	// rounding, so a camera resting on the floor is let be within a ten-thousandth of it.
	constexpr float k_FloorRounding = 1e-4f;
	if (_targetOrigin.y < groundAtStart + camera_limits::k_Clearance - k_FloorRounding)
	{
		_targetOrigin.y = groundAtStart + camera_limits::k_Clearance;
		moved.pushed = true;
	}

	// Above its ceiling it slides down the line it looks along, what it looks at with it
	if (zones.exclusionsOn)
	{
		const auto ceiling = camera_zones::HeightLimit(zones, groundAtStart);
		if (_targetOrigin.y > ceiling)
		{
			if (const auto slide = camera_zones::SlideUnderLimit(_targetOrigin, _targetFocus, ceiling); slide.has_value())
			{
				_targetOrigin += *slide;
				_targetFocus += *slide;
			}
			moved.pushed = true;
			moved.focusMoved = true;
		}
	}

	// What it looks at is put the distance of the land ahead along its line of sight while it is tilted (but not turned
	// round the hand), always for the self-tilting camera, and after a flight's first frame was pushed
	const bool tilting = _mode != Mode::ArcBall && _rotateAroundDelta.x != 0.0f;
	if (tilting || (_features & camera_help::feature::k_AutoPitch) != 0 || refocus)
	{
		_targetFocus = _targetOrigin + glm::normalize(_targetFocus - _targetOrigin) * _focusDistance;
	}

	// Within the disc round the island; what it looks at stays where it is
	if (const auto inside = camera_limits::KeepInsideAllowedArea(_targetOrigin); inside.has_value())
	{
		_targetOrigin = *inside;
		moved.focusMoved = true;
	}

	// No further in a frame than the limit, except on a flight's first frame or zooming
	if (!_flewThisFrame && zoomDelta == 0.0f)
	{
		const auto seconds = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
		const auto limit = camera_limits::FrameMoveLimit(_focusDistance, seconds);
		if (const auto limited = camera_limits::LimitFrameMove(originAtFrameStart, _targetOrigin, limit); limited.has_value())
		{
			_targetOrigin = *limited;
			moved.pushed = true;
		}
	}

	// Moved by any of these, the camera turns, tilts and zooms from where it now is, unless the land is being dragged or
	// it turns round the hand
	if (moved.Any() && _mode != Mode::DraggingLandscape && _mode != Mode::ArcBall)
	{
		_originFocusDistanceAtInteractionStart =
		    glm::max(glm::distance(_targetOrigin, _targetFocus), camera_limits::k_ClosestZoom);
		UpdateFocusPointInteractionParameters(_targetOrigin, _targetFocus, eulerAngles, camera);
	}
	return moved;
}

void DefaultWorldCameraModel::UpdateFocusPointInteractionParameters(glm::vec3 origin, glm::vec3 focus, glm::vec3 eulerAngles,
                                                                    const Camera& camera)
{
	_focusAtClick = _targetFocus;
	_screenSpaceMouseRaycastHitAtClick =
	    camera.RaycastMouseToLand(true, Camera::Interpolation::Drawn).and_then(ecs::components::GetTransformPosition);
	if (_screenSpaceMouseRaycastHitAtClick.has_value())
	{
		_arcBallRadius = PointDistanceAlongLineSegment(origin, focus, *_screenSpaceMouseRaycastHitAtClick);
	}
	_originAtClick = _targetOrigin;
	_mouseAtClick = Locator::gameActionSystem::value().GetMousePosition();
	_originFocusDistanceAtInteractionStart = glm::distance(origin, focus);
	// Dragging the land, the camera, the cursor and the plane are those of the press
	if (_mode == Mode::DraggingLandscape && _dragging && _landGrip.has_value())
	{
		_originAtClick = _landGrip->origin;
		_focusAtClick = _landGrip->focus;
		_mouseAtClick = _landGrip->cursor;
		_originToHandPlaneNormal = _landGrip->plane.normal;
		_alignmentAtInteractionStart = _landGrip->plane.distance;
		_originFocusDistanceAtInteractionStart = glm::distance(_landGrip->origin, _landGrip->focus);
	}
	_averageIslandDistance = GetVerticalLineInverseDistanceWeighingRayCast(camera);
	{
		const auto diff = _targetOrigin - _targetFocus;
		// If the camera is directly above the focus point, set pitch to 90 degrees.
		if (glm::all(glm::lessThan(glm::abs(glm::xz(diff)), glm::vec2(0.1f, 0.1f))))
		{
			eulerAngles = glm::vec3(0.0f, glm::half_pi<float>(), 0.0f);
		}
		// Otherwise, calculate yaw and pitch based on the direction to the focus point.
		else
		{
			eulerAngles =
			    glm::vec3(glm::pi<float>() - glm::atan(diff.x, -diff.z), glm::atan(diff.y, glm::length(glm::xz(diff))), 0.0f);
		}
	}
	const auto extra =
	    (_focusDistance - _averageIslandDistance) * glm::clamp(eulerAngles.y / (6.0f * glm::pi<float>()), 0.0f, 1.0f);
	_averageIslandDistance += extra;
}

void DefaultWorldCameraModel::UpdateMode(const Camera& camera, glm::vec3 eulerAngles, float zoomDelta, glm::uvec2 mouseCurrent)
{
	switch (_mode)
	{
	case Mode::Cartesian:
		UpdateModeCartesian();
		break;
	case Mode::Polar:
		UpdateModePolar(eulerAngles, zoomDelta == 0.0f);
		break;
	case Mode::ArcBall:
		UpdateModeArcBall(eulerAngles, mouseCurrent, camera.GetHorizontalFieldOfView());
		break;
	case Mode::DraggingLandscape:
		UpdateModeDragging(camera, mouseCurrent);
		break;
	case Mode::FlyingToPoint:
		FlyTo(camera_fly::Reason::DoubleClick);
		break;
	}

	if (_mode != Mode::ArcBall && _rotateAroundDelta.x != 0.0f)
	{
		const auto diff = _targetFocus - _targetOrigin;
		_targetOrigin = _currentOrigin;
		_targetFocus = _targetOrigin + diff;
		_focusAtClick = _targetFocus;
	}
}

void DefaultWorldCameraModel::UpdateModeCartesian()
{
	// Drag focus on land
	const auto dist =
	    _screenSpaceCenterRaycastHit.has_value() ? glm::distance(*_screenSpaceCenterRaycastHit, _targetOrigin) : _focusDistance;
	_targetFocus = ProjectPointOnForwardVector(glm::max(dist - 1.0f, 0.1f));
	_focusAtClick = _targetFocus;
}

void DefaultWorldCameraModel::UpdateModePolar(glm::vec3 eulerAngles, bool recalculatePoint)
{
	// Pitch should already be clamped in TiltZoom

	// Put average distance point on half-line from camera
	const auto averageIslandPoint = _targetOrigin + _averageIslandDistance * glm::normalize(GetTargetForwardVector());

	// Rotate camera origin based on euler angles as polar coordinates and focus and distance at interaction
	_targetOrigin = _focusAtClick + _originFocusDistanceAtInteractionStart * glm::euclidean(glm::yx(eulerAngles));

	if (recalculatePoint)
	{
		// The new line of sight goes to the point that the camera turns about
		const auto diff =
		    averageIslandPoint - (_targetOrigin + _averageIslandDistance * glm::normalize(_focusAtClick - _targetOrigin));
		_targetOrigin += diff;
		_focusAtClick += diff;
	}
	_targetFocus = _focusAtClick;
}

void DefaultWorldCameraModel::UpdateModeArcBall(glm::vec3 eulerAngles, glm::u16vec2 mouseCurrent, float xFov)
{
	if (!_screenSpaceMouseRaycastHitAtClick.has_value())
	{
		return;
	}
	const auto point = _focusAtClick + _originFocusDistanceAtInteractionStart * glm::euclidean(glm::yx(eulerAngles));

	const auto basisZ = glm::normalize(point - _focusAtClick);
	const auto basisX = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), basisZ));
	const auto basisY = glm::normalize(glm::cross(basisZ, basisX));

	const auto windowSize = static_cast<glm::vec2>(Locator::windowing::value().GetSize());
	const auto aspect = windowSize.y / windowSize.x;
	const auto screenCentreOffset = static_cast<glm::vec2>(mouseCurrent) / windowSize - 0.5f;

	const auto halfTanX = glm::tan(xFov * 0.5f);
	const auto halfTanY = halfTanX * aspect;

	const auto cameraOffsetX = screenCentreOffset.x * halfTanX * basisX * _arcBallRadius * 2.0f;
	const auto cameraOffsetY = screenCentreOffset.y * halfTanY * basisY * _arcBallRadius * 2.0f;

	_targetFocus = _screenSpaceMouseRaycastHitAtClick.value() + cameraOffsetX + cameraOffsetY;
	_targetOrigin = _targetFocus + basisZ * _arcBallRadius;
	_focusAtClick = _targetFocus;
}

void DefaultWorldCameraModel::UpdateModeDragging(const Camera& camera, glm::u16vec2 mouseCurrent)
{
	// Panning is strafing, and the land must be under the cursor now and where the land was gripped
	if ((_features & camera_help::feature::k_Strafe) == 0 || !_screenSpaceMouseRaycastHit.has_value() ||
	    (_landGrip.has_value() && !_landGrip->land))
	{
		return;
	}
	// Land gripped too far ahead isn't dragged, until the buttons are let go: the camera flies there instead, if double
	// clicks are allowed
	if (_landGrip.has_value() && _landGrip->depth > camera_pan::k_MaxGripDepth)
	{
		_dragGivenUp = true;
		_flyToGrip = (_features & camera_help::feature::k_DoubleClick) != 0;
		return;
	}

	// The lines of sight through the cursor now and at the grip, both from the camera as it was at the grip
	const auto screenSize = glm::vec2(Locator::windowing::value().GetSize());
	const auto inverseViewProjection = glm::inverse(camera.GetProjectionMatrix(Camera::Projection::Normal) *
	                                                glm::lookAt(_originAtClick, _focusAtClick, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto rayThrough = [&inverseViewProjection, screenSize](glm::vec2 cursor) {
		const auto ndc = glm::vec2((cursor.x / screenSize.x - 0.5f) * 2.0f, ((1.0f - cursor.y / screenSize.y) - 0.5f) * 2.0f);
		const auto near = inverseViewProjection * glm::vec4(ndc, 0.0f, 1.0f);
		const auto far = inverseViewProjection * glm::vec4(ndc, 0.5f, 1.0f);
		return glm::normalize(glm::vec3(far) / far.w - glm::vec3(near) / near.w);
	};
	const auto place =
	    camera_pan::Pan({.normal = _originToHandPlaneNormal, .distance = _alignmentAtInteractionStart}, _originAtClick,
	                    _focusAtClick, _originFocusDistanceAtInteractionStart, rayThrough(glm::vec2(mouseCurrent)),
	                    rayThrough(glm::vec2(_mouseAtClick)), glm::ivec2(mouseCurrent), glm::ivec2(_mouseAtClick));
	if (!place.has_value())
	{
		return;
	}
	// Counted once for each time the controls are handled
	SendHelpEvents(camera_help::events::LandDragEvents(_helpControls));
	_helpControls = {};

	// The camera stops short of land in its way, or of the sea
	auto stopped = *place;
	const auto move = place->origin - _originAtClick;
	if (glm::length(move) > 0.0f && Locator::pickingSystem::has_value())
	{
		// The line from where the camera was through where it goes, carried on to the map's edge, meets the land, or else
		// the sea near the camera
		const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(_originAtClick, place->origin,
		                                                                camera.GetOrigin(Camera::Interpolation::Drawn));
		if (hit.has_value())
		{
			stopped = camera_pan::StopShortOfLand(*place, _originAtClick, *hit);
		}
	}
	_targetOrigin = stopped.origin;
	_targetFocus = stopped.focus;
}

bool DefaultWorldCameraModel::FlyTo(camera_fly::Reason reason)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto& terrain = Locator::terrainSystem::value();
	// The thing under the hand, on the land where it stands, else the land or sea under the cursor
	std::optional<glm::vec3> place = _screenSpaceMouseRaycastHit;
	if (Locator::pickingSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		const auto& registry = Locator::entitiesRegistry::value();
		if (pick.object.has_value() && registry.Valid(*pick.object))
		{
			if (const auto* transform = registry.TryGet<const ecs::components::Transform>(*pick.object); transform != nullptr)
			{
				place = transform->position;
				place->y = terrain.GetHeightAt(glm::xz(*place));
			}
		}
	}
	const auto destination = camera_fly::Destination(place, CurrentZones());
	if (!destination.has_value())
	{
		return false;
	}
	const camera_limits::Ground ground {
	    .height = [&terrain](glm::vec2 point) { return terrain.GetHeightAt(point); },
	    .normal = [&terrain](glm::vec2 point) { return terrain.GetNormalAt(point); },
	};
	const auto flight = camera_fly::Plan(reason, ground, *destination, _targetOrigin, _targetFocus, _lastScreenCentreHit);
	_targetOrigin = flight.origin;
	_targetFocus = flight.focus;
	_elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(flight.startSeconds));
	// A long flight goes by a point high over the half way point, from where the camera now is
	if (flight.halfWay)
	{
		_flightPath = FlightPath {
		    .origin = flight.origin,
		    .focus = flight.focus,
		    .midpoint = camera_fly::HalfWayPoint(flight.origin, _currentOrigin, k_FlightHeightFactor, ground.height),
		};
	}
	if (flight.woosh)
	{
		Woosh();
	}
	_flewThisFrame = true;
	return true;
}

void DefaultWorldCameraModel::Woosh()
{
	// One of four wooshes, picked by the clock, centred on the listener
	static constexpr auto k_WooshingNoiseIds = std::array<audio::SoundId, 4> {
	    audio::SoundId::G_Woosh_01,
	    audio::SoundId::G_Woosh_02,
	    audio::SoundId::G_Woosh_03,
	    audio::SoundId::G_Woosh_04,
	};
	const auto woosh = k_WooshingNoiseIds.at(static_cast<size_t>(machine_clock::Ticks() & 3));
	audio::PlayGameSoundEffect(static_cast<entt::id_type>(woosh), std::nullopt);
}

void DefaultWorldCameraModel::UpdateCameraInterpolationValues(const Camera& camera)
{
	// Get current curve interpolated values from camera
	_currentOrigin = camera.GetOrigin(Camera::Interpolation::Current);
	_currentFocus = camera.GetFocus(Camera::Interpolation::Current);
	_targetOrigin = camera.GetOrigin(Camera::Interpolation::Target);
	_targetFocus = camera.GetFocus(Camera::Interpolation::Target);
}

void DefaultWorldCameraModel::UpdateRaycastHitPoints(const Camera& camera)
{
	// The rays through the cursor and the middle of the screen start from the kept view of the last frame. This view can
	// be different from the destination while the camera moves, and on the frame after a command puts the camera.
	{
		const auto hit = camera.RaycastMouseToLand(true, Camera::Interpolation::Drawn);
		_screenSpaceMouseRaycastHit = hit.and_then(ecs::components::GetTransformPosition);
	}
	{
		const auto hit = camera.RaycastScreenCoordToLand({0.5f, 0.5f}, true, Camera::Interpolation::Drawn);
		_screenSpaceCenterRaycastHit = hit.and_then(ecs::components::GetTransformPosition);
		_lastScreenCentreHit = _screenSpaceCenterRaycastHit.value_or(_lastScreenCentreHit);
	}
}

float DefaultWorldCameraModel::GetZoomScale() const
{
	// Zooming speeds up with height, so a wheel notch covers about the same share of the view at any altitude
	const auto heightAboveFocus = _targetOrigin.y - _targetFocus.y;
	const auto maxScale = heightAboveFocus < 0.0f ? k_ZoomScaleMaxBelowFocus : k_ZoomScaleMax;
	return glm::clamp(k_StepScaleHeightFactor * glm::abs(heightAboveFocus), k_StepScaleMin, maxScale);
}

void DefaultWorldCameraModel::UpdateFocusDistance()
{
	_focusDistance =
	    _screenSpaceCenterRaycastHit
	        .and_then([this](auto hit) -> std::optional<float> { return glm::max(10.0f, glm::distance(hit, _targetOrigin)); })
	        .value_or(glm::max(10.0f, _averageIslandDistance));
}

bool DefaultWorldCameraModel::KeepInsideFence(const camera_zones::Zones& zones, glm::vec3 originAtFrameStart,
                                              glm::vec3 listener)
{
	if (!Locator::cameraZoneSystem::has_value())
	{
		return false;
	}
	// Outside the fence it is put back inside, what it looks at moving with it; turned by a drag round the edge of the
	// screen, it is put further in, and goes on turning
	const bool edgeTurning = _dragging && _drag.GetMode() == camera_drag::DragMode::EdgeRotate;
	const auto pushed = camera_zones::PushInsideFence(zones.fence, zones.fenceOn, originAtFrameStart, _targetOrigin,
	                                                  _targetFocus, camera_zones::SearchRadius(edgeTurning));
	if (!pushed.has_value())
	{
		return false;
	}
	_targetOrigin += *pushed;
	_targetFocus += *pushed;
	Locator::cameraZoneSystem::value().FenceHit(_targetOrigin, listener);
	_heldBack = _heldBack || !edgeTurning;
	return true;
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> DefaultWorldCameraModel::Update(std::chrono::microseconds dt,
                                                                                          const Camera& camera)
{
	_elapsedTime += dt;
	_flewThisFrame = false;

	UpdateCameraInterpolationValues(camera);
	const auto originAtFrameStart = _targetOrigin;
	UpdateRaycastHitPoints(camera);
	UpdateFocusDistance();

	// A drag just pressed grips the land under the cursor, or the focus with no land there
	if (_dragging && !_landGrip.has_value())
	{
		const auto gripped = _screenSpaceMouseRaycastHit.value_or(_targetFocus);
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt(glm::xz(_targetOrigin))
		                        : gripped.y;
		_landGrip = LandGrip {
		    .origin = _targetOrigin,
		    .focus = _targetFocus,
		    .cursor = glm::u16vec2(Locator::gameActionSystem::value().GetMousePosition()),
		    .plane = camera_pan::PlaneThrough(gripped, _targetOrigin, ground),
		    .land = _screenSpaceMouseRaycastHit.has_value(),
		    .depth = PointDistanceAlongLineSegment(_targetOrigin, _targetFocus, gripped),
		};
	}

	// Get angles (yaw, pitch, roll). Roll is always 0
	glm::vec3 eulerAngles = EulerFromPoints(_targetOrigin, _focusAtClick);

	if (_mode != _modePrev && _mode != Mode::FlyingToPoint && _modePrev != Mode::FlyingToPoint)
	{
		UpdateFocusPointInteractionParameters(camera.GetOrigin(Camera::Interpolation::Target),
		                                      camera.GetFocus(Camera::Interpolation::Target), eulerAngles, camera);
		// The camera turns and tilts about the point it now looks at, not the one it looked at before the change
		eulerAngles = EulerFromPoints(_targetOrigin, _focusAtClick);
	}

	ComputeDistanceFromBoundY();

	// Get step size
	const auto scalingFactor = k_StepScaleMin;
	const auto zoomDelta = _rotateAroundDelta.z * 0.0015f * GetZoomScale();

	if (_mode == Mode::Polar || _mode == Mode::ArcBall)
	{
		// Adjust camera's orientation based on user input. Call will reset deltas.
		TiltZoom(eulerAngles, scalingFactor, zoomDelta);
	}

	const auto mouseCurrent = Locator::gameActionSystem::value().GetMousePosition();
	_originFocusDistanceAtInteractionStart =
	    glm::max(_originFocusDistanceAtInteractionStart + zoomDelta, camera_limits::k_ClosestZoom);

	UpdateMode(camera, eulerAngles, zoomDelta, mouseCurrent);
	// Land gripped too far ahead flies the camera there, and the double click waiting is taken with it
	if (_flyToGrip)
	{
		_flyToGrip = false;
		std::ignore = Locator::gameActionSystem::value().TakeDoubleClick();
		FlyTo(camera_fly::Reason::GripTooFar);
	}
	// Turned round the hand with the camera below what it looks at, it goes back over where it started the frame too
	const bool turnedFromBelow = _mode == Mode::ArcBall && _targetOrigin.y < _targetFocus.y;

	// The self-tilting camera keeps to its height over the land, where it was at the start of the frame, looking the way
	// it now does, while it tilts itself or nothing is asked of it
	if ((_features & camera_help::feature::k_AutoPitch) != 0 && _keepsToItsPlace)
	{
		const auto height = Locator::cameraHelpSystem::has_value() ? Locator::cameraHelpSystem::value().Get().autoPitchHeight
		                                                           : camera_help::k_DefaultAutoPitchHeight;
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt(glm::xz(originAtFrameStart))
		                        : 0.0f;
		const auto origin = glm::vec3(originAtFrameStart.x, ground + height, originAtFrameStart.z);
		_targetFocus += origin - _targetOrigin;
		_targetOrigin = origin;
		// The point it turns and tilts about is what it now looks at, so that the next frame measures the way it faces
		// from there rather than towards a point left off its line of sight
		_focusAtClick = _targetFocus;
	}

	const auto moved = ApplyLimits(originAtFrameStart, camera.GetOrigin(), dt, zoomDelta, eulerAngles, camera);

	// The self-tilting camera eases over a second while it keeps to its place, or is tilted, unless a flight's first frame
	// was pushed
	const bool tilting = _mode != Mode::ArcBall && _rotateAroundDelta.x != 0.0f;
	_selfTiltEase = (_features & camera_help::feature::k_AutoPitch) != 0 && (_keepsToItsPlace || tilting || turnedFromBelow) &&
	                !(_flewThisFrame && moved.pushed);

	return ComputeUpdateReturnInfo(moved, camera.GetInterpolatorTime());
}

std::optional<CameraModel::CameraInterpolationUpdateInfo>
DefaultWorldCameraModel::ComputeUpdateReturnInfo(LimitsMoved moved, std::chrono::microseconds t)
{
	if (!_flightPath.has_value())
	{
		const auto sinceFlight = std::chrono::duration<float>(_elapsedTime).count();
		const auto seconds =
		    _selfTiltEase ? k_SelfTiltEaseSeconds : camera_limits::EaseSeconds(sinceFlight, moved.pushed, moved.focusMoved);
		return {{GetTargetOrigin(), GetTargetFocus(),
		         std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(seconds))}};
	}
	if (t > k_MinimalCameraAnimationDuration / 2)
	{
		std::optional<FlightPath> backup = std::nullopt;
		std::swap(backup, _flightPath);
		_elapsedTime = decltype(_elapsedTime)::zero();
		return {{backup->origin, backup->focus, k_MinimalCameraAnimationDuration}};
	}
	if (_flightPath->midpoint.has_value())
	{
		std::optional<glm::vec3> backup = std::nullopt;
		std::swap(backup, _flightPath->midpoint);
		return {{*backup, _flightPath->focus,
		         std::chrono::duration_cast<std::chrono::microseconds>(k_MinimalCameraAnimationDuration * 0.9f)}};
	}
	return std::nullopt;
}

void DefaultWorldCameraModel::HandleActions(std::chrono::microseconds dt)
{
	_rotateAroundDelta = glm::vec3();
	_keyBoardMoveDelta = glm::vec2();

	// Compute delta position (dp) based on the elapsed time and speed.
	const auto dp = k_InteractionSpeedMultiplier * std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
	const auto& actionSystem = Locator::gameActionSystem::value();
	// Moving over the land can be sped up or slowed down; turning, tilting and zooming keep the game's speed
	const auto moveSpeed =
	    Locator::camera::has_value() ? Locator::camera::value().GetKeyboardMoveSpeed() : k_KeyboardMoveSpeedDefault;
	const auto moveDp = ScaleKeyboardMove(dp, moveSpeed);

	// What the scripts let the player do, less double clicking while watching a fight
	const auto help =
	    Locator::cameraHelpSystem::has_value() ? Locator::cameraHelpSystem::value().Get() : camera_help::CameraHelp {};
	const bool onFight = Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsCameraOnFight();
	_features = onFight ? camera_help::DuringFight(help.features) : help.features;
	const bool aroundMouse = (_features & camera_help::feature::k_RotateAroundMouse) != 0;
	const bool rotateAroundMouse = aroundMouse && actionSystem.Get(input::BindableActionMap::ROTATE_AROUND_MOUSE_ON);

	// TODO(#709): Set tricons based on ZOOM or TILT if any move is detected

	if (actionSystem.GetAny(input::BindableActionMap::ROTATE_LEFT, input::BindableActionMap::ROTATE_RIGHT))
	{
		const float distance = (actionSystem.Get(input::BindableActionMap::ROTATE_LEFT) ? -1.0f : 1.0f) * dp;
		_rotateAroundDelta.y += distance;
	}

	if (actionSystem.GetAny(input::BindableActionMap::TILT_UP, input::BindableActionMap::TILT_DOWN))
	{
		const float distance = (actionSystem.Get(input::BindableActionMap::TILT_DOWN) ? -1.0f : 1.0f) * dp;
		_rotateAroundDelta.x += distance;
	}

	if (actionSystem.GetAny(input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS))
	{
		const float direction = actionSystem.Get(input::BindableActionMap::MOVE_FORWARDS) ? -1.0f : 1.0f;
		const float distance = direction * dp;
		// If ZOOM_ON is active, apply the movement as a zoom action.
		if (actionSystem.Get(input::BindableActionMap::ZOOM_ON))
		{
			_rotateAroundDelta.z += distance;
		}
		// If ROTATE_ON is active, apply the movement as a tilt action.
		// TODO(#710): fight will always be rotating
		else if (actionSystem.Get(input::BindableActionMap::ROTATE_ON))
		{
			_rotateAroundDelta.x += distance;
		}
		// Otherwise, apply the movement normally.
		else
		{
			_keyBoardMoveDelta.x += direction * moveDp;
		}
	}

	if (actionSystem.GetAny(input::BindableActionMap::MOVE_RIGHT, input::BindableActionMap::MOVE_LEFT))
	{
		const float direction = actionSystem.Get(input::BindableActionMap::MOVE_RIGHT) ? -1.0f : 1.0f;
		const float distance = direction * dp;
		// If ZOOM_ON is active, apply the movement as a zoom action.
		// If ROTATE_ON is active, apply the movement as a tilt action.
		// TODO(#710): fight will always be rotating
		if (actionSystem.GetAny(input::BindableActionMap::ZOOM_ON, input::BindableActionMap::ROTATE_ON))
		{
			_rotateAroundDelta.y += distance;
		}
		// Otherwise, apply the movement normally.
		else
		{
			_keyBoardMoveDelta.y -= direction * moveDp;
		}
	}

	// A wheel step is a fixed distance, whatever the frame time
	if (const auto wheelNotches = actionSystem.GetMouseWheelDelta(); wheelNotches != 0.0f)
	{
		_rotateAroundDelta.z -= wheelNotches * k_WheelZoomPerNotch;
	}
	else if (actionSystem.GetAny(input::BindableActionMap::ZOOM_IN, input::BindableActionMap::ZOOM_OUT))
	{
		_rotateAroundDelta.z +=
		    actionSystem.Get(input::BindableActionMap::ZOOM_IN) ? -k_WheelZoomPerNotch : k_WheelZoomPerNotch;
	}

	const bool twoButtons = aroundMouse && actionSystem.Get(input::UnbindableActionMap::TWO_BUTTON_CLICK);
	if (twoButtons)
	{
		_rotateAroundDelta.z += actionSystem.GetMouseDelta().y * k_TwoButtonZoomFactor;
	}
	// Moving across turns the camera too, once the mouse has moved a fortieth of the screen's width across in a frame,
	// or since both were pressed
	{
		const auto width = Locator::windowing::has_value() ? Locator::windowing::value().GetSize().x : 1;
		const auto across = _twoButtonTurn.Update(twoButtons, actionSystem.GetMouseDelta().x,
		                                          static_cast<int>(actionSystem.GetMousePosition().x), width);
		_rotateAroundDelta.y += static_cast<float>(across) * k_TwoButtonTurnFactor;
	}

	if (rotateAroundMouse)
	{
		const auto mouseDelta = static_cast<glm::vec2>(actionSystem.GetMouseDelta());
		_rotateAroundDelta += glm::vec3(glm::yx(mouseDelta * k_RotateOnSpeedMultiplier), 0.0f);
	}

	// Ctrl and Shift together ease the clear view in over half a second, and out again
	_clearView.SetDestination(
	    actionSystem.Get(input::BindableActionMap::ZOOM_ON) && actionSystem.Get(input::BindableActionMap::ROTATE_ON) ? 1.0f
	                                                                                                                 : 0.0f,
	    k_ClearViewSeconds);
	_clearView.Update(std::chrono::duration<float>(dt).count());

	const auto handPositions = actionSystem.GetHandPositions();
	_handPosition = handPositions[0].or_else([handPositions] { return handPositions[1]; });

	_controlsTime += dt;
	// A drag starts with the hand on the land, and goes on for as long as the button is held, even once the cursor has
	// gone past the land (over the sky, or the land far off): the land isn't dragged meanwhile, but the drag doesn't end
	// and hand the camera back to the controls that turn or tilt it
	const bool gripping =
	    actionSystem.Get(input::BindableActionMap::MOVE) && (_handPosition.has_value() || (_dragging && !rotateAroundMouse));
	if (!gripping || rotateAroundMouse)
	{
		std::ignore = HandleDrag(false);
	}

	// The help's profile weighs the controls as they were asked for, before the features take any away
	_helpControls = {
	    .features = _features,
	    .turn = _rotateAroundDelta.y,
	    .tilt = _rotateAroundDelta.x,
	    .zoom = _rotateAroundDelta.z,
	    .move = _keyBoardMoveDelta,
	    .landGripped = gripping,
	    .rotateAroundMouse = rotateAroundMouse,
	    .bothButtons = twoButtons,
	    .mouseDelta = glm::ivec2(actionSystem.GetMouseDelta()),
	};
	_helpEvents = {};
	// Turning, tilting, zooming or turning round the hand, or the movement keys unless the land is gripped with them
	const bool moving = camera_help::events::AnyInput(_helpControls);
	// Any of the camera's controls stops a flight by a high point going on
	if (moving || gripping || rotateAroundMouse || twoButtons)
	{
		_flightPath.reset();
	}
	// The camera takes a waiting double click once double clicks are allowed and it isn't being moved; meanwhile the
	// click waits. Taken, it is counted here and flies the camera below.
	const bool doubleClicked = (_features & camera_help::feature::k_DoubleClick) != 0 && !moving &&
	                           Locator::gameActionSystem::value().TakeDoubleClick();
	// A drag of the land given up too far ahead takes every control away until the buttons are let go
	_turnedThisFrame.reset();
	if (!_dragGivenUp)
	{
		const auto inputEvents = camera_help::events::InputEvents(_helpControls);
		_helpEvents.Add(inputEvents);
		// The keys, the middle button and both buttons turned the camera as far as they asked
		if (inputEvents.Contains(help::profile::k_CameraTurn) && Locator::windowing::has_value())
		{
			_turnedThisFrame =
			    _helpControls.turn * glm::pi<float>() / static_cast<float>(Locator::windowing::value().GetSize().x);
		}
		if (doubleClicked && Locator::pickingSystem::has_value())
		{
			const auto& pick = Locator::pickingSystem::value().GetPick();
			_helpEvents.Add(camera_help::events::DoubleClickEvents(_features, pick.object.has_value(), pick.land.has_value()));
		}
	}

	// Without zooming the zoom does nothing, without turning nothing turns, without tilting nothing tilts unless the
	// camera tilts itself, and without strafing the movement keys do nothing
	if ((_features & camera_help::feature::k_Zoom) == 0)
	{
		_rotateAroundDelta.z = 0.0f;
	}
	if ((_features & camera_help::feature::k_Rotate) == 0)
	{
		_rotateAroundDelta.y = 0.0f;
	}
	if ((_features & camera_help::feature::k_Strafe) == 0)
	{
		_keyBoardMoveDelta = glm::vec2();
	}
	// The self-tilting camera tilts towards its pitch while the land isn't dragged
	bool autoTilting = false;
	if ((_features & camera_help::feature::k_AutoPitch) != 0 && !_dragging)
	{
		const auto pitch = EulerFromPoints(_targetOrigin, _focusAtClick).y;
		if (const auto input = camera_help::AutoPitchInput(help.autoPitch, pitch, std::chrono::duration<float>(dt).count()))
		{
			_rotateAroundDelta.x = *input;
			autoTilting = true;
		}
	}
	if ((_features & camera_help::feature::k_Pitch) == 0 && !autoTilting)
	{
		_rotateAroundDelta.x = 0.0f;
	}
	// The hints follow the cursor while no button holds the camera and the player isn't turning, moving or zooming it;
	// the camera tilting itself doesn't stop them
	const bool playerMovesCamera = _rotateAroundDelta != glm::vec3() || _keyBoardMoveDelta != glm::vec2();
	if (camera_drag::HintsFollowCursor(_dragging || rotateAroundMouse, playerMovesCamera, autoTilting) &&
	    Locator::windowing::has_value())
	{
		const auto& windowing = Locator::windowing::value();
		const auto screenSize = windowing.GetSize();
		const auto cursor = camera_drag::AreaCursor(glm::ivec2(actionSystem.GetMousePosition()), EdgeArea(screenSize));
		_tricons = camera_drag::IdleTricons(cursor, _screenSpaceMouseRaycastHit.has_value(), windowing.IsWindowed());
		// Watching a fight, the camera offers no tilting
		if (onFight)
		{
			_tricons &= ~camera_drag::tricon::k_Pitch;
		}
	}

	_modePrev = _mode;
	// The double click taken above flies the camera to what was clicked
	if (doubleClicked)
	{
		_mode = Mode::FlyingToPoint;
	}
	else if (rotateAroundMouse)
	{
		_mode = Mode::ArcBall;
	}
	else if (gripping)
	{
		_mode = HandleDrag(true);
	}
	else if (_keyBoardMoveDelta != glm::vec2() || _rotateAroundDelta != glm::vec3())
	{
		_mode = Mode::Polar;
	}
	else
	{
		_mode = Mode::Cartesian;
	}

	// Put back inside the fence, nothing the player does moves the camera, or counts towards the help, until every
	// control is let go
	if (_heldBack)
	{
		if (!AnyControlHeld())
		{
			_heldBack = false;
		}
		else
		{
			_rotateAroundDelta = glm::vec3();
			_keyBoardMoveDelta = glm::vec2();
			std::ignore = HandleDrag(false);
			_mode = Mode::Cartesian;
			_helpEvents = {};
			_turnedThisFrame.reset();
		}
	}
	// Whether the self-tilting camera keeps to its place this frame: held back, the controls count for nothing and the
	// camera doesn't tilt itself
	{
		const auto& asked = _helpControls;
		const bool anythingAsked = asked.turn != 0.0f || asked.tilt != 0.0f || asked.zoom != 0.0f ||
		                           asked.move != glm::vec2() || asked.rotateAroundMouse;
		_keepsToItsPlace =
		    camera_help::KeepsToItsPlace(autoTilting && !_heldBack, _dragging && !_heldBack, anythingAsked && !_heldBack);
	}
	SendHelpEvents(_helpEvents);
	// The measure of how fast the player turns the camera, which the advisors' cheering of the turning lesson goes by,
	// takes in this frame's turn, or slows towards still
	if (const auto seconds = std::chrono::duration<float>(dt).count(); seconds > 0.0f && Locator::cameraHelpSystem::has_value())
	{
		auto& turnSpeed = Locator::cameraHelpSystem::value().Get().turnSpeed;
		turnSpeed = help::turn_cheer::SmoothSpeed(turnSpeed, _turnedThisFrame.value_or(0.0f), seconds);
	}
	UpdateIconFrame(onFight);
}

void DefaultWorldCameraModel::UpdateIconFrame(bool onFight)
{
	using camera_drag::DragMode;
	const auto& actionSystem = Locator::gameActionSystem::value();
	const auto& controls = _helpControls;
	const bool asked = controls.turn != 0.0f || controls.tilt != 0.0f || controls.zoom != 0.0f;
	const auto mode = _dragging ? _drag.GetMode() : std::nullopt;
	glm::vec2 cursor {0.0f};
	if (Locator::windowing::has_value())
	{
		const auto screenSize = Locator::windowing::value().GetSize();
		cursor = camera_drag::AreaCursor(glm::ivec2(actionSystem.GetMousePosition()), EdgeArea(screenSize));
	}
	_iconFrame = {
	    // A drag given up too far ahead shows only that
	    .mouseHints = _dragGivenUp ? hand_tricons::k_TooFar : (_dragging ? _drag.GetTricons() : _tricons),
	    .gripping = controls.landGripped,
	    // Gripping the land is all the camera has unless it is also turned, tilted or zoomed, or turned round the mouse
	    .grippingOnly = controls.landGripped && !asked && !controls.rotateAroundMouse,
	    .zoomKeyHeld = actionSystem.Get(input::BindableActionMap::ZOOM_ON),
	    .rotateKeyHeld = actionSystem.Get(input::BindableActionMap::ROTATE_ON),
	    .turn = controls.turn,
	    .tilt = controls.tilt,
	    .zoom = controls.zoom,
	    .edgeTurning = mode == DragMode::EdgeRotate,
	    .pitchDragging = mode == DragMode::Pitch || mode == DragMode::PitchFromTop,
	    .rotatingAroundMouse = controls.rotateAroundMouse,
	    .fight = onFight,
	    .clearView = _clearView.GetValue(),
	    .features = _features,
	    .blocked = _heldBack || _dragGivenUp,
	    .cursor = cursor,
	};
}

bool DefaultWorldCameraModel::AnyControlHeld() const
{
	using input::BindableActionMap;
	const auto& actionSystem = Locator::gameActionSystem::value();
	return actionSystem.GetAny(BindableActionMap::MOVE, BindableActionMap::ROTATE_AROUND_MOUSE_ON,
	                           BindableActionMap::ROTATE_LEFT, BindableActionMap::ROTATE_RIGHT, BindableActionMap::TILT_UP,
	                           BindableActionMap::TILT_DOWN, BindableActionMap::MOVE_FORWARDS,
	                           BindableActionMap::MOVE_BACKWARDS, BindableActionMap::MOVE_LEFT, BindableActionMap::MOVE_RIGHT,
	                           BindableActionMap::ZOOM_IN, BindableActionMap::ZOOM_OUT) ||
	       actionSystem.Get(input::UnbindableActionMap::TWO_BUTTON_CLICK) || actionSystem.GetMouseWheelDelta() != 0.0f;
}

void DefaultWorldCameraModel::SendHelpEvents(camera_help::events::EventSet events)
{
	if (events.Empty() || !Locator::helpProfileSystem::has_value())
	{
		return;
	}
	auto& profile = Locator::helpProfileSystem::value();
	events.ForEach([&profile](uint32_t event) { profile.Trigger(event); });
}

DefaultWorldCameraModel::Mode DefaultWorldCameraModel::HandleDrag(bool held)
{
	using camera_drag::DragMode;
	if (!held)
	{
		_dragging = false;
		_dragGivenUp = false;
		return _mode;
	}
	if (_dragGivenUp)
	{
		return Mode::Cartesian;
	}
	if (!Locator::windowing::has_value())
	{
		return Mode::DraggingLandscape;
	}
	auto& actionSystem = Locator::gameActionSystem::value();
	const auto screenSize = Locator::windowing::value().GetSize();
	const auto cursor = glm::ivec2(actionSystem.GetMousePosition());
	const auto milliseconds =
	    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(_controlsTime).count());
	if (!_dragging)
	{
		// The drag takes the hints of the cursor before it was pressed
		_dragging = true;
		_drag.Start(_tricons, camera_drag::AreaCursor(cursor, EdgeArea(screenSize)), milliseconds);
		_ring.Start(cursor, EdgeArea(screenSize));
		// The land is gripped as the camera next looks at it
		_landGrip.reset();
	}
	else
	{
		_drag.Move(actionSystem.GetMouseDelta(), EdgeArea(screenSize), milliseconds, _screenSpaceMouseRaycastHit.has_value(),
		           _features);
	}

	const auto mode = _drag.GetMode();
	if (!mode.has_value())
	{
		// Until it is decided, the camera stays put
		return Mode::Cartesian;
	}
	switch (*mode)
	{
	case DragMode::Pan:
		return Mode::DraggingLandscape;
	case DragMode::EdgeRotate:
	{
		// The cursor is held on the ring, and the camera turns about its focus by the angle swept round the middle. The
		// ring is measured from where the pointer is now rather than where it was as the frame began: the mouse has
		// moved on since, and putting the cursor back on the ring from the older place would undo that movement every
		// frame and hold the camera still. Its place on the ring is kept to a fraction of a pixel, so slow movements of a
		// pixel or two a frame still turn the camera
		const auto pointer = actionSystem.GetPointerPosition();
		const auto step = _ring.Step(pointer, EdgeArea(screenSize));
		// The turn feels as it did on the original's default screen, whatever the resolution: a pixel of the pointer turns
		// as a pixel did there, and the player's pointer, moving faster on a bigger screen so the same mouse movement
		// sweeps the same share of it, would turn the camera that much faster too
		const float turn = step.turn / actionSystem.GetPointerScale();
		_helpEvents.Add(camera_help::events::EdgeTurnEvents(_helpControls, turn));
		if ((_features & camera_help::feature::k_Rotate) != 0)
		{
			_turnedThisFrame = turn;
			_rotateAroundDelta.y += turn * static_cast<float>(screenSize.x) / glm::pi<float>();
		}
		actionSystem.WarpCursor(step.cursor);
		return Mode::Polar;
	}
	case DragMode::Pitch:
	case DragMode::PitchFromTop:
	{
		// Through the lens, the view across a 4:3 screen, so a drag tilts as much for the screen's height on any screen
		const auto fov = Locator::camera::has_value() ? Locator::camera::value().GetLens() : 0.0f;
		_helpEvents.Add(camera_help::events::TiltDragEvents(_helpControls));
		if ((_features & camera_help::feature::k_Pitch) != 0)
		{
			_rotateAroundDelta.x += camera_drag::PitchStep(actionSystem.GetMouseDelta().y, screenSize.y, fov) / k_PitchPerInput;
		}
		return Mode::Polar;
	}
	}
	return Mode::DraggingLandscape;
}

camera_drag::EdgeArea DefaultWorldCameraModel::EdgeArea(glm::ivec2 screenSize)
{
	const bool bars =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	return camera_drag::EdgeAreaOf(screenSize, bars);
}

CameraModel::HandCues DefaultWorldCameraModel::GetHandCues() const
{
	if (_dragging)
	{
		return {.tricons = _drag.GetTricons(),
		        .dragging = true,
		        .dragMode = _drag.GetMode(),
		        .clearViewGrip = _clearView.GetValue() > k_ClearViewGrips,
		        .icons = _iconFrame,
		        .clearView = _clearView.GetValue()};
	}
	return {.tricons = _tricons, .icons = _iconFrame, .clearView = _clearView.GetValue()};
}

void DefaultWorldCameraModel::SetFlight(glm::vec3 origin, glm::vec3 focus)
{
	// A flight ending outside the fence ends where the line from there to what it looks at meets the fence
	if (Locator::cameraZoneSystem::has_value())
	{
		const auto& zones = Locator::cameraZoneSystem::value().GetZones();
		origin = camera_zones::FlightOriginInsideFence(zones.fence, zones.fenceOn, origin, focus);
	}
	_flightPath = CharterFlight(origin, focus, _currentOrigin, k_FlightHeightFactor);
	Woosh();
}

void DefaultWorldCameraModel::FlyBack(glm::vec3 origin, glm::vec3 focus, bool woosh)
{
	if (Locator::cameraZoneSystem::has_value())
	{
		const auto& zones = Locator::cameraZoneSystem::value().GetZones();
		origin = camera_zones::FlightOriginInsideFence(zones.fence, zones.fenceOn, origin, focus);
	}
	_flightPath = CharterFlight(origin, focus, _currentOrigin, k_FlightHeightFactor);
	if (woosh)
	{
		Woosh();
	}
}

void DefaultWorldCameraModel::PlaceAt(glm::vec3 origin, glm::vec3 focus)
{
	_currentOrigin = origin;
	_currentFocus = focus;
	_targetOrigin = origin;
	_targetFocus = focus;
	// The command puts only the camera. The controls that the player holds keep their state and continue:
	// - A turn or a tilt keeps the point that it turns about and its distance. The camera thus looks from the place
	//   towards that point.
	// - A turn about the hand keeps the land point under the hand and its distance.
	// - A drag of the land continues with the grip, the cursor and the view of its start.
	// - A hold of the controls after a push from the fence continues.
	// A flight continues from here. Its clock stops, thus after its first part the camera stays here. A control of the
	// camera stops the flight. A movement that runs for half the flight time starts the last part of the flight.
	// A double tap that waits for this frame flies the camera from here.
}

glm::vec3 DefaultWorldCameraModel::GetTargetOrigin() const
{
	return _targetOrigin;
}

glm::vec3 DefaultWorldCameraModel::GetTargetFocus() const
{
	return _targetFocus;
}

std::chrono::seconds DefaultWorldCameraModel::GetIdleTime() const
{
	SPDLOG_LOGGER_WARN(spdlog::get("game"), "TODO: Idle Time not implemented");
	return {};
}

glm::vec3 DefaultWorldCameraModel::GetTargetForwardVector() const
{
	return GetTargetFocus() - GetTargetOrigin();
}

glm::vec3 DefaultWorldCameraModel::GetTargetForwardUnitVector() const
{
	return glm::normalize(GetTargetForwardVector());
}

glm::vec3 DefaultWorldCameraModel::ProjectPointOnForwardVector(float distanceFromOrigin) const
{
	return _targetOrigin + GetTargetForwardUnitVector() * distanceFromOrigin;
}
