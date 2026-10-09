/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WorshipSiteSystem.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/WorshipSites.h"
#include "GameWorshipSiteWorld.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace ws = openblack::ecs::worship_site;

namespace
{

/// A line for a log, when the log is there (it isn't in the tests)
void Log(spdlog::level::level_enum level, const char* name, std::string_view message)
{
	if (const auto logger = spdlog::get(name))
	{
		logger->log(level, message);
	}
}

[[nodiscard]] glm::vec2 Across(const glm::vec3& position)
{
	return {position.x, position.z};
}

} // namespace

WorshipSiteSystem::WorshipSiteSystem()
    : WorshipSiteSystem(std::make_unique<GameWorshipSiteWorld>())
{
}

WorshipSiteSystem::WorshipSiteSystem(std::unique_ptr<worship_site::WorldInterface> world)
    : _world(std::move(world))
{
}

WorshipSiteSystem::~WorshipSiteSystem() = default;

void WorshipSiteSystem::AddTemple(entt::entity temple, float facing, bool standing)
{
	_world->Entities().Assign<CitadelWorship>(temple, CitadelWorship {.facing = facing});
	// A temple made standing whole opens its sites at once; one still to be built opens them when it is built
	if (standing)
	{
		TempleBuilt(temple);
	}
}

void WorshipSiteSystem::TempleBuilt(entt::entity temple)
{
	_world->Entities().Get<CitadelWorship>(temple).standing = true;
	OpenSites(temple, 0.0f);
}

void WorshipSiteSystem::PersonJoinedTown(entt::entity town)
{
	if (_world->PopulationOf(town) == 1)
	{
		CheckAddSite(town);
	}
}

void WorshipSiteSystem::CheckAddSite(entt::entity town)
{
	auto& registry = _world->Entities();
	const auto& component = registry.Get<const Town>(town);
	if (!ws::MayHaveSite(_world->LandNumber(), component.cannotHaveWorshipSite, _world->PopulationOf(town)) ||
	    component.owner == PlayerNames::NEUTRAL)
	{
		return;
	}
	const auto temple = TempleOf(component.owner);
	if (temple == entt::null)
	{
		return;
	}
	if (const auto site = FindOrMakeForTown(temple, town); site != entt::null)
	{
		AddTownIfMissing(site, town);
	}
}

void WorshipSiteSystem::LandLaidOut()
{
	auto& registry = _world->Entities();
	std::vector<std::pair<PlayerNames, entt::entity>> temples;
	registry.Each<const Temple, const CitadelWorship>(
	    [&temples](entt::entity entity, const Temple& temple, const CitadelWorship& /*unused*/) {
		    temples.emplace_back(temple.owner, entity);
	    });
	for (const auto& [player, temple] : temples)
	{
		if (TempleOf(player) != temple)
		{
			continue;
		}
		for (const auto town : TownsOf(player))
		{
			if (registry.Get<const Town>(town).worshipSite != entt::null)
			{
				continue;
			}
			if (const auto site = FindOrMakeForTown(temple, town); site != entt::null)
			{
				AddTownIfMissing(site, town);
			}
		}
	}
}

entt::entity WorshipSiteSystem::MakeBuiltSite(PlayerNames player, Tribe tribe)
{
	const auto temple = TempleOf(player);
	if (temple == entt::null || !_world->Entities().Get<const CitadelWorship>(temple).standing)
	{
		return entt::null;
	}
	const auto site = FindOrMake(temple, tribe);
	if (site == entt::null)
	{
		return entt::null;
	}
	auto& registry = _world->Entities();
	const auto towns = TownsOf(player);
	const auto town = std::ranges::find_if(
	    towns, [&registry, tribe](entt::entity candidate) { return registry.Get<const Tribe>(candidate) == tribe; });
	if (town == towns.end())
	{
		// With no town of the tribe it is built at once, but not handed back
		BuildBy(site, 1.0f);
		return entt::null;
	}
	AddTownIfMissing(site, *town);
	// Only a site its town has been asked to build is built: one made here for the first time waits for its builders
	const auto& requests = registry.Get<const WorshipSite>(site).buildRequests;
	if (std::ranges::any_of(requests, [town](const auto& request) { return request.town == *town; }))
	{
		BuildBy(site, 1.0f);
	}
	return site;
}

void WorshipSiteSystem::SetCanHaveSites(entt::entity townOrTemple, bool can)
{
	auto& registry = _world->Entities();
	if (!registry.Valid(townOrTemple))
	{
		Log(spdlog::level::err, "scripting", "Worship sites can only be allowed or stopped for a town or temple");
		return;
	}
	if (auto* town = registry.TryGet<Town>(townOrTemple); town != nullptr)
	{
		town->cannotHaveWorshipSite = !can;
		if (!can)
		{
			return;
		}
		CheckAddSite(townOrTemple);
		// Allowing a town allows its player's temple too, and its tribe's site is made if it may be
		if (const auto temple = TempleOf(registry.Get<const Town>(townOrTemple).owner); temple != entt::null)
		{
			registry.Get<CitadelWorship>(temple).cannotMakeSites = false;
			FindOrMakeForTown(temple, townOrTemple);
		}
		return;
	}
	if (auto* worship = registry.TryGet<CitadelWorship>(townOrTemple); worship != nullptr)
	{
		worship->cannotMakeSites = !can;
		if (can)
		{
			OpenSites(townOrTemple, 0.0f);
		}
		return;
	}
	Log(spdlog::level::err, "scripting", "Worship sites can only be allowed or stopped for a town or temple");
}

void WorshipSiteSystem::UpdateTurn()
{
	_world->Entities().Each<const WorshipSite, Mesh>(
	    [this](const WorshipSite& site, Mesh& mesh) { mesh.id = _world->SiteMesh(site.temple); });
}

void WorshipSiteSystem::BuildBy(entt::entity site, float share)
{
	auto& registry = _world->Entities();
	if (IsBuilt(site))
	{
		return;
	}
	// Its altar goes up with it, but isn't seen until it stands whole
	auto& progress = registry.Get<BuildProgress>(site);
	progress.built = std::max(progress.built + share, 0.0f);
	if (progress.built >= 1.0f)
	{
		Built(site);
	}
}

bool WorshipSiteSystem::IsBuilt(entt::entity site) const
{
	const auto& registry = _world->Entities();
	// A site keeps how far it is built only while it goes up
	const auto* progress = registry.TryGet<const BuildProgress>(site);
	return registry.AllOf<WorshipSite>(site) && (progress == nullptr || progress->built >= 1.0f);
}

entt::entity WorshipSiteSystem::TempleOf(PlayerNames player) const
{
	// The first temple made for the player
	entt::entity found = entt::null;
	_world->Entities().Each<const Temple, const CitadelWorship>(
	    [player, &found](entt::entity entity, const Temple& temple, const CitadelWorship& /*unused*/) {
		    if (temple.owner == player && (found == entt::null || entity < found))
		    {
			    found = entity;
		    }
	    });
	return found;
}

void WorshipSiteSystem::OpenSites(entt::entity temple, float desireBoost)
{
	auto& registry = _world->Entities();
	for (const auto town : TownsOf(registry.Get<const Temple>(temple).owner))
	{
		const auto site = FindOrMakeForTown(temple, town);
		if (site == entt::null)
		{
			continue;
		}
		AddTownIfMissing(site, town);
		if (IsBuilt(site))
		{
			continue;
		}
		// The town is asked to build the site, if it isn't already, and wants to by the boost
		auto& requests = registry.Get<WorshipSite>(site).buildRequests;
		auto request =
		    std::ranges::find_if(requests, [town](const WorshipSite::BuildRequest& asked) { return asked.town == town; });
		if (request == requests.end())
		{
			requests.insert(requests.begin(), WorshipSite::BuildRequest {.town = town});
			request = requests.begin();
		}
		request->desireBoost = desireBoost;
	}
}

entt::entity WorshipSiteSystem::FindOrMakeForTown(entt::entity temple, entt::entity town)
{
	auto& registry = _world->Entities();
	if (registry.Get<const CitadelWorship>(temple).cannotMakeSites)
	{
		return entt::null;
	}
	const auto& component = registry.Get<const Town>(town);
	if (!ws::MayHaveSite(_world->LandNumber(), component.cannotHaveWorshipSite, _world->PopulationOf(town)))
	{
		return entt::null;
	}
	return FindOrMake(temple, registry.Get<const Tribe>(town));
}

entt::entity WorshipSiteSystem::FindOrMake(entt::entity temple, Tribe tribe)
{
	if (const auto site = FindForTribe(temple, tribe); site != entt::null)
	{
		return site;
	}
	return Make(temple, tribe);
}

entt::entity WorshipSiteSystem::FindForTribe(entt::entity temple, Tribe tribe) const
{
	const auto& registry = _world->Entities();
	for (const auto site : registry.Get<const CitadelWorship>(temple).sites)
	{
		if (site != entt::null && registry.Get<const WorshipSite>(site).tribe == tribe)
		{
			return site;
		}
	}
	return entt::null;
}

entt::entity WorshipSiteSystem::Make(entt::entity temple, Tribe tribe)
{
	auto& registry = _world->Entities();
	if (!registry.Get<const CitadelWorship>(temple).standing)
	{
		return entt::null;
	}
	const auto templePosition = registry.Get<const Transform>(temple).position;
	const auto templeOwner = registry.Get<const Temple>(temple).owner;

	// The place is judged against the town of the tribe nearest the temple, of any player; without one, the temple
	auto spot = Across(templePosition);
	auto nearest = std::numeric_limits<float>::max();
	std::vector<std::pair<uint32_t, entt::entity>> towns;
	registry.Each<const Town, const Tribe, const Transform>(
	    [&towns, tribe](entt::entity entity, const Town& town, const Tribe& townTribe, const Transform& /*unused*/) {
		    if (townTribe == tribe)
		    {
			    towns.emplace_back(town.id, entity);
		    }
	    });
	std::ranges::sort(towns);
	for (const auto& [id, town] : towns)
	{
		const auto at = Across(registry.Get<const Transform>(town).position);
		if (const auto distance = glm::distance(Across(templePosition), at); distance < nearest)
		{
			nearest = distance;
			spot = at;
		}
	}

	const auto placePoint = _world->SitePoint(ws::k_PlacePoint);
	if (!placePoint.has_value())
	{
		Log(spdlog::level::err, "game", "The worship site's model has no point to place it by");
		return entt::null;
	}
	auto& worship = registry.Get<CitadelWorship>(temple);
	std::array<bool, ws::k_Places> taken {};
	std::ranges::transform(worship.sites, taken.begin(), [](entt::entity site) { return site != entt::null; });
	const auto place = ws::NearestFreePlace(taken, Across(templePosition), *placePoint, spot);
	if (!place.has_value())
	{
		return entt::null;
	}

	// The site stands at the temple's own place, turned to face out from its place around it
	const auto facing = ws::PlaceFacing(worship.facing, *place);
	const auto site = registry.Create();
	const auto ground = glm::vec3(templePosition.x, _world->LandHeightAt(Across(templePosition)), templePosition.z);
	registry.Assign<Transform>(site, ground, glm::mat3(glm::eulerAngleY(-facing)), glm::vec3(1.0f));
	registry.Assign<Mesh>(site, _world->SiteMesh(temple), static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<BuildProgress>(site, 0.0f);
	registry.Assign<WorshipSite>(site, WorshipSite {
	                                       .temple = temple,
	                                       .player = templeOwner,
	                                       .tribe = tribe,
	                                       .place = static_cast<uint8_t>(*place),
	                                       .facing = facing,
	                                   });
	worship.sites.at(*place) = site;

	// Every town of the player of the tribe worships there
	for (const auto town : TownsOf(templeOwner))
	{
		if (registry.Get<const Tribe>(town) == tribe)
		{
			AddTown(site, town);
		}
	}

	// The tribe's altar stands at the head of the site, facing as it does
	const auto altarPoint = _world->SitePoint(ws::k_AltarPoint).value_or(glm::vec3(0.0f));
	const auto altarAt = ws::TurnedPoint(Across(templePosition), facing, altarPoint);
	const auto altar = registry.Create();
	registry.Assign<Transform>(altar, glm::vec3(altarAt.x, _world->LandHeightAt(altarAt), altarAt.y),
	                           glm::mat3(glm::eulerAngleY(-facing)), glm::vec3(1.0f));
	registry.Assign<WorshipAltar>(altar, site);
	registry.Get<WorshipSite>(site).altar = altar;
	if (const auto logger = spdlog::get("game"))
	{
		logger->debug("Worship site of tribe {} made for player {} in place {}", static_cast<int>(tribe),
		              static_cast<int>(templeOwner), *place);
	}
	return site;
}

void WorshipSiteSystem::AddTown(entt::entity site, entt::entity town)
{
	auto& registry = _world->Entities();
	registry.Get<Town>(town).worshipSite = site;
	auto& towns = registry.Get<WorshipSite>(site).towns;
	towns.insert(towns.begin(), town);
}

void WorshipSiteSystem::AddTownIfMissing(entt::entity site, entt::entity town)
{
	const auto& towns = _world->Entities().Get<const WorshipSite>(site).towns;
	if (std::ranges::find(towns, town) == towns.end())
	{
		AddTown(site, town);
	}
}

void WorshipSiteSystem::Built(entt::entity site)
{
	auto& registry = _world->Entities();
	auto& component = registry.Get<WorshipSite>(site);
	component.buildRequests.clear();
	// It stands whole, drawn as itself, and its altar is seen from now on
	registry.Remove<BuildProgress>(site);
	if (registry.Valid(component.altar) && !registry.AllOf<Mesh>(component.altar))
	{
		registry.Assign<Mesh>(component.altar, _world->AltarMesh(component.tribe), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
	}
}

std::vector<entt::entity> WorshipSiteSystem::TownsOf(PlayerNames player) const
{
	std::vector<std::pair<uint32_t, entt::entity>> owned;
	_world->Entities().Each<const Town>([&owned, player](entt::entity entity, const Town& town) {
		if (town.owner == player)
		{
			owned.emplace_back(town.gained, entity);
		}
	});
	std::ranges::sort(owned);
	std::vector<entt::entity> towns;
	towns.reserve(owned.size());
	std::ranges::transform(owned, std::back_inserter(towns), [](const auto& pair) { return pair.second; });
	return towns;
}
