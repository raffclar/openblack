/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCameraModel.h"

#include <cmath>

#include <algorithm>
#include <numbers>
#include <utility>

#include <glm/geometric.hpp>

#include "3D/CameraTrack.h"

using namespace openblack;
using namespace openblack::script_camera;

namespace
{
using Zoomers = std::array<camera::Zoomer, 3>;

void Place(Zoomers& zoomers, const glm::vec3& point)
{
	for (glm::length_t i = 0; i < 3; ++i)
	{
		zoomers.at(static_cast<size_t>(i)).SetPosition(point[i]);
	}
}

void Send(Zoomers& zoomers, const glm::vec3& point, float seconds)
{
	for (glm::length_t i = 0; i < 3; ++i)
	{
		zoomers.at(static_cast<size_t>(i)).SetDestination(point[i], 0.0f, seconds);
	}
}

glm::vec3 ValueOf(const Zoomers& zoomers)
{
	return {zoomers[0].Value(), zoomers[1].Value(), zoomers[2].Value()};
}

glm::vec3 DestinationOf(const Zoomers& zoomers)
{
	return {zoomers[0].Destination(), zoomers[1].Destination(), zoomers[2].Destination()};
}

float DistanceSquared(const glm::vec3& a, const glm::vec3& b)
{
	const auto d = a - b;
	return glm::dot(d, d);
}
} // namespace

std::optional<glm::vec3> script_camera::PullIntoWorld(const glm::vec3& destination)
{
	const auto fromCentre = destination - glm::vec3(k_WorldCentre, 0.0f, k_WorldCentre);
	const float distanceSquared = glm::dot(fromCentre, fromCentre);
	if (!(k_WorldRadiusSquared < distanceSquared))
	{
		return std::nullopt;
	}
	const float scale = 1.0f / (std::sqrt(distanceSquared) * k_WorldScale);
	return glm::vec3(fromCentre.x * scale + k_WorldCentre, fromCentre.y * scale, fromCentre.z * scale + k_WorldCentre);
}

View script_camera::Drawn(const View& camera, const std::function<float(float x, float z)>& groundAt)
{
	auto view = camera;
	// A camera where it looks would look nowhere
	if (DistanceSquared(view.origin, view.focus) < k_ArrivedDistanceSquared)
	{
		view.origin.x -= 1.0f;
		view.origin.y += 1.0f;
	}
	const float below = groundAt(view.origin.x, view.origin.z) + k_GroundClearance - view.origin.y;
	if (below > 0.0f)
	{
		view.origin.y += below;
		view.focus.y += below;
	}
	return view;
}

HeadingAndPitch script_camera::HeadingAndPitchFromPoints(const glm::vec3& origin, const glm::vec3& focus)
{
	constexpr double k_Overhead = 0.01;
	const auto d = origin - focus;
	if (std::abs(d.x) < k_Overhead && std::abs(d.z) < k_Overhead)
	{
		return {.heading = 0.0f, .pitch = k_StraightDownPitch};
	}
	constexpr float k_NoDirection = 1e-6f;
	const float headingFromSouth = d.z * d.z + d.x * d.x > k_NoDirection ? static_cast<float>(std::atan2(d.x, -d.z)) : 0.0f;
	const float across = std::sqrt(d.x * d.x + d.z * d.z);
	return {.heading = std::numbers::pi_v<float> - headingFromSouth, .pitch = static_cast<float>(std::atan2(d.y, across))};
}

glm::vec3 script_camera::PointFromDistanceHeadingAndPitch(const glm::vec3& point, float distance, float heading, float pitch)
{
	const auto cosPitch = static_cast<float>(std::cos(static_cast<double>(pitch)));
	const auto sinPitch = std::sin(static_cast<double>(pitch));
	const auto sinHeading = std::sin(static_cast<double>(heading));
	const auto cosHeading = std::cos(static_cast<double>(heading));
	return {static_cast<float>(sinHeading * cosPitch * distance + point.x), static_cast<float>(sinPitch * distance + point.y),
	        static_cast<float>(cosHeading * cosPitch * distance + point.z)};
}

float script_camera::FollowSeconds(float secondsSinceBegun, float zoomTimeScale)
{
	const float seconds =
	    secondsSinceBegun > k_FollowBlendSeconds
	        ? k_FollowEndSeconds
	        : (k_FollowEndSeconds - k_FollowStartSeconds) * (secondsSinceBegun / k_FollowBlendSeconds) + k_FollowStartSeconds;
	return seconds * zoomTimeScale;
}

glm::vec3 script_camera::FollowOrigin(const FollowedThing& thing, FollowSettings& settings)
{
	settings.pitch = settings.pitch > k_FollowLowestPitch ? settings.pitch : k_FollowLowestPitch;
	float heading = settings.heading;
	if (settings.relativeHeading && thing.gameAngle.has_value())
	{
		// The game angle as a turn from the thing's facing, a quarter turn round from the camera's headings
		constexpr float k_HalfGameAngleTo3D = std::numbers::pi_v<float> / 2048.0f;
		heading -= static_cast<float>(*thing.gameAngle * 2) * k_HalfGameAngleTo3D - std::numbers::pi_v<float> / 2.0f;
	}
	return PointFromDistanceHeadingAndPitch(thing.point, settings.viewingDistance, heading, settings.pitch);
}

float script_camera::CreatureFacing(float bodyTurn)
{
	return bodyTurn + k_FullTurn - k_CreatureFacingOffset;
}

bool script_camera::IsWrongFacing(float facing)
{
	return !(facing < k_HighestFacing);
}

View script_camera::FaceView(const FacedThing& thing, float distance)
{
	const glm::vec3 focus {thing.position.x, thing.position.y + (thing.height * 0.5f), thing.position.z};
	float heading = thing.facing;
	while (heading > k_FullTurn)
	{
		heading -= k_FullTurn;
	}
	return {.origin = PointFromDistanceHeadingAndPitch(focus, distance, heading, k_FacePitch), .focus = focus};
}

float script_camera::DualSeconds(float secondsSinceBegun)
{
	if (secondsSinceBegun > k_DualBlendSeconds)
	{
		return k_DualEndSeconds;
	}
	return (k_DualEndSeconds - k_DualStartSeconds) * (secondsSinceBegun / k_DualBlendSeconds) + k_DualStartSeconds;
}

View script_camera::DualView(const DualThing& first, const DualThing& second, DualSettings& settings)
{
	const auto& a = first.position;
	const auto& b = second.position;
	const float averageHeight = (first.height + second.height) * 0.5f;
	const float tallest = first.height > second.height ? first.height : second.height;
	const glm::vec3 focus {(b.x + a.x) * 0.5f, ((b.y + a.y) * 0.5f) + (averageHeight * 0.5f), (b.z + a.z) * 0.5f};
	settings.pitch = settings.pitch > k_DualLowestPitch ? settings.pitch : k_DualLowestPitch;
	// Worked through at the FPU's precision, kept as a float at the end
	const double dx = static_cast<double>(a.x) - b.x;
	const double dz = static_cast<double>(a.z) - b.z;
	const auto distance =
	    static_cast<float>(((std::sqrt((dx * dx) + (dz * dz)) + second.radius + first.radius) * settings.zoom) +
	                       (static_cast<double>(tallest) * k_DualHeightFactor));
	// The heading is turned with the line from the first thing to the second
	const glm::vec3 across = b - a;
	float heading = settings.heading;
	if (std::abs(static_cast<double>(across.x)) > k_DualSameSpot || std::abs(static_cast<double>(across.z)) > k_DualSameSpot)
	{
		heading = settings.heading - static_cast<float>(std::atan2(across.x, -across.z));
	}
	return {.origin = PointFromDistanceHeadingAndPitch(focus, distance, heading, settings.pitch), .focus = focus};
}

ScriptCameraModel::ScriptCameraModel(const glm::vec3& origin, const glm::vec3& focus, GroundHeight groundAt)
    : _groundAt(std::move(groundAt))
{
	Place(_origin, origin);
	Place(_focus, focus);
}

ScriptCameraModel::~ScriptCameraModel() = default;

void ScriptCameraModel::SetOrigin(const glm::vec3& origin)
{
	StopTrack();
	StopFollowing();
	Place(_origin, origin);
}

void ScriptCameraModel::SetFocus(const glm::vec3& focus)
{
	StopTrack();
	StopLookingAt();
	Place(_focus, focus);
}

void ScriptCameraModel::MoveOrigin(const glm::vec3& origin, float seconds)
{
	StopTrack();
	StopFollowing();
	Send(_origin, origin, seconds);
}

void ScriptCameraModel::MoveFocus(const glm::vec3& focus, float seconds)
{
	StopTrack();
	StopLookingAt();
	Send(_focus, focus, seconds);
}

void ScriptCameraModel::StopFollowing()
{
	_followed = nullptr;
	_followedId.reset();
	// A heading kept relative to the thing starts again from straight behind it
	if (_follow.relativeHeading)
	{
		_follow.heading = 0.0f;
	}
}

void ScriptCameraModel::StopLookingAt()
{
	_lookedAt = nullptr;
	_lookedAtId.reset();
}

std::optional<uint32_t> ScriptCameraModel::GetFocusedThing() const
{
	if (InDualView())
	{
		return std::nullopt;
	}
	return _lookedAtId.has_value() ? _lookedAtId : _followedId;
}

void ScriptCameraModel::FollowAndLookAt(Subject thing, float distance)
{
	_followed = std::move(thing.lookup);
	_followedId = _followed ? std::optional(thing.id) : std::nullopt;
	if (_followed)
	{
		const auto view = HeadingAndPitchFromPoints(DestinationOf(_origin), DestinationOf(_focus));
		_follow.heading = view.heading;
		_follow.pitch = view.pitch;
		_follow.viewingDistance = distance;
	}
	SnapToFollowed();
}

void ScriptCameraModel::Follow(Subject thing)
{
	_followed = std::move(thing.lookup);
	_followedId = _followed ? std::optional(thing.id) : std::nullopt;
	if (_followed)
	{
		const auto view = HeadingAndPitchFromPoints(DestinationOf(_origin), DestinationOf(_focus));
		_follow.heading = view.heading;
		_follow.pitch = view.pitch;
		const auto followed = _followed();
		_follow.viewingDistance = followed.has_value() ? followed->height * k_FollowHeights : 0.0f;
	}
	if (_follow.relativeHeading)
	{
		_follow.heading = 0.0f;
	}
	SnapToFollowed();
}

void ScriptCameraModel::LookAt(Subject thing)
{
	StopTrack();
	_lookedAt = std::move(thing.lookup);
	_lookedAtId = _lookedAt ? std::optional(thing.id) : std::nullopt;
}

void ScriptCameraModel::StopTrack()
{
	_track = nullptr;
	_trackRunner.reset();
}

void ScriptCameraModel::RunTrack(std::shared_ptr<const edt::EDTTrack> track)
{
	StopLookingAt();
	StopTrack();
	_trackTime = std::chrono::milliseconds::zero();
	if (track == nullptr || track->position.points.empty() || track->focus.points.empty())
	{
		return;
	}
	_track = std::move(track);
	_trackRunner = std::make_unique<camera_track::WayRunner>(_track->position);
}

void ScriptCameraModel::PassGameTime(std::chrono::milliseconds gameTime)
{
	// Under a two-thing camera the track holds where it is
	if (_track != nullptr && _duals.empty())
	{
		_trackTime += gameTime;
	}
}

void ScriptCameraModel::UpdateTrack()
{
	// Held at its start and its end
	const auto duration = _track->position.duration;
	const auto time = static_cast<int32_t>(std::clamp<int64_t>(_trackTime.count(), 0, duration));
	const auto origin = _trackRunner->Get(_track->position, time);
	const auto focus = camera_track::Bezier(_track->focus, _trackRunner->GetSegment(), _trackRunner->GetParameter());
	// Put there still: the glides start from rest at the track's point
	Place(_origin, origin);
	Place(_focus, focus);
}

void ScriptCameraModel::StartDualView(DualSubject first, DualSubject second)
{
	// The same two things again change nothing
	if (!_duals.empty() && _duals.back().first.id == first.id && _duals.back().second.id == second.id)
	{
		return;
	}
	_duals.push_back({.first = std::move(first), .second = std::move(second), .settings = {}});
	_seconds = 0.0f;
}

void ScriptCameraModel::UpdateDualView(DualSubject first, DualSubject second)
{
	if (_duals.empty())
	{
		return;
	}
	auto& dual = _duals.back();
	dual.first = std::move(first);
	dual.second = std::move(second);
	dual.lastFirst.reset();
	dual.lastSecond.reset();
}

void ScriptCameraModel::LetGoOfGoneThings()
{
	const auto gone = [](const DualSubject& subject) { return !subject.lookup || !subject.lookup().has_value(); };
	std::erase_if(_duals, [&gone](const Dual& dual) { return gone(dual.first) || gone(dual.second); });
}

bool ScriptCameraModel::ReleaseDualView()
{
	if (_duals.empty())
	{
		return false;
	}
	_duals.pop_back();
	_seconds = 0.0f;
	return true;
}

void ScriptCameraModel::UpdateDual()
{
	auto& dual = _duals.back();
	// A thing gone is framed where it was last seen until the turn lets go of it
	if (auto first = dual.first.lookup ? dual.first.lookup() : std::nullopt; first.has_value())
	{
		dual.lastFirst = first;
	}
	if (auto second = dual.second.lookup ? dual.second.lookup() : std::nullopt; second.has_value())
	{
		dual.lastSecond = second;
	}
	if (!dual.lastFirst.has_value() || !dual.lastSecond.has_value())
	{
		return;
	}
	const float seconds = DualSeconds(_seconds);
	const auto view = DualView(*dual.lastFirst, *dual.lastSecond, dual.settings);
	Send(_focus, view.focus, seconds);
	Send(_origin, view.origin, seconds);
}

void ScriptCameraModel::SetFollowSettings(float distance, float zoomTimeScale, float heading, bool relativeHeading)
{
	_follow.viewingDistance = distance;
	_follow.zoomTimeScale = zoomTimeScale;
	_follow.heading = heading;
	_follow.relativeHeading = relativeHeading;
}

void ScriptCameraModel::SnapToFollowed()
{
	const auto followed = _followed ? _followed() : std::nullopt;
	const auto lookedAt = _lookedAt ? _lookedAt() : followed;
	if (lookedAt.has_value())
	{
		Place(_focus, lookedAt->point);
	}
	if (followed.has_value())
	{
		Place(_origin, FollowOrigin(*followed, _follow));
	}
	_seconds = k_FollowBlendSeconds;
}

void ScriptCameraModel::UpdateFollowing()
{
	// A thing that is gone is followed no more
	auto followed = _followed ? _followed() : std::nullopt;
	if (_followed && !followed.has_value())
	{
		_followed = nullptr;
		_followedId.reset();
	}
	auto lookedAt = _lookedAt ? _lookedAt() : std::nullopt;
	if (_lookedAt && !lookedAt.has_value())
	{
		_lookedAt = nullptr;
		_lookedAtId.reset();
	}
	_follow.viewingDistance =
	    _follow.viewingDistance > k_FollowNearest ? std::min(_follow.viewingDistance, k_FollowFarthest) : k_FollowNearest;
	const float seconds = FollowSeconds(_seconds, _follow.zoomTimeScale);
	if (const auto& focus = lookedAt.has_value() ? lookedAt : followed; focus.has_value())
	{
		Send(_focus, focus->point, seconds);
	}
	if (followed.has_value())
	{
		Send(_origin, FollowOrigin(*followed, _follow), seconds);
	}
}

bool ScriptCameraModel::Arrived() const
{
	// A two-thing camera has arrived when its glides have; the script's track is held under it
	if (_track != nullptr && _duals.empty())
	{
		return _track->position.duration <= _trackTime.count();
	}
	return DistanceSquared(DestinationOf(_origin), ValueOf(_origin)) < k_ArrivedDistanceSquared &&
	       DistanceSquared(DestinationOf(_focus), ValueOf(_focus)) < k_ArrivedDistanceSquared;
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> ScriptCameraModel::Update(std::chrono::microseconds dt,
                                                                                    const Camera& /*camera*/)
{
	const float seconds = std::min(std::chrono::duration<float>(dt).count(), k_MaxFrameSeconds);
	_seconds += seconds;
	if (!_duals.empty())
	{
		UpdateDual();
	}
	else
	{
		// A track puts the camera where it has got to before any following sends it on
		if (_track != nullptr)
		{
			UpdateTrack();
		}
		UpdateFollowing();
	}
	for (auto& zoomer : _origin)
	{
		zoomer.Update(seconds);
	}
	for (auto& zoomer : _focus)
	{
		zoomer.Update(seconds);
	}
	const auto view = Drawn({.origin = ValueOf(_origin), .focus = ValueOf(_focus)}, _groundAt);
	// Sent out of the world, it turns back to its edge
	if (const auto pulled = PullIntoWorld(DestinationOf(_origin)); pulled.has_value())
	{
		Send(_origin, *pulled, k_WorldReturnSeconds);
	}
	// The camera is drawn where the coordinates are, without a glide of its own
	return CameraInterpolationUpdateInfo {
	    .origin = view.origin, .focus = view.focus, .duration = std::chrono::microseconds::zero()};
}

void ScriptCameraModel::HandleActions(std::chrono::microseconds /*dt*/)
{
	// The player has no say while a script has the camera
}

void ScriptCameraModel::SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/)
{
	// Nor do the player's flights to places
}

glm::vec3 ScriptCameraModel::GetTargetOrigin() const
{
	return DestinationOf(_origin);
}

glm::vec3 ScriptCameraModel::GetTargetFocus() const
{
	return DestinationOf(_focus);
}

std::chrono::seconds ScriptCameraModel::GetIdleTime() const
{
	return std::chrono::seconds::zero();
}
