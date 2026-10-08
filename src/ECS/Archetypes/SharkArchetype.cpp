/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SharkArchetype.h"

#include <glm/gtx/euler_angles.hpp>

#include "3D/AllMeshes.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Animations.h"
#include "ECS/Components/CutByPlane.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity SharkArchetype::Create(const glm::vec3& position, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	// Twice the scale; the matrix Scale(2) . Translate(x, GetAltitude + relY, z) . RotateY(angle).
	// (info.dat's mesh 370 MSH_O_LARGE_FISH_DEAD of mobile object 24 is not used.)
	registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale * 2.0f));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(MeshId::SharkBoned), static_cast<int8_t>(0),
	                      static_cast<int8_t>(1));
	// The swim clip (AnimPack 129); the time advances with the frame's game time, wrapped (a looping clip)
	auto& animation = registry.Assign<SkeletalAnimation>(entity);
	animation.clip = ecs::ClipId(static_cast<uint32_t>(AnimId::SharkBonedSwim));
	animation.clipIndex = static_cast<int32_t>(AnimId::SharkBonedSwim);
	animation.hasClip = true;
	// The position at the start of the turn; the heading is 0 after the creation
	registry.Assign<Shark>(entity, position, 0.0f);
	// The part under the water is drawn tinted 0xFF303070, the part above as it is; no shadow, no reflection, not
	// pickable
	registry.Assign<CutByPlane>(entity, 0xFF303070u, true);
	registry.SetDirty();
	return entity;
}
