/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileWalkPaths.h"

#include <cstdlib>

#include <algorithm>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/MobileWalkPath.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs
{
using components::MobileWalkPath;
using components::Transform;

namespace
{
bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_WALK_PATH_TRACE") != nullptr;
	return trace;
}

/// The position is set through MapCoords, fixed point: x * 6553.6 truncated toward zero, back to x * 1/6553.6 when the 3D
/// object is placed
float MapCoordsRound(float x)
{
	return map_coords::Quantise(x);
}
} // namespace

glm::vec3 SampleWalkPath(MobileWalkPath& walk, int32_t* sampleOut)
{
	const auto& track = *walk.track;
	const int32_t duration = track.position.duration; // the running position way's
	// the sample, current (forward) or duration - current, truncated toward zero, clamped to 0..duration
	int32_t sample =
	    walk.forward ? static_cast<int32_t>(walk.current) : static_cast<int32_t>(static_cast<float>(duration) - walk.current);
	sample = std::clamp(sample, 0, duration);
	if (sampleOut != nullptr)
	{
		*sampleOut = sample;
	}
	// the position way run to the sample (its point is dropped), then the focus way's Bezier with the segment and t it
	// left
	walk.runner->Get(sample);
	return track.focus.Bezier(walk.runner->Segment(), walk.runner->Parameter());
}

bool StartMobileWalkPath(entt::entity entity, int32_t path, bool forward, float from, float to)
{
	auto track = LoadCameraTrack(path);
	if (track == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the list keeps the object once; its DataPath is replaced (the old one is leaked in the original)
	auto& walk = registry.AssignOrReplace<MobileWalkPath>(entity);
	walk.runner = std::make_unique<CameraWayRunner>(track->position);
	walk.track = std::move(track);
	walk.to = to;
	walk.forward = forward;
	walk.step = 100.0f;
	walk.cachedSpeed = 1.0f;
	// duration (as a float) * from
	walk.current = static_cast<float>(walk.track->position.duration) * from;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "WALK_PATH: object {} track {} forward {} from {} to {} ({} ms, {} points)",
		                   static_cast<uint32_t>(entity), path, forward, from, to, walk.track->position.duration,
		                   walk.track->position.points.size());
	}
	return true;
}

void ProcessMobileWalkPaths()
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	std::vector<entt::entity> done;
	registry.Each<MobileWalkPath, Transform>(
	    [&](entt::entity entity, MobileWalkPath& walk, Transform& transform) {
		    const auto& track = *walk.track;
		    const int32_t duration = track.position.duration; // the running position way's
		    int32_t sample = 0;
		    const auto point = SampleWalkPath(walk, &sample);
		    // done: current / duration reached `to`
		    if (!(walk.current / static_cast<float>(duration) < walk.to))
		    {
			    // out of the list, without moving it this turn
			    if (Trace())
			    {
				    SPDLOG_LOGGER_INFO(spdlog::get("game"), "WALK_PATH: object {} done at {}", static_cast<uint32_t>(entity),
				                       walk.current);
			    }
			    done.push_back(entity);
			    return;
		    }
		    walk.current += walk.step;
		    if (static_cast<float>(duration) < walk.current)
		    {
			    walk.current = static_cast<float>(duration);
		    }
		    // the position (x, z, relative y = 0), then the 3D object's (coords, no rotation, scale 1.0):
		    // y = GetAltitude + 0. The latter also resets the 3D object's rotation and scale, which the shark's draw sets
		    // again every frame (ECS/Sharks.cpp), so only the position is kept here.
		    const float x = MapCoordsRound(point.x);
		    const float z = MapCoordsRound(point.z);
		    transform.position = glm::vec3(x, island.GetHeightAt(glm::vec2(x, z)), z);
		    if (Trace())
		    {
			    SPDLOG_LOGGER_INFO(
			        spdlog::get("game"),
			        "WALK_PATH: object {} sample {} segment {} t {:.6f} focus ({:.4f}, {:.4f}, {:.4f}) -> ({:.4f}, "
			        "{:.4f}, {:.4f})",
			        static_cast<uint32_t>(entity), sample, walk.runner->Segment(), walk.runner->Parameter(), point.x, point.y,
			        point.z, transform.position.x, transform.position.y, transform.position.z);
		    }
	    },
	    entt::exclude<components::Unavailable>);
	for (const auto entity : done)
	{
		registry.Remove<MobileWalkPath>(entity);
	}
}

} // namespace openblack::ecs
