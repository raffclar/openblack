/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraControl.h"

#include <cmath>

#include <utility>

#include <InspectorQuery.h>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

using namespace openblack::inspector;

namespace
{

/// The camera never stands closer to what it looks at than this, nor looks straight up or down
constexpr float k_ClosestDistance = 1.0f;
constexpr float k_SteepestPitch = 89.0f;
constexpr float k_FarthestDistance = 10000.0f;

Json Point(const glm::vec3& point)
{
	return Json::array({point.x, point.y, point.z});
}

std::optional<glm::vec3> PointParam(const Json& params, std::string_view key, const CameraControlInterface& camera,
                                    bool allowGround, std::string& error)
{
	const auto it = params.find(key);
	if (it == params.end())
	{
		return std::nullopt;
	}
	bool planar = false;
	const auto point = ReadPoint(*it, &planar);
	if (!point.has_value() || (planar && !allowGround))
	{
		error = std::string(key) + (allowGround ? " is [x, z] on the land or [x, y, z]" : " is [x, y, z]");
		return std::nullopt;
	}
	glm::vec3 value {static_cast<float>((*point)[0]), static_cast<float>((*point)[1]), static_cast<float>((*point)[2])};
	if (planar)
	{
		value.y = camera.GroundHeight({value.x, value.z});
	}
	return value;
}

} // namespace

std::optional<CameraPose> openblack::inspector::ResolveCameraPose(const Json& params, const CameraState& now,
                                                                  const CameraControlInterface& camera, std::string& error)
{
	const auto position = PointParam(params, "position", camera, false, error);
	const auto focus = PointParam(params, "focus", camera, true, error);
	if (!error.empty())
	{
		return std::nullopt;
	}
	const CameraPose current {.origin = now.origin, .focus = now.focus};
	auto angles = AnglesOf(current);
	const auto yaw = NumberMember(params, "yaw");
	const auto pitch = NumberMember(params, "pitch");
	const auto distance = NumberMember(params, "distance");
	const bool turned = yaw.has_value() || pitch.has_value() || distance.has_value();
	if (pitch.has_value() && std::abs(*pitch) > k_SteepestPitch)
	{
		error = "pitch is from -89 to 89 degrees below the horizon";
		return std::nullopt;
	}
	if (distance.has_value() && (*distance < k_ClosestDistance || *distance > k_FarthestDistance))
	{
		error = "distance is from 1 to 10000";
		return std::nullopt;
	}
	if (position.has_value() && turned)
	{
		error = "give a position and a focus, or a focus with angles, not a position with angles";
		return std::nullopt;
	}
	if (!position.has_value() && !focus.has_value() && !turned)
	{
		error = "give a position, a focus, or yaw, pitch or distance";
		return std::nullopt;
	}
	angles.yaw = static_cast<float>(yaw.value_or(angles.yaw));
	angles.pitch = static_cast<float>(pitch.value_or(angles.pitch));
	angles.distance = static_cast<float>(distance.value_or(angles.distance));

	CameraPose pose = current;
	if (position.has_value() && focus.has_value())
	{
		pose = {.origin = *position, .focus = *focus};
	}
	else if (position.has_value())
	{
		// It looks the way it did from its new place
		pose = {.origin = *position, .focus = *position + (current.focus - current.origin)};
	}
	else
	{
		pose.focus = focus.value_or(current.focus);
		pose.origin = OriginFor(pose.focus, angles);
	}
	if (glm::distance(pose.origin, pose.focus) < k_ClosestDistance)
	{
		error = "the position is too close to the focus";
		return std::nullopt;
	}
	return pose;
}

namespace
{

Json StateJson(const CameraState& state)
{
	const auto angles = AnglesOf({.origin = state.origin, .focus = state.focus});
	return {
	    {"origin", Point(state.origin)},
	    {"focus", Point(state.focus)},
	    {"yaw", angles.yaw},
	    {"pitch", angles.pitch},
	    {"distance", angles.distance},
	    {"rotation", Point(state.rotation)},
	    {"forward", Point(state.forward)},
	    {"horizontal_fov", state.horizontalFieldOfView},
	    {"near_clip", state.nearClip},
	    {"model", state.model},
	    {"held_by_path", state.heldByPath},
	};
}

ParameterDescription Optional(std::string name, std::string type, std::string description)
{
	return {.name = std::move(name), .type = std::move(type), .description = std::move(description), .required = false};
}

} // namespace

CameraAngles openblack::inspector::AnglesOf(const CameraPose& pose)
{
	const auto towards = pose.focus - pose.origin;
	const auto distance = glm::length(towards);
	if (distance <= 0.0f)
	{
		return {};
	}
	const auto direction = towards / distance;
	return {.yaw = glm::degrees(std::atan2(direction.x, direction.z)),
	        .pitch = glm::degrees(std::asin(-direction.y)),
	        .distance = distance};
}

glm::vec3 openblack::inspector::OriginFor(glm::vec3 focus, const CameraAngles& angles)
{
	const auto yaw = glm::radians(angles.yaw);
	const auto pitch = glm::radians(angles.pitch);
	const glm::vec3 direction {std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
	return focus - direction * angles.distance;
}

std::unique_ptr<ProviderInterface> openblack::inspector::MakeCameraProvider(CameraControlInterface& camera)
{
	auto provider = std::make_unique<FunctionProvider>("camera");
	provider->Add({.name = "state",
	               .description = "Where the camera is, what it looks at, its yaw, pitch (degrees below the horizon) and "
	                              "distance, its field of view, what moves it, and whether a camera path holds it",
	               .parameters = {},
	               .kind = ResultKind::Object,
	               .needsNear = false},
	              [&camera](const QueryContext& /*context*/) {
		              const auto state = camera.State();
		              return state.has_value() ? QueryResult::Value(StateJson(*state))
		                                       : QueryResult::Error("there is no camera");
	              });
	const std::vector<ParameterDescription> pose {
	    Optional("position", "point", "Where the camera stands, [x, y, z]"),
	    Optional("focus", "point", "What it looks at, [x, z] on the land or [x, y, z]"),
	    Optional("yaw", "number", "Degrees about the up axis: 0 looks along +z, 90 along +x"),
	    Optional("pitch", "number", "Degrees below the horizon"),
	    Optional("distance", "number", "From the camera to its focus: the zoom"),
	};
	const auto move = [&camera](bool fly) {
		return [&camera, fly](const QueryContext& context) {
			const auto now = camera.State();
			if (!now.has_value())
			{
				return QueryResult::Error("there is no camera");
			}
			std::string error;
			const auto pose = ResolveCameraPose(context.params, *now, camera, error);
			if (!pose.has_value())
			{
				return QueryResult::Error(error);
			}
			if (auto why = fly ? camera.Fly(*pose) : camera.Set(*pose); !why.empty())
			{
				return QueryResult::Error(why);
			}
			const auto angles = AnglesOf(*pose);
			return QueryResult::Value({
			    {fly ? "flying_to" : "set_to",
			     {{"origin", Point(pose->origin)},
			      {"focus", Point(pose->focus)},
			      {"yaw", angles.yaw},
			      {"pitch", angles.pitch},
			      {"distance", angles.distance}}},
			    {"model", now->model},
			});
		};
	};
	provider->Add({.name = "set",
	               .description = "Puts the camera somewhere at once, as the scripts place it; what isn't given is kept. "
	                              "Refused while a camera path holds the camera",
	               .parameters = pose,
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              move(false));
	provider->Add({.name = "fly",
	               .description = "Flies the camera somewhere as the bookmarks fly it; read camera.state as it goes",
	               .parameters = pose,
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              move(true));
	return provider;
}
