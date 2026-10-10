/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HighDetailSystem.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::high_detail_rules;

namespace
{
/// The ordinary models that have a detailed one
constexpr std::array k_ModelsWithDetail = {MeshId::PersonNorseMaleA1, MeshId::PersonNorseFemaleA1, MeshId::PersonBoyWhite1,
                                           MeshId::PersonAnimalTrainer};

/// The detailed model's id in the mesh cache, loaded from the misc folder the first time it is worn
std::optional<entt::id_type> LoadDetailedModel(std::string_view file)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	const auto name = "misc/" + std::string(file);
	const auto id = resources::HashIdentifier(name);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(id))
	{
		return id;
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Misc>() / file;
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "High detail model {} not found", path.generic_string());
		return std::nullopt;
	}
	meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
	return id;
}
} // namespace

void HighDetailSystem::Make(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<HighDetail>(thing))
	{
		return;
	}
	auto& detail = registry.Assign<HighDetail>(thing);
	auto* mesh = registry.TryGet<Mesh>(thing);
	if (mesh == nullptr)
	{
		return;
	}
	// Known by the model it wears close up
	auto* detailMeshes = registry.TryGet<DetailMeshes>(thing);
	const auto worn = detailMeshes != nullptr ? detailMeshes->meshes[0] : mesh->id;
	const auto model =
	    std::ranges::find_if(k_ModelsWithDetail, [worn](MeshId id) { return resources::HashIdentifier(id) == worn; });
	if (model == k_ModelsWithDetail.end())
	{
		return;
	}
	const auto detailed = rules::DetailedModelFor(*model);
	const auto id = LoadDetailedModel(detailed->file);
	if (!id.has_value())
	{
		return;
	}
	detail.usualModel = mesh->id;
	detail.face = detailed->face;
	mesh->id = *id;
	// It wears the detailed model at every distance
	if (detailMeshes != nullptr)
	{
		detail.usualDetailModels = detailMeshes->meshes;
		detailMeshes->meshes = {*id, *id, *id};
	}
	registry.SetDirty();
}

void HighDetailSystem::Release(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* detail = registry.TryGet<const HighDetail>(thing);
	if (detail == nullptr)
	{
		return;
	}
	if (auto* mesh = registry.TryGet<Mesh>(thing); mesh != nullptr && detail->usualModel.has_value())
	{
		mesh->id = *detail->usualModel;
	}
	if (auto* detailMeshes = registry.TryGet<DetailMeshes>(thing);
	    detailMeshes != nullptr && detail->usualDetailModels.has_value())
	{
		detailMeshes->meshes = *detail->usualDetailModels;
	}
	registry.Remove<HighDetail>(thing);
	registry.SetDirty();
}

bool HighDetailSystem::Order(entt::entity thing, rules::ThingSpecial special, bool on)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* detail = registry.TryGet<HighDetail>(thing);
	if (detail == nullptr)
	{
		return false;
	}
	auto& living = Locator::livingActionSystem::value();
	const auto result = rules::ApplyThingSpecial(special, on, detail->orders, living.VillagerYAngle(thing).value_or(0.0f));
	detail->orders = result.orders;
	if (result.yAngle.has_value())
	{
		living.VillagerSetYAngle(thing, *result.yAngle);
	}
	return true;
}

void HighDetailSystem::Update()
{
	const auto& director = Locator::cinematicDirectorSystem::value();
	if (rules::KeepsHighDetail(director.IsWideScreenOn(), director.GetWideScreenOwner()))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> drawn;
	registry.Each<const HighDetail>([&drawn](entt::entity entity, const HighDetail&) { drawn.push_back(entity); });
	for (const auto entity : drawn)
	{
		Release(entity);
	}
}
