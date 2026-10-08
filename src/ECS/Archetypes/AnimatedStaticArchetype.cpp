/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimatedStaticArchetype.h"

#include <cstddef>

#include <ECS/Components/Feature.h>
#include <glm/gtx/euler_angles.hpp>

#include "3D/AllMeshes.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Animations.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

// The default clip sits where the original reads it in the info (the info.dat record starts 16 bytes into it)
static_assert(offsetof(GAnimatedStaticInfo, defaultAnim) == 0x128 - 0x10);

namespace
{
/// A Norse Gate (mesh 212) gets two lamp objects with MSH_O_TOWNLIGHT at (-15, 30, 0) and (+15, 30, 0) in the gate's
/// matrix (rows with the scale, plus the translation), with angle 0 and scale 1, each with a lantern light. They are
/// not map objects. (The gate by value: creating the lamps can move the Transform storage.)
void CreateNorseGateLamps(const Transform gate)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto mesh = resources::HashIdentifier(MeshId::ObjectTownLight);
	for (const float side : {-15.0f, 15.0f})
	{
		const glm::vec3 position =
		    gate.position + side * gate.rotation[0] * gate.scale.x + 30.0f * gate.rotation[1] * gate.scale.y;
		const auto lamp = registry.Create();
		registry.Assign<Transform>(lamp, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(lamp, mesh, static_cast<int8_t>(0), static_cast<int8_t>(1));
		registry.Assign<LanternLight>(lamp, static_cast<uint8_t>(0));
	}
}
} // namespace

entt::entity AnimatedStaticArchetype::Create(const glm::vec3& position, AnimatedStaticInfo type, float yAngleRadians,
                                             float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().animatedStatic.at(static_cast<size_t>(type));

	// The exact same as Feature but info is different and type is a different enum
	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);
	// const auto& feature = registry.Assign<Feature>(entity, type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));

	registry.Assign<AnimatedStatic>(entity, type);

	// the object plays the info's clip (Norse Gate 191, Gate Stone Plinth 195, Piper Cave Entrance 189). Its draw moves
	// its time forwards or backwards with the open state, clamped to the clip: closed is t = 0, and nothing opens a gate
	// yet.
	const auto clip = static_cast<int32_t>(info.defaultAnim);
	if (clip >= 0)
	{
		auto& animation = registry.Assign<SkeletalAnimation>(entity);
		animation.clip = ecs::ClipId(static_cast<uint32_t>(clip));
		animation.clipIndex = clip;
		animation.hasClip = true;
		animation.time = 0.0f;
		animation.speed = 0.0f;
	}

	if (info.meshId == MeshId::BuildingNorseGate)
	{
		CreateNorseGateLamps(transform);
	}
	// InsertMapObject; the gate's lamps are not map objects
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
