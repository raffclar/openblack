/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSkeletons.h"

#include "3D/AllMeshes.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Villager.h"
#include "ECS/GraveyardRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerChecks.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

namespace openblack::villager_skeleton
{
namespace
{
using ecs::components::DetailMeshes;
using ecs::components::Mesh;
using ecs::components::Villager;
using ecs::components::WalkingSkeleton;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

std::array<entt::id_type, 3> ModelsFor(entt::entity villager, bool child)
{
	const auto* person = Entities().TryGet<const Villager>(villager);
	std::array<MeshId, 3> own {MeshId::Dummy, MeshId::Dummy, MeshId::Dummy};
	if (person != nullptr && Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value().villager.at(
		    static_cast<size_t>(GVillagerInfo::Find(person->tribe, person->number)));
		own = child ? std::array {info.childMeshHigh, info.childMeshMedium, info.childMeshLow}
		            : std::array {info.highDetail, info.stdDetail, info.lowDetail};
	}
	const auto worn = graveyard::ModelsWorn(Is(villager), MeshId::PersonSkeletonMale, own);
	return {resources::HashIdentifier(worn[0]), resources::HashIdentifier(worn[1]), resources::HashIdentifier(worn[2])};
}

void Set(entt::entity villager, bool skeleton)
{
	auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr)
	{
		return;
	}
	if (skeleton)
	{
		registry.AssignOrReplace<WalkingSkeleton>(villager);
	}
	else
	{
		registry.Remove<WalkingSkeleton>(villager);
	}
	const auto models = ModelsFor(villager, person->lifeStage == Villager::LifeStage::Child);
	if (auto* mesh = registry.TryGet<Mesh>(villager); mesh != nullptr)
	{
		mesh->id = models[0];
	}
	if (auto* detail = registry.TryGet<DetailMeshes>(villager); detail != nullptr)
	{
		detail->meshes = models;
	}
	registry.SetDirty();
}

void SetByScript(entt::entity villager, bool skeleton)
{
	if (!Entities().AllOf<Villager>(villager))
	{
		return;
	}
	Set(villager, skeleton);
	if (Locator::infoConstants::has_value())
	{
		ecs::villager_checks::GrowForAge(villager);
	}
}

bool Is(entt::entity villager)
{
	const auto& registry = Entities();
	return registry.Valid(villager) && registry.AllOf<WalkingSkeleton>(villager);
}

} // namespace openblack::villager_skeleton
