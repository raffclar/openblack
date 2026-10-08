/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingWalkPath.h"

#include <algorithm>
#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/MobileWalkPath.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingAngle.h"
#include "ECS/MapCells.h"
#include "ECS/MobileWalkPaths.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "Locator.h"

namespace openblack::ecs::living
{
using components::DrawPosition;
using components::LivingWalkPath;
using components::Transform;
using components::Villager;
using components::WallHug;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The speed (map units a turn) in the path's units: / 655 x 0.1
constexpr float k_SpeedDivisor = 655.0f;
constexpr float k_SpeedScale = 0.1f;
/// Below this length there is no step; above this squared distance the walker turns
constexpr float k_MinLength = 0.01f;
constexpr float k_MinTurnSq = 0.001f;

/// The u16 speed the original keeps (ecs::WholeSpeed, ECS/VillagerSpeed.h); 0 without a WallHug
uint16_t WholeSpeed(entt::entity living)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(living);
	return wallHug != nullptr ? ecs::WholeSpeed(*wallHug) : 0;
}

/// (float) speed / 655 x 0.1 (at 24 bits)
float PathSpeed(uint16_t speed)
{
	return static_cast<float>(speed) / k_SpeedDivisor * k_SpeedScale;
}

/// The position at the start of the turn (DrawPosition::turnStart); (openblack) before the first turn has started
/// there is no copy yet, so the current position (the Transform)
glm::vec3 TurnStartOf(entt::entity living)
{
	if (const auto* draw = Entities().TryGet<const DrawPosition>(living); draw != nullptr && draw->started)
	{
		return draw->turnStart;
	}
	const auto* transform = Entities().TryGet<const Transform>(living);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}
} // namespace

bool StartWalkPath(entt::entity living, int32_t track, VillagerStates final, float from, float to, bool forward)
{
	auto& registry = Entities();
	if (!registry.AllOf<Villager>(living))
	{
		return false; // (pending) an animal's state 28
	}
	auto camera = LoadCameraTrack(track);
	if (camera == nullptr)
	{
		return false;
	}
	// the old path goes; a new one
	auto& walk = registry.AssignOrReplace<LivingWalkPath>(living);
	walk.path.runner = std::make_unique<CameraWayRunner>(camera->position);
	walk.path.track = std::move(camera);
	walk.trackNumber = track;
	walk.path.to = to;
	walk.path.forward = forward;
	// duration / (focus.length / (speed / 655 x 0.1))
	const auto speed = WholeSpeed(living);
	const float s = PathSpeed(speed);
	const float length = walk.path.track->focus.length / s;
	walk.path.step = static_cast<float>(walk.path.track->position.duration) / length;
	walk.path.cachedSpeed = static_cast<float>(speed); // the speed as a float
	// duration x from
	walk.path.current = static_cast<float>(walk.path.track->position.duration) * from;
	// the states set (28, final); when that succeeds, the state's animation
	if (villager::SetCurrentAndDestinationState(living, VillagerStates::MoveAlongPath, final) != 1)
	{
		return false;
	}
	VillagerSetStateClip(living, true);
	return true;
}

uint32_t MoveAlongPath(components::LivingAction& action)
{
	auto& registry = Entities();
	const auto living = registry.ToEntity(action);
	auto* walk = registry.TryGet<LivingWalkPath>(living);
	if (walk == nullptr || walk->path.track == nullptr)
	{
		return 1;
	}
	auto& path = walk->path;
	const auto& track = *path.track;
	// the speed changed: s = speed / 655 x 0.1, L = s != 0 ? length / s : 0, step = L > 0.01 ? duration / L : 0, the
	// state's animation again, the speed kept
	const auto speed = WholeSpeed(living);
	if (static_cast<float>(speed) != path.cachedSpeed)
	{
		path.cachedSpeed = static_cast<float>(speed);
		const float s = PathSpeed(speed);
		float length = 0.0f;
		if (s != 0.0f)
		{
			length = track.focus.length / s;
		}
		path.step = 0.0f;
		if (length > k_MinLength)
		{
			path.step = static_cast<float>(track.position.duration) / length;
		}
		VillagerSetStateClip(living, true);
		path.cachedSpeed = static_cast<float>(speed);
	}
	// the mobile object path's sample and point
	const auto point = SampleWalkPath(path);
	// GetWalkPathPercentage < to, else SetTopStateToFinal
	const auto duration = static_cast<float>(track.position.duration);
	if (!(path.current / duration < path.to))
	{
		villager::SetTopStateToFinal(living);
		return 1;
	}
	// current += step, at most the duration
	path.current = path.step + path.current;
	if (duration < path.current)
	{
		path.current = duration;
	}
	// from the position at the turn's start: (start - point) squared, z then x; above 0.001 -> SetYAngle of the angle
	// from start to point (rounded to a float)
	const auto start = TurnStartOf(living);
	const float dx = start.x - point.x;
	const float dz = start.z - point.z;
	if (dz * dz + dx * dx > k_MinTurnSq)
	{
		SetYAngle(living, static_cast<float>(affine::GetYAngleBetween(start, point)));
	}
	// moved to the point quantised to map coordinates, relative y 0: on the land
	const float x = map_coords::Quantise(point.x);
	const float z = map_coords::Quantise(point.z);
	const float y = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
	map_cells::MoveMapObject(living, glm::vec3(x, y, z));
	return 1;
}

std::optional<float> GetWalkPathPercentage(entt::entity living)
{
	const auto* walk = Entities().TryGet<const LivingWalkPath>(living);
	if (walk == nullptr || walk->path.track == nullptr)
	{
		return std::nullopt;
	}
	return walk->path.current / static_cast<float>(walk->path.track->position.duration);
}

bool WalkPathReached(entt::entity living, float v)
{
	// no path -> true; GetWalkPathPercentage >= v -> true
	const auto percentage = GetWalkPathPercentage(living);
	return !percentage.has_value() || !(*percentage < v);
}

} // namespace openblack::ecs::living
