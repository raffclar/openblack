/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameWorshipSiteWorld.h"

#include <algorithm>
#include <optional>
#include <string>

#include <DanceFile.h>
#include <L3DFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorshipSites.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The worship site's model, laid out about the temple it stands at
constexpr auto k_SiteMesh = entt::hashed_string("temple/b_worship_l3d");
} // namespace

ecs::Registry& GameWorshipSiteWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

int32_t GameWorshipSiteWorld::LandNumber() const
{
	return Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
}

std::optional<glm::vec3> GameWorshipSiteWorld::SitePoint(uint32_t index) const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_SiteMesh.value()))
	{
		return std::nullopt;
	}
	const auto& points = meshes.Handle(k_SiteMesh.value())->GetExtraMetrics();
	if (index >= points.size())
	{
		return std::nullopt;
	}
	return glm::vec3(points.at(index)[3]);
}

entt::id_type GameWorshipSiteWorld::SiteMesh(entt::entity temple)
{
	// Every part of the site's model is drawn with the first skin of the temple's model, so the site takes on the
	// temple's look for its player's alignment as it changes
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* templeMesh = registry.Valid(temple) ? registry.TryGet<const Mesh>(temple) : nullptr;
	if (templeMesh == nullptr || !Locator::resources::has_value())
	{
		return k_SiteMesh.value();
	}
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	auto& files = resources.GetL3DFiles();
	if (!meshes.Contains(templeMesh->id) || !files.Contains(k_SiteMesh.value()))
	{
		return k_SiteMesh.value();
	}
	const auto& templeModel = *meshes.Handle(templeMesh->id);
	// Only a temple with its own model, made as its look changes, outlives the site's model made with its skin
	if (!templeModel.IsDynamic() || templeModel.GetSkinOrder().empty())
	{
		return k_SiteMesh.value();
	}
	const auto name = fmt::format("temple/worship/{}", templeMesh->id);
	const auto id = entt::hashed_string(name.c_str()).value();
	if (!meshes.Contains(id))
	{
		auto site = *files.Handle(k_SiteMesh.value());
		const auto skin = templeModel.GetSkinOrder().front();
		for (auto& primitive : site.EditPrimitiveHeaders())
		{
			primitive.material.skinID = skin;
		}
		meshes.Load(id, resources::L3DLoader::FromFileWithSkinsOfTag {}, name, site, templeModel);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Worship sites at temple model {} wear its skin", templeMesh->id);
	}
	return id;
}

entt::id_type GameWorshipSiteWorld::AltarMesh(Tribe tribe) const
{
	const auto& sites = Locator::infoConstants::value().worshipSite;
	const auto row = static_cast<size_t>(tribe);
	return resources::HashIdentifier(row < sites.size() ? sites.at(row).meshType : MeshId::Dummy);
}

float GameWorshipSiteWorld::LandHeightAt(glm::vec2 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

uint32_t GameWorshipSiteWorld::PopulationOf(entt::entity town) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	auto population = static_cast<uint32_t>(registry.Get<const Town>(town).homelessVillagers.size());
	registry.Each<const Villager>([town, &population](entt::entity /*unused*/, const Villager& villager) {
		if (villager.town == town)
		{
			++population;
		}
	});
	return population;
}

magic::WorshipBatteryRules GameWorshipSiteWorld::ChantRules(Tribe tribe, PlayerNames player) const
{
	const auto& sites = Locator::infoConstants::value().worshipSite;
	const auto row = static_cast<size_t>(tribe);
	// Whatever the site's tribe, its dancers chant with the player's Aztec power
	const float power =
	    Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetTribalPower(player, Tribe::AZTEC) : 1.0f;
	return row < sites.size() ? magic::WorshipBatteryRulesFor(sites.at(row), power) : magic::WorshipBatteryRules {};
}

bool GameWorshipSiteWorld::DanceStartsAutomatically(DanceInfo dance) const
{
	const auto& dances = Locator::infoConstants::value().dance;
	const auto row = static_cast<size_t>(dance);
	return row < dances.size() && dances.at(row).startsAutomatically != 0;
}

uint32_t GameWorshipSiteWorld::Turn() const
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

entt::entity GameWorshipSiteWorld::MakeFoodPot(glm::vec3 position, float yAngle)
{
	const auto pot = archetypes::PotArchetype::CreateEmpty(position, yAngle, PotInfo::StoragePitFoodPile);
	if (pot != entt::null)
	{
		Locator::entitiesRegistry::value().Get<Transform>(pot).scale = glm::vec3(worship_site::k_FoodPotScale);
	}
	return pot;
}

std::optional<uint32_t> GameWorshipSiteWorld::DanceLoops(DanceInfo dance)
{
	const auto& dances = Locator::infoConstants::value().dance;
	const auto row = static_cast<size_t>(dance);
	if (row >= dances.size() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	// The table names the file from the game's folder, without an extension
	const std::string name(dances.at(row).fileName.data());
	auto& files = Locator::resources::value().GetDanceFiles();
	try
	{
		if (!files.Contains(entt::hashed_string(name.c_str()).value()))
		{
			std::string path = name;
			std::ranges::replace(path, '\\', '/');
			files.Load(entt::hashed_string(name.c_str()), resources::DanceFileLoader::FromDiskTag {},
			           Locator::filesystem::value().FindPath(path));
		}
		return files.Handle(entt::hashed_string(name.c_str()))->loops;
	}
	catch (const std::exception& e)
	{
		if (const auto logger = spdlog::get("game"))
		{
			logger->error("Can't read the dance {}: {}", name, e.what());
		}
		return std::nullopt;
	}
}
