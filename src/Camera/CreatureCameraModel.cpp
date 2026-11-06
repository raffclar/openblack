/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCameraModel.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "CameraZones.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

namespace openblack
{

CreatureCameraModel::CreatureCameraModel(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 creature, float height,
                                         creature_follow::StartDistance start)
    : _view(creature_follow::Start(cameraOrigin, cameraFocus, creature, height, start))
    , _focus(creature_follow::Focus(creature, height))
    , _position(creature)
{
}

CreatureCameraModel::CreatureCameraModel(const creature_follow::Loaded& loaded, glm::vec3 creature, float height)
    : _view(loaded.view)
    , _focus(creature_follow::Focus(creature, height))
    , _position(creature)
    , _relativeHeading(loaded.relativeHeading)
    , _offsets(loaded.offsets)
    , _timeScale(loaded.timeScale)
    , _autoReposition(loaded.autoReposition)
{
}

void CreatureCameraModel::SetTarget(glm::vec3 position, float height, std::optional<uint16_t> gameAngle)
{
	_focus = creature_follow::Focus(position, height);
	_position = position;
	_gameAngle = gameAngle;
	_thingHasModel = true;
}

void CreatureCameraModel::SetTarget(const creature_follow::Target& target)
{
	SetTarget(target.position, target.height, target.gameAngle);
	_thingHasModel = target.hasModel;
}

bool CreatureCameraModel::KeepsOffsets() const
{
	return _offsets.has_value() && _thingHasModel;
}

CreatureCameraModel::Points CreatureCameraModel::Aim() const
{
	if (KeepsOffsets())
	{
		return {.origin = _position + _offsets->eye, .focus = _position + _offsets->focus};
	}
	auto view = creature_follow::Clamped(_view);
	// On a thing with no model, the offsets give the distance after the bounds, and the heading and pitch place the eye
	if (_offsets.has_value())
	{
		view.distance = creature_follow::OffsetDistance(*_offsets);
	}
	// Only a thing that walks has a game angle to turn the heading with
	if (_relativeHeading && _gameAngle.has_value())
	{
		view.yaw = creature_follow::RelativeHeading(view.yaw, *_gameAngle);
	}
	return {.origin = creature_follow::Origin(_focus, view), .focus = _focus};
}

void CreatureCameraModel::ClearView(const camera_limits::Ground& ground)
{
	_view = creature_follow::Clamped(creature_follow::ClearView(_view, _focus, ground));
}

creature_follow::KeyOutcome CreatureCameraModel::Turn(const creature_follow::Keys& keys, float screenWidth)
{
	const auto outcome = creature_follow::Apply(_view, _offsets, keys, screenWidth);
	if (outcome == creature_follow::KeyOutcome::Leave)
	{
		_leaveRequested = true;
	}
	return outcome;
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> CreatureCameraModel::Update(std::chrono::microseconds dt,
                                                                                      [[maybe_unused]] const Camera& camera)
{
	const auto seconds = std::chrono::duration<float>(dt).count();
	// It sets off afresh each frame for where it wants to be, arriving after the ease's time, so it eases after the
	// creature as it moves
	const auto ease = creature_follow::EaseSeconds(_seconds) * _timeScale;
	_seconds += seconds;
	_view = creature_follow::Clamped(_view);
	// With offsets, the distance of the view is the distance between the two points of the offsets
	if (_offsets.has_value())
	{
		_view.distance = creature_follow::OffsetDistance(*_offsets);
	}
	const auto aim = Aim();
	auto origin = aim.origin;
	// Where it would go outside the fence, it stops where its line of sight crosses the fence, and the player's own
	// camera takes over from there
	if (Locator::cameraZoneSystem::has_value())
	{
		const auto& zones = Locator::cameraZoneSystem::value().GetZones();
		const auto inside = camera_zones::FlightOriginInsideFence(zones.fence, zones.fenceOn, origin, aim.focus);
		if (inside != origin)
		{
			origin = inside;
			_leaveRequested = true;
			_stoppedAtFence = true;
		}
	}
	return CameraInterpolationUpdateInfo {
	    .origin = origin,
	    .focus = aim.focus,
	    .duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(ease)),
	};
}

void CreatureCameraModel::HandleKeys(std::chrono::microseconds dt)
{
	using input::BindableActionMap;
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto& actions = Locator::gameActionSystem::value();
	const auto seconds = std::chrono::duration<float>(dt).count();
	const int across =
	    (actions.Get(BindableActionMap::MOVE_RIGHT) ? 1 : 0) - (actions.Get(BindableActionMap::MOVE_LEFT) ? 1 : 0);
	const int along =
	    (actions.Get(BindableActionMap::MOVE_BACKWARDS) ? 1 : 0) - (actions.Get(BindableActionMap::MOVE_FORWARDS) ? 1 : 0);
	auto keys = creature_follow::KeysFor(across, along, seconds);
	keys.shift = actions.Get(BindableActionMap::ROTATE_ON);
	keys.ctrl = actions.Get(BindableActionMap::ZOOM_ON);
	if (actions.Get(BindableActionMap::ZOOM_OUT))
	{
		keys.wheel -= creature_follow::k_WheelNotch;
	}
	if (actions.Get(BindableActionMap::ZOOM_IN))
	{
		keys.wheel += creature_follow::k_WheelNotch;
	}
	const auto width = Locator::windowing::has_value() ? static_cast<float>(Locator::windowing::value().GetSize().x) : 1.0f;
	Turn(keys, width);

	// Ctrl and Shift held together, with the cursor keys still, swing the camera clear of the land in the way. A camera
	// that turns with its thing, or that keeps offsets on a thing with a model, does not swing.
	if (_autoReposition && !_relativeHeading && !KeepsOffsets() && keys.shift && keys.ctrl && !keys.Moving() &&
	    Locator::terrainSystem::has_value())
	{
		const auto& land = Locator::terrainSystem::value();
		ClearView({
		    .height = [&land](glm::vec2 point) { return land.GetHeightAt(point); },
		    .normal = [&land](glm::vec2 point) { return land.GetNormalAt(point); },
		});
	}
}

void CreatureCameraModel::SetFlight(glm::vec3 origin, glm::vec3 focus, [[maybe_unused]] float rise)
{
	// Sent somewhere, it keeps looking at the creature from the way the flight would have it look
	const auto turned = creature_follow::HeadingPitchOf(origin, focus);
	_view.yaw = turned.heading;
	_view.pitch = turned.pitch;
	_view.distance = glm::distance(origin, focus);
	_view = creature_follow::Clamped(_view);
}

} // namespace openblack
