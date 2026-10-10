/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WalkPathSystem.h"

#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Components/Whale.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

bool WalkPathSystem::Start(entt::entity thing, int32_t number, bool forward, float from, float to)
{
	auto track = camera_edits::FindTrack(number);
	if (track == nullptr)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "Cannot load track No {}", number);
		return false;
	}
	auto walk = camera_track::StartWalk(*track, forward, from, to);
	Locator::entitiesRegistry::value().AssignOrReplace<WalkPath>(
	    thing, WalkPath {.number = number, .track = std::move(track), .walk = std::move(walk)});
	return true;
}

void WalkPathSystem::ProcessTurn()
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	std::vector<entt::entity> done;
	registry.Each<WalkPath, Transform>([&](entt::entity entity, WalkPath& path, Transform& transform) {
		// A living walks its own track in its state
		if (path.living)
		{
			return;
		}
		const auto at = camera_track::WalkTurn(path.walk, *path.track);
		if (!at.has_value())
		{
			done.push_back(entity);
			return;
		}
		// On the land there; a whale moves for its turn and is drawn getting there
		const glm::vec3 position(at->x, land.GetHeightAt(*at), at->y);
		if (auto* whale = registry.TryGet<Whale>(entity))
		{
			whale->position = position;
		}
		else
		{
			transform.position = position;
		}
	});
	for (const auto entity : done)
	{
		registry.Remove<WalkPath>(entity);
	}
	registry.SetDirty();
}
