/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Sharks.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraTracks.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileWalkPaths.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/WaterRings.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs
{
using components::DrawPosition;
using components::Shark;
using components::Transform;

namespace
{
/// What this module keeps between calls (Locator::worldEffects)
struct SharksState
{
	/// One int timer for all the sharks, each one adds the frame's whole milliseconds to it (two sharks share
	/// the rings)
	int32_t wakeTimer {0};
};

SharksState& SharksData()
{
	return openblack::Locator::worldEffects::value().Get<SharksState>();
}

/// A ring at (point.x, 0, point.z) once the timer passes 50 ms (strictly; then %= 50)
void EmitWakeRing(const glm::vec3& point, float heading, int32_t frameMilliseconds)
{
	auto& state = SharksData();
	if (state.wakeTimer > 50)
	{
		state.wakeTimer %= 50;
		const WaterRing ring {.position = glm::vec3(point.x, 0.0f, point.z),
		                      .age = 0,
		                      .growth = 10.0f,
		                      .angle = heading,
		                      .aspect = 0.5f,
		                      .rate = 0.5f,
		                      .cell = 0x31,
		                      .argb = 0x90FFFFFFu};
		// the pool's in-use flag is set; the wind drift is left as the slot had it (WaterRing: not kept).
		// A full pool (1024) makes no ring, the timer is reset all the same.
		AddWaterRing(ring);
	}
	// += the frame's whole milliseconds
	state.wakeTimer += frameMilliseconds;
}
} // namespace

void ProcessSharksTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<Shark, const Transform>([](Shark& shark, const Transform& transform) {
		// the turn's start = Pos
		shark.turnStart = transform.position;
	});
	// (the WALK_PATH list is a later step of the turn: Game::GameLogicLoop)
}

void UpdateSharks(float turnFraction, float gameMilliseconds)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& island = Locator::terrainSystem::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const float f = turnFraction;
	// the frame's whole game ms, game_clock::FrameGameMs from Game
	const auto frameMilliseconds = static_cast<int32_t>(gameMilliseconds);
	std::vector<entt::entity> undrawn;
	registry.Each<Shark, Transform>([&](entt::entity entity, Shark& shark, Transform& transform) {
		const auto pos = transform.position;
		const auto prev = shark.turnStart;
		// the heading of the move (pos - prevPos) * 10 / 65536 with y = 0; the last one while it stands still
		if (pos.x != prev.x || pos.z != prev.z)
		{
			// the Y angle, stored as a float
			shark.heading = static_cast<float>(affine::GetYAngleOfXZ(glm::vec3(pos.x - prev.x, 0.0f, pos.z - prev.z)));
		}
		// between the start and the end of the turn, each end at GetAltitude + relY
		const float y0 = island.GetHeightAt(glm::vec2(prev.x, prev.z));
		const float y1 = island.GetHeightAt(glm::vec2(pos.x, pos.z));
		const glm::vec3 drawn((1.0f - f) * prev.x + f * pos.x, (1.0f - f) * y0 + f * y1, (1.0f - f) * prev.z + f * pos.z);
		// the object's position (P, heading, scale): RotateY(heading), the same turn as at the creation
		transform.rotation = affine::AngleY(shark.heading);
		if (auto* draw = registry.TryGet<DrawPosition>(entity); draw != nullptr)
		{
			draw->position = drawn;
			draw->rotation = transform.rotation;
			draw->shearX = 0.0f;
			draw->shearZ = 0.0f;
		}
		else
		{
			undrawn.push_back(entity);
		}

		// the wake: the translation of the bone matrix of EBone[0]'s bone x the EBone point. The matrices are the ones the
		// shark's own cut draw just filled: the clip's frames blended and each bone multiplied by its parent, the root by
		// the object's world matrix (position, heading and scale), so it is world space; no camera matrix is involved.
		// Here: model x this frame's pose (model space bones) x the point, the same product.
		const auto* mesh = registry.TryGet<const components::Mesh>(entity);
		if (mesh == nullptr || !meshes.Contains(mesh->id))
		{
			return;
		}
		const auto l3d = meshes.Handle(mesh->id);
		const auto& point = l3d->GetEBonePoint0();
		if (!point.has_value())
		{
			return;
		}
		const auto* animation = registry.TryGet<const components::SkeletalAnimation>(entity);
		const auto& bones = animation != nullptr && animation->pose.size() == l3d->GetBoneMatrices().size()
		                        ? animation->pose
		                        : l3d->GetBoneMatrices();
		const auto bone = point->first < bones.size() ? bones[point->first] : glm::mat4(1.0f);
		const auto model = affine::Model(drawn, transform.rotation, transform.scale);
		EmitWakeRing(glm::vec3(model * bone * glm::vec4(point->second, 1.0f)), shark.heading, frameMilliseconds);
	});
	for (const auto entity : undrawn)
	{
		const auto& transform = registry.Get<const Transform>(entity);
		const auto position = transform.position;
		const auto rotation = transform.rotation;
		auto& draw = registry.Assign<DrawPosition>(entity);
		draw.turnStart = position;
		draw.started = true;
		draw.position = position;
		draw.rotation = rotation;
	}
}

void RunSharkDebugHook()
{
	const char* test = std::getenv("OPENBLACK_TEST_SHARK");
	if (test == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	struct Walk
	{
		int32_t track;
		int32_t camera;
		int forward;
		float from;
		float to;
	};
	// Land 1, FollowUs (challenge.chl): Shark1 = CREATE(Whale, 5000, CONVERT_CAMERA_FOCUS(221)) walks WALK_PATH(Shark1,
	// forward, 21, 0.0, 1.0), Shark2 at CONVERT_CAMERA_FOCUS(230) walks track 20
	std::vector<Walk> walks;
	Walk one {0, 0, 1, 0.0f, 1.0f};
	if (std::sscanf(test, "%d,%d,%d,%f,%f", &one.track, &one.camera, &one.forward, &one.from, &one.to) >= 2)
	{
		walks.push_back(one);
	}
	else
	{
		walks.push_back({21, 221, 1, 0.0f, 1.0f});
		walks.push_back({20, 230, 1, 0.0f, 1.0f});
	}
	const auto& island = Locator::terrainSystem::value();
	for (const auto& walk : walks)
	{
		const auto camera = LoadCameraBin(walk.camera);
		if (!camera.has_value())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Shark test: no Cam{} in camera.edt", walk.camera);
			continue;
		}
		// CREATE(Marker, 0, focus), GET_POSITION of it, CREATE(Whale, 5000, that): at GetAltitude there, angle 0, scale 1
		const auto& focus = camera->focus;
		const glm::vec3 at(focus.x, island.GetHeightAt(glm::vec2(focus.x, focus.z)), focus.z);
		const auto entity = archetypes::SharkArchetype::Create(at, 0.0f, 1.0f);
		const bool started = StartMobileWalkPath(entity, walk.track, walk.forward != 0, walk.from, walk.to);
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Shark test: shark {} at Cam{} focus ({:.2f}, {:.2f}, {:.2f}), WALK_PATH track {} forward {} from {} "
		    "to {}{}",
		    static_cast<uint32_t>(entity), walk.camera, at.x, at.y, at.z, walk.track, walk.forward, walk.from, walk.to,
		    started ? "" : ": cannot load the track");
	}
}

} // namespace openblack::ecs
