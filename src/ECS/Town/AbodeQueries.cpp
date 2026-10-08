/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeQueries.h"

#include <glm/gtc/constants.hpp>

#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::abode_queries
{
using namespace components;

namespace
{
/// The abode's GAbodeInfo. (inferred) openblack's abode keeps no info pointer: the record of its number
/// and mesh, else the first of that number (as FireObjectTraits.cpp's AbodeInfoOf)
const GAbodeInfo* InfoOf(entt::entity abode)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* a = registry.TryGet<const Abode>(abode);
	if (a == nullptr)
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const GAbodeInfo* first = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != a->type)
		{
			continue;
		}
		if (mesh != nullptr && resources::HashIdentifier(info.meshId) == mesh->id)
		{
			return &info;
		}
		if (first == nullptr)
		{
			first = &info;
		}
	}
	return first;
}
} // namespace

bool IsAvailable(entt::entity abode)
{
	return ecs::IsAvailable(abode);
}

bool IsBuilt(entt::entity abode)
{
	return abodes::IsBuilt(abode);
}

bool IsFunctional(entt::entity abode)
{
	// IsAvailable && IsBuilt && thresholdForStopBeingFunctional < the life
	if (!IsAvailable(abode) || !IsBuilt(abode))
	{
		return false;
	}
	const auto* info = InfoOf(abode);
	if (info == nullptr || !(info->thresholdForStopBeingFunctional < life::LifeOf(abode)))
	{
		return false;
	}
	// and IsBuilt again
	return IsBuilt(abode);
}

glm::ivec2 GetArrivePos(entt::entity abode)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(abode);
	if (transform == nullptr)
	{
		return glm::ivec2(0);
	}
	const auto position = town_queries::PosOf(abode);
	// A door point, then x != 0 && z != 0
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return position;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return position;
	}
	const auto& door = meshes.Handle(mesh->id)->GetDoorPos();
	if (!door.has_value())
	{
		return position;
	}
	// (approximate) the mesh point through the object's matrix (rotation x scale, as the chimney's)
	const glm::vec3 world = transform->position + transform->rotation * (*door * transform->scale);
	// x and z x 6553.6, truncated
	const glm::ivec2 result(map_coords::ToFixed(world.x), map_coords::ToFixed(world.z));
	if (result.x == 0 || result.y == 0)
	{
		return position;
	}
	return result;
}

glm::ivec2 GetPosOutside(entt::entity abode, float p1, float p2, float p3)
{
	// The door
	const auto door = GetArrivePos(abode);
	// GameFloatRand(2 pi / p1) - 2 pi / (p1 + p1)
	const float spread = game_random::GameFloatRand(glm::two_pi<float>() / p1) - glm::two_pi<float>() / (p1 + p1);
	// + Get3DAngleFromXZ(pos, door)
	const float angle = town_queries::Get3DAngleFromXZ(town_queries::PosOf(abode), door) + spread;
	// GameFloatRand(p3) + p2
	const float distance = game_random::GameFloatRand(p3) + p2;
	// door + GetPosFromAngle(angle, distance)
	return door + town_queries::GetPosFromAngle(angle, distance);
}
} // namespace openblack::ecs::abode_queries
