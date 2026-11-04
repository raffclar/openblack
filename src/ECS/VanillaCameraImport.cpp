/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaCameraImport.h"

#include <cstring>

#include <algorithm>
#include <chrono>
#include <string_view>

#include "VanillaWorld.h"

namespace openblack::ecs::vanilla_camera_import
{

namespace
{

/// The three glides of a point of the file, x, y and z. None when the file does not have all three.
std::optional<std::array<Glide, 3>> GlidesOf(std::span<const uint8_t> bytes)
{
	if (bytes.size() != 3 * k_GlideBytes)
	{
		return std::nullopt;
	}
	std::array<Glide, 3> glides {};
	for (size_t axis = 0; axis < glides.size(); ++axis)
	{
		glides.at(axis) = GlideOf(bytes.subspan(axis * k_GlideBytes).first<k_GlideBytes>());
	}
	return glides;
}

glm::vec3 PointOf(std::span<const uint8_t> bytes)
{
	glm::vec3 point {0.0f};
	if (bytes.size() == sizeof(point))
	{
		std::memcpy(&point, bytes.data(), sizeof(point));
	}
	return point;
}

std::chrono::microseconds MicrosecondsOf(float seconds)
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(seconds));
}

/// The glide of one point: the three coordinates share their seconds, so that the camera times them together
struct PointGlide
{
	ZoomInterpolator3f interpolator;
	std::chrono::microseconds time;
	std::chrono::microseconds duration;
};

std::optional<PointGlide> PointGlideOf(const std::array<Glide, 3>& glides)
{
	const auto& [x, y, z] = glides;
	if (x.elapsed != y.elapsed || x.elapsed != z.elapsed || x.duration != y.duration || x.duration != z.duration)
	{
		return std::nullopt;
	}
	const glm::vec3 start {x.start, y.start, z.start};
	const glm::vec3 target {x.target, y.target, z.target};
	// The camera's glide takes its speeds as distances over the whole glide
	const glm::vec3 startSpeed = glm::vec3 {x.startSpeed, y.startSpeed, z.startSpeed} * x.duration;
	const glm::vec3 targetSpeed = glm::vec3 {x.targetSpeed, y.targetSpeed, z.targetSpeed} * x.duration;
	const auto duration = MicrosecondsOf(x.duration);
	// A glide that is over stops at its target
	const auto time = std::min(MicrosecondsOf(x.elapsed), duration);
	return PointGlide {
	    .interpolator = ZoomInterpolator3f(start, target, startSpeed, targetSpeed), .time = time, .duration = duration};
}

} // namespace

Glide GlideOf(std::span<const uint8_t, k_GlideBytes> bytes)
{
	std::array<float, k_GlideBytes / sizeof(float)> values {};
	std::memcpy(values.data(), bytes.data(), bytes.size());
	// The game does not read the fifth number
	return {.value = values[0],
	        .target = values[1],
	        .targetSpeed = values[2],
	        .speed = values[3],
	        .elapsed = values[5],
	        .duration = values[6],
	        .start = values[7],
	        .startSpeed = values[8],
	        .c2 = values[9],
	        .c3 = values[10],
	        .c4 = values[11]};
}

std::optional<Mode> ModeOf(uint32_t number)
{
	switch (number)
	{
	case 0:
		return Mode::World;
	case 0x46:
		return Mode::Follow;
	case 0x48:
		return Mode::FollowAndTurn;
	case 0x65:
		return Mode::Free;
	default:
		return std::nullopt;
	}
}

std::optional<ImportedCamera> ReadCamera(const gamesave::VanillaWorld& world)
{
	const auto records = world.OfType(gamesave::VanillaType::Camera);
	if (records.empty())
	{
		return std::nullopt;
	}
	const auto& record = *records.front();
	const auto focus = GlidesOf(world.Raw(record, "cameraFocusGlide"));
	const auto eye = GlidesOf(world.Raw(record, "cameraEyeGlide"));
	const auto lens = world.Raw(record, "cameraLensGlide");
	const auto mode = ModeOf(world.Word(record, "mode").value_or(0));
	if (!focus.has_value() || !eye.has_value() || lens.size() != k_GlideBytes || !mode.has_value())
	{
		return std::nullopt;
	}
	ImportedCamera camera {.focus = *focus,
	                       .eye = *eye,
	                       .lens = GlideOf(lens.first<k_GlideBytes>()),
	                       .belowLand = world.Byte(record, "cameraBelowLand").value_or(0) != 0,
	                       .mode = *mode,
	                       .follow = std::nullopt,
	                       .free = std::nullopt};
	if (*mode == Mode::Follow || *mode == Mode::FollowAndTurn)
	{
		camera.follow = Follow {
		    .heading = world.Float(record, "followHeading").value_or(0.0f),
		    .pitch = world.Float(record, "followPitch").value_or(0.0f),
		    .distance = world.Float(record, "followDistance").value_or(0.0f),
		    .timeScale = world.Float(record, "followTimeScale").value_or(0.0f),
		    .turnSpeed = world.Float(record, "followTurnSpeed").value_or(0.0f),
		    .relativeHeading = world.Word(record, "followRelativeHeading").value_or(0) != 0,
		    .useOffsets = world.Word(record, "followUseOffsets").value_or(0) != 0,
		    .focusOffset = PointOf(world.Raw(record, "followFocusOffset")),
		    .eyeOffset = PointOf(world.Raw(record, "followEyeOffset")),
		    .target = world.Ref(record, "followTarget").value_or(0),
		    .autoReposition = world.Word(record, "followAutoReposition").value_or(0) != 0,
		};
	}
	if (*mode == Mode::Free)
	{
		const auto keys = world.Word(record, "freeKeys").value_or(0);
		camera.free = Free {
		    .speed = world.Float(record, "freeSpeed").value_or(0.0f),
		    .across = static_cast<int32_t>(world.Word(record, "freeMoveX").value_or(0)),
		    .along = static_cast<int32_t>(world.Word(record, "freeMoveY").value_or(0)),
		    .shift = (keys & k_ShiftKey) != 0,
		    .ctrl = (keys & k_CtrlKey) != 0,
		};
	}
	return camera;
}

std::optional<Camera::Motion> MotionOf(const std::array<Glide, 3>& eye, const std::array<Glide, 3>& focus)
{
	const auto eyeGlide = PointGlideOf(eye);
	const auto focusGlide = PointGlideOf(focus);
	if (!eyeGlide.has_value() || !focusGlide.has_value())
	{
		return std::nullopt;
	}
	return Camera::Motion {.origin = eyeGlide->interpolator,
	                       .focus = focusGlide->interpolator,
	                       .time = eyeGlide->time,
	                       .duration = eyeGlide->duration,
	                       .focusTime = focusGlide->time,
	                       .focusDuration = focusGlide->duration};
}

camera::Zoomer LensOf(const Glide& lens)
{
	camera::Zoomer zoomer;
	camera::Zoomer::VisitSaved(zoomer,
	                           [&lens](float& value, float& speed, float& destination, float& destinationSpeed, float& time,
	                                   float& duration, float& startValue, float& startSpeed, float& c1, float& c2, float& c3) {
		                           value = lens.value;
		                           speed = lens.speed;
		                           destination = lens.target;
		                           destinationSpeed = lens.targetSpeed;
		                           time = lens.elapsed;
		                           duration = lens.duration;
		                           startValue = lens.start;
		                           startSpeed = lens.startSpeed;
		                           c1 = lens.c2;
		                           c2 = lens.c3;
		                           c3 = lens.c4;
	                           });
	return zoomer;
}

glm::vec3 ValueOf(const std::array<Glide, 3>& glides)
{
	return {glides[0].value, glides[1].value, glides[2].value};
}

} // namespace openblack::ecs::vanilla_camera_import
