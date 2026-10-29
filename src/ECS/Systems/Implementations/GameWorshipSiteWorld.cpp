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
#include "ECS/Archetypes/Utils.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
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

std::optional<glm::vec3> GameWorshipSiteWorld::SiteDoor() const
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
	return meshes.Handle(k_SiteMesh.value())->GetDoorPos();
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

std::optional<std::pair<glm::vec2, float>> GameWorshipSiteWorld::FixedCircleOf(entt::id_type model,
                                                                               const components::Transform& placed) const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	return ecs::archetypes::GetFixedObstacleBoundingCircle(model, placed);
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

entt::entity GameWorshipSiteWorld::MakeDance(DanceInfo dance, glm::vec3 position, entt::entity site)
{
	return Locator::danceSystem::has_value() ? Locator::danceSystem::value().Create(dance, position, site, 0, false, entt::null)
	                                         : entt::null;
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
		// The pot takes its size after it is made. If the size changes, the pot comes out of the map's cells and goes
		// in again.
		auto& transform = Locator::entitiesRegistry::value().Get<Transform>(pot);
		const bool resized = transform.scale.x != worship_site::k_FoodPotScale;
		transform.scale = glm::vec3(worship_site::k_FoodPotScale);
		if (resized)
		{
			ecs::thing_map_place::Reinsert(pot);
		}
	}
	return pot;
}

void GameWorshipSiteWorld::PowerUpArtefacts(entt::entity site, uint32_t dancers)
{
	if (Locator::artefactSystem::has_value())
	{
		Locator::artefactSystem::value().PowerUpSite(site, dancers);
	}
}

void GameWorshipSiteWorld::OpenBuildingSite(entt::entity site, entt::entity town, float desire)
{
	construction::OpenWorshipSite(site, town, desire);
}

void GameWorshipSiteWorld::CloseBuildingSite(entt::entity site)
{
	construction::CloseSite(site);
}

void GameWorshipSiteWorld::CloseTownsBuildingSite(entt::entity site, entt::entity town)
{
	construction::CloseTownsSite(site, town);
}

void GameWorshipSiteWorld::RemoveDance(entt::entity dance)
{
	if (Locator::danceSystem::has_value())
	{
		Locator::danceSystem::value().Destroy(dance);
	}
}

void GameWorshipSiteWorld::RemoveSpellIcons(entt::entity site)
{
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().SiteRemoved(site);
	}
}

void GameWorshipSiteWorld::ArtefactsLeaveSite(entt::entity site)
{
	Locator::entitiesRegistry::value().Each<TownArtefact>([site](entt::entity /*unused*/, TownArtefact& artefact) {
		if (artefact.site == site)
		{
			artefact.inSite = false;
		}
	});
}

uint32_t GameWorshipSiteWorld::StartMotes()
{
	// The mana path's effect, at the middle of the world
	return Locator::particleSystem::has_value()
	           ? Locator::particleSystem::value().StartOwned(ParticleType::ManaPath, glm::vec3(0.0f), 1.0f)
	           : 0;
}

void GameWorshipSiteWorld::AddMote(uint32_t effect, glm::vec3 from, glm::vec3 to, uint32_t rgb)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	if (auto* running = Locator::particleSystem::value().Find(effect); running != nullptr)
	{
		running->AddManaPathSpark({.from = from, .to = to, .rgb = rgb & 0xFFFFFFu});
	}
}

bool GameWorshipSiteWorld::StepMotes(uint32_t effect)
{
	// The site steps it with no hand, at full strength
	return Locator::particleSystem::has_value() &&
	       Locator::particleSystem::value().ProcessForSpell(
	           effect, particles::ProcessInfo {}, std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count());
}

void GameWorshipSiteWorld::RemoveMotes(uint32_t effect)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().Delete(effect);
	}
}

entt::entity GameWorshipSiteWorld::MakeNeedFlag(entt::entity site, uint8_t need)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& row = Locator::infoConstants::value().showNeeds.at(need);
	const auto flag = registry.Create();
	registry.Assign<WorshipSiteFlag>(flag, WorshipSiteFlag {.site = site, .need = need});
	registry.Assign<Mesh>(flag, resources::HashIdentifier(row.mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<Transform>(flag, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	return flag;
}

void GameWorshipSiteWorld::Remove(entt::entity object)
{
	world_objects::Remove(object);
}
