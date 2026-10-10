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

#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DanceRules.h"
#include "ECS/Registry.h"
#include "ECS/WorshipSites.h"
#include "GameWorshipSiteWorld.h"
#include "Magic/WorshipBattery.h"

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
	auto& registry = _world->Entities();
	registry.Each<const WorshipSite, Mesh>(
	    [this](const WorshipSite& site, Mesh& mesh) { mesh.id = _world->SiteMesh(site.temple); });
	// The sites' dances go on
	const auto turn = _world->Turn();
	registry.Each<const WorshipSite>([this, turn](entt::entity site, const WorshipSite& /*unused*/) {
		if (auto* dance = DanceOf(site); dance != nullptr)
		{
			dance_rules::ProcessTurn(*dance, _world->DanceStartsAutomatically(dance->type), turn);
		}
	});
}

void WorshipSiteSystem::ProcessChants()
{
	auto& registry = _world->Entities();
	// Every player in turn, the neutral player last, each site of their temple by its place round it
	for (size_t player = 0; player < static_cast<size_t>(PlayerNames::_COUNT); ++player)
	{
		const auto temple = TempleOf(static_cast<PlayerNames>(player));
		if (temple == entt::null)
		{
			continue;
		}
		for (const auto site : registry.Get<const CitadelWorship>(temple).sites)
		{
			if (site == entt::null || !registry.Valid(site))
			{
				continue;
			}
			const auto& component = registry.Get<const WorshipSite>(site);
			auto& chants = registry.Get<WorshipChants>(site);
			// Spell icons are not made at the sites yet, so none charges
			const float drawn =
			    magic::ProcessWorshipTurn(chants, _world->ChantRules(component.tribe, component.player), Dancers(site), {});
			CountChantsUsed(site, drawn);
			// The dance goes as hard as its dancers chant, stopping when they don't
			if (auto* dance = DanceOf(site); dance != nullptr)
			{
				dance_rules::SetWorshipSpeed(*dance, chants.danceIntensity);
			}
		}
	}
}

void WorshipSiteSystem::SetDancers(entt::entity site, uint32_t dancers)
{
	if (auto* dance = DanceOf(site); dance != nullptr)
	{
		dance->dancers = dancers;
	}
}

uint32_t WorshipSiteSystem::Dancers(entt::entity site) const
{
	// Those dancing its dance chant; a site without a dance has none
	const auto* dance = DanceOf(site);
	return dance != nullptr ? dance->dancers : 0;
}

Dance* WorshipSiteSystem::DanceOf(entt::entity site) const
{
	auto& registry = _world->Entities();
	const auto dance = registry.Get<const WorshipSite>(site).dance;
	return registry.Valid(dance) ? registry.TryGet<Dance>(dance) : nullptr;
}

float WorshipSiteSystem::UseChants(entt::entity site, float amount)
{
	const float given = magic::UseWorshipChants(_world->Entities().Get<WorshipChants>(site), amount);
	CountChantsUsed(site, given);
	return given;
}

float WorshipSiteSystem::UseCreateChants(entt::entity site, float amount)
{
	return _world->Entities().Get<const WorshipChants>(site).infinite ? amount : UseChants(site, amount);
}

float WorshipSiteSystem::MaintainSpell(entt::entity site, float amount)
{
	return _world->Entities().Get<const WorshipChants>(site).freeMaintenance ? amount : UseChants(site, amount);
}

void WorshipSiteSystem::ReturnChants(entt::entity site, float amount)
{
	// The battery takes it all, however full
	_world->Entities().Get<WorshipChants>(site).battery += amount;
}

float WorshipSiteSystem::ChantsAvailable(entt::entity site) const
{
	return magic::WorshipAvailable(_world->Entities().Get<const WorshipChants>(site));
}

float WorshipSiteSystem::TakeChantsForVirtualInfluence(PlayerNames player, uint32_t interfaces)
{
	auto& registry = _world->Entities();
	const auto temple = TempleOf(player);
	if (temple == entt::null)
	{
		return 0.0f;
	}
	float asked = 0.0f;
	for (const auto site : registry.Get<const CitadelWorship>(temple).sites)
	{
		if (site == entt::null || !registry.Valid(site))
		{
			continue;
		}
		const auto& component = registry.Get<const WorshipSite>(site);
		const auto& chants = registry.Get<const WorshipChants>(site);
		const float take = magic::WorshipAvailableForVirtualInfluence(
		    chants, _world->ChantRules(component.tribe, component.player), Dancers(site), interfaces);
		UseChants(site, take);
		// What was asked is counted, not what was given
		asked += take;
	}
	return asked;
}

void WorshipSiteSystem::CountChantsUsed(entt::entity site, float amount)
{
	auto& registry = _world->Entities();
	const auto owner = registry.Get<const WorshipSite>(site).player;
	registry.Each<Player>([owner, amount](Player& player) {
		if (player.name == owner)
		{
			player.totalChantsUsed += amount;
		}
	});
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
	registry.Assign<WorshipChants>(site);
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
	Init(site);
	if (const auto logger = spdlog::get("game"))
	{
		logger->debug("Worship site of tribe {} made for player {} in place {}", static_cast<int>(tribe),
		              static_cast<int>(templeOwner), *place);
	}
	return site;
}

void WorshipSiteSystem::Init(entt::entity site)
{
	auto& registry = _world->Entities();
	auto& component = registry.Get<WorshipSite>(site);
	const auto at = Across(registry.Get<const Transform>(site).position);

	// Its dance, its place's own, is centred on the altar's point; made at a quarter speed and stopped, it is set
	// going at half speed at once
	const auto dancePoint = _world->SitePoint(ws::k_DancePoint).value_or(glm::vec3(0.0f));
	const auto danceAt = ws::TurnedPoint(at, component.facing, dancePoint);
	const auto dance = registry.Create();
	registry.Assign<Transform>(dance, glm::vec3(danceAt.x, _world->LandHeightAt(danceAt), danceAt.y), glm::mat3(1.0f),
	                           glm::vec3(1.0f));
	auto& danced = registry.Assign<Dance>(
	    dance,
	    Dance {.type = static_cast<DanceInfo>(static_cast<int>(ws::k_FirstPlaceDance) + component.place), .owner = site});
	dance_rules::SetSpeed(danced, ws::k_DanceMadeSpeed);
	dance_rules::SetWorshipSpeed(danced, ws::k_SiteDanceStartSpeed);
	component.dance = dance;

	// Then its food pot, empty, beside the gate, turned a little further than the site
	const auto potAt = ws::TurnedPoint(at, component.facing, ws::k_FoodPotPoint);
	component.foodPot =
	    _world->MakeFoodPot(glm::vec3(potAt.x, _world->LandHeightAt(potAt), potAt.y), component.facing + ws::k_FoodPotTurn);
}

void WorshipSiteSystem::AddTown(entt::entity site, entt::entity town)
{
	auto& registry = _world->Entities();
	registry.Get<Town>(town).worshipSite = site;
	// A town with a storage pit, built or planned, is linked to the site by a footpath from its pit to the site's gate.
	// The site keeps the link; finding the footpath's way between them isn't done yet, so it holds none.
	const auto& abodes = registry.Get<const Town>(town).abodes;
	const bool hasPit = std::ranges::any_of(
	    abodes, [&registry](entt::entity abode) { return registry.Valid(abode) && registry.AllOf<StoragePit>(abode); });
	if (hasPit && !registry.AllOf<FootpathLink>(site))
	{
		const auto gate =
		    ws::TurnedPoint(Across(registry.Get<const Transform>(site).position), registry.Get<const WorshipSite>(site).facing,
		                    _world->SitePoint(ws::k_PlacePoint).value_or(glm::vec3(0.0f)));
		registry.Assign<FootpathLink>(site, glm::vec3(gate.x, _world->LandHeightAt(gate), gate.y),
		                              std::vector<Footpath::Id> {});
	}
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
