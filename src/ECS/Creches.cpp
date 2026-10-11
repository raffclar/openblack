/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Creches.h"

#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/CrecheRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::creches
{
namespace
{
using ecs::components::Abode;
using ecs::components::Mesh;
using ecs::components::Town;
using ecs::components::Transform;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The town a building belongs to, none when its town is gone
entt::entity TownOfBuilding(entt::entity building)
{
	const auto& registry = Entities();
	const auto* abode = registry.Valid(building) ? registry.TryGet<const Abode>(building) : nullptr;
	if (abode == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

std::optional<AbodeType> KindOf(entt::entity building)
{
	const auto* info = ecs::world_objects::AbodeInfoOf(building);
	return info != nullptr ? std::optional(info->abodeType) : std::nullopt;
}

bool IsCreche(entt::entity building)
{
	return KindOf(building) == AbodeType::Creche;
}

/// The building's model, when it has one loaded
const graphics::L3DMesh* ModelOf(entt::entity building)
{
	const auto* mesh = Entities().TryGet<const Mesh>(building);
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return nullptr;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(mesh->id) ? meshes.Handle(mesh->id).operator->() : nullptr;
}
} // namespace

void Made(entt::entity building)
{
	if (IsCreche(building) && ecs::villager_home::IsFunctional(building))
	{
		StartsWorking(building);
	}
}

void StartsWorking(entt::entity building)
{
	if (!IsCreche(building))
	{
		return;
	}
	if (const auto town = TownOfBuilding(building); town != entt::null)
	{
		auto& own = Entities().Get<Town>(town).creche;
		if (own == entt::null || !Entities().Valid(own))
		{
			own = building;
		}
	}
}

void Lost(entt::entity building)
{
	auto& registry = Entities();
	const auto town = TownOfBuilding(building);
	if (town == entt::null)
	{
		return;
	}
	auto& data = registry.Get<Town>(town);
	if (!IsCreche(building))
	{
		// Something standing in as the town's crèche goes: the town is left with none
		if (data.creche == building)
		{
			data.creche = entt::null;
		}
		return;
	}
	std::vector<entt::entity> others;
	std::vector<creche::OtherBuilding> facts;
	for (const auto other : data.abodes)
	{
		if (other != building && registry.Valid(other))
		{
			const auto kind = KindOf(other);
			others.push_back(other);
			facts.push_back({.working = ecs::villager_home::IsFunctional(other),
			                 .takesOver = kind.has_value() && creche::TakesOverAsCreche(*kind)});
		}
	}
	const auto takesOver = creche::CrecheAfterLoss(data.creche == building, facts);
	data.creche = takesOver.has_value() ? others.at(*takesOver) : entt::null;
}

entt::entity CrecheOf(entt::entity town)
{
	const auto& registry = Entities();
	if (town == entt::null || !registry.Valid(town))
	{
		return entt::null;
	}
	const auto* data = registry.TryGet<const Town>(town);
	return data != nullptr && data->creche != entt::null && registry.Valid(data->creche) ? data->creche : entt::null;
}

uint32_t PathsOf(entt::entity building)
{
	const auto* model = ModelOf(building);
	return model != nullptr ? creche::PathsIn(model->GetExtraMetrics().size()) : 0;
}

std::optional<glm::vec2> PathPoint(entt::entity building, size_t point)
{
	const auto* model = ModelOf(building);
	if (model == nullptr || point >= model->GetExtraMetrics().size())
	{
		return std::nullopt;
	}
	// The point is the place of the marker in the model, put where the building stands
	const auto& transform = Entities().Get<const Transform>(building);
	const glm::vec3 local = model->GetExtraMetrics()[point][3];
	return glm::xz(transform.position + transform.rotation * (transform.scale * local));
}

} // namespace openblack::creches
