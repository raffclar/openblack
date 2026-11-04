/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>

#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/CreatureFollow.h"
#include "Camera/FollowHeadingCameraModel.h"
#include "Camera/FreeCameraModel.h"
#include "ECS/Components/Transform.h"
#include "ECS/FollowHeadingThing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/VanillaCameraImport.h"
#include "Locator.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace camera_import = openblack::ecs::vanilla_camera_import;
using openblack::ecs::systems::vanilla_import::EntityOf;

namespace
{

std::shared_ptr<spdlog::logger> ImportLog()
{
	return spdlog::get("game");
}

} // namespace

void SaveGameSystem::ImportVanillaCameraMode(const camera_import::ImportedCamera& file, const VanillaMade& made)
{
	auto& camera = Locator::camera::value();
	const auto& registry = Locator::entitiesRegistry::value();
	switch (file.mode)
	{
	case camera_import::Mode::World:
		return;
	case camera_import::Mode::Follow:
	{
		const auto& follow = file.follow.value_or(camera_import::Follow {});
		const auto thing = EntityOf(made, follow.target);
		// A camera with no thing, or with a thing that is gone, ends at once: the world camera stays
		if (thing == entt::null || !registry.Valid(thing))
		{
			SPDLOG_LOGGER_INFO(ImportLog(), "The follow camera of the file has no thing, so the world camera stays");
			return;
		}
		// A flock is followed at the place of its leader
		if (!ecs::CameraTargetOf(registry, thing).has_value())
		{
			SPDLOG_LOGGER_WARN(ImportLog(),
			                   "The follow camera of the file follows a thing with no place (object {}), so the world "
			                   "camera stays.",
			                   follow.target);
			return;
		}
		const creature_follow::Loaded loaded {
		    .view = {.yaw = follow.heading, .pitch = follow.pitch, .distance = follow.distance},
		    .timeScale = follow.timeScale,
		    .autoReposition = follow.autoReposition,
		    .relativeHeading = follow.relativeHeading,
		    .offsets = follow.useOffsets
		                   ? std::optional(creature_follow::Offsets {.focus = follow.focusOffset, .eye = follow.eyeOffset})
		                   : std::nullopt,
		};
		if (!Locator::creatureModeSystem::has_value() || !Locator::creatureModeSystem::value().EnterLoaded(thing, loaded))
		{
			SPDLOG_LOGGER_WARN(ImportLog(), "The follow camera of the file cannot take the camera, so the world camera stays");
			return;
		}
		SPDLOG_LOGGER_INFO(ImportLog(), "The camera of the file follows object {}", follow.target);
		return;
	}
	case camera_import::Mode::FollowAndTurn:
	{
		const auto& follow = file.follow.value_or(camera_import::Follow {});
		const auto thing = EntityOf(made, follow.target);
		if (thing == entt::null || !registry.Valid(thing) || !registry.AllOf<Transform>(thing))
		{
			SPDLOG_LOGGER_INFO(ImportLog(), "The turning follow camera of the file has no thing, so the world camera stays");
			return;
		}
		const follow_heading::Settings settings {
		    .heading = follow.heading,
		    .pitch = follow.pitch,
		    .distance = follow.distance,
		    .headingSpeed = follow.turnSpeed,
		};
		camera.SetModel(std::make_unique<FollowHeadingCameraModel>(
		    ecs::FollowHeadingThing(thing), settings, camera.HeldFollowHeading(), camera.GetOrigin(), camera.GetFocus()));
		SPDLOG_LOGGER_INFO(ImportLog(), "The camera of the file follows object {} and turns with it", follow.target);
		return;
	}
	case camera_import::Mode::Free:
	{
		const auto& free = file.free.value_or(camera_import::Free {});
		const free_camera::State state {
		    .speed = free.speed,
		    .across = free.across,
		    .along = free.along,
		    .shift = free.shift,
		    .ctrl = free.ctrl,
		};
		camera.SetModel(std::make_unique<FreeCameraModel>(camera.GetOrigin(), camera.GetFocus(), state));
		SPDLOG_LOGGER_INFO(ImportLog(), "The camera of the file is a free camera");
		return;
	}
	}
}
