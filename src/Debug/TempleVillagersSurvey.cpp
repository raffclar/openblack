/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleVillagersSurvey.h"

#include <algorithm>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat4x4.hpp>

#include "3D/TempleInteriorInterface.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "Locator.h"
#include "Temple/TemplePeople.h"

using namespace openblack;
using namespace openblack::temple_villagers;

namespace
{
namespace components = ecs::components;
using WorshipPlace = ecs::systems::WorshipSiteSystemInterface::SitePlace;

constexpr std::array<WorshipPlace, static_cast<size_t>(SitePlace::_Count)> k_WorshipPlaces {
    WorshipPlace::Gate, WorshipPlace::Rest, WorshipPlace::Altar, WorshipPlace::Door, WorshipPlace::Centre};

/// The walk a villager is on, from its move state
std::optional<Walk> WalkOf(const ecs::Registry& registry, entt::entity villager)
{
	const auto* hug = registry.TryGet<const components::WallHug>(villager);
	if (hug == nullptr)
	{
		return std::nullopt;
	}
	Walk walk {.goal = hug->goal, .speed = hug->speed};
	const auto tagged = [&]<typename Tag>(std::string_view name) {
		if (const auto* tag = registry.TryGet<const Tag>(villager); tag != nullptr && walk.moveState.empty())
		{
			walk.moveState = name;
			walk.stepGoal = tag->stepGoal;
		}
	};
	tagged.template operator()<components::MoveStateLinearTag>("linear");
	tagged.template operator()<components::MoveStateOrbitTag>("orbit");
	tagged.template operator()<components::MoveStateExitCircleTag>("exit circle");
	tagged.template operator()<components::MoveStateStepThroughTag>("step through");
	tagged.template operator()<components::MoveStateFinalStepTag>("final step");
	tagged.template operator()<components::MoveStateArrivedTag>("arrived");
	if (const auto* circle = registry.TryGet<const components::WallHugObjectReference>(villager); circle != nullptr)
	{
		walk.circleCentre = circle->centre;
		walk.circleRadius = circle->radius;
	}
	return walk;
}

void SurveySites(const ecs::Registry& registry, const components::CitadelWorship& worship, Survey& survey)
{
	const auto* sites = Locator::worshipSiteSystem::has_value() ? &Locator::worshipSiteSystem::value() : nullptr;
	for (const auto site : worship.sites)
	{
		const auto* component =
		    site != entt::null && registry.Valid(site) ? registry.TryGet<const components::WorshipSite>(site) : nullptr;
		if (component == nullptr)
		{
			continue;
		}
		Site entry {
		    .site = site,
		    .tribe = component->tribe,
		    .place = component->place,
		    .worshippers = component->worshippers,
		    .towns = component->towns,
		};
		if (sites != nullptr)
		{
			entry.built = sites->IsBuilt(site);
			entry.dancers = sites->Dancers(site);
			entry.foodInPot = sites->FoodInPot(site);
			for (size_t i = 0; i < k_WorshipPlaces.size(); ++i)
			{
				entry.places.at(i) = sites->PlaceAt(site, k_WorshipPlaces.at(i));
			}
		}
		survey.sites.push_back(std::move(entry));
	}
}

void SurveyTowns(const ecs::Registry& registry, PlayerNames player, Survey& survey)
{
	registry.Each<const components::Town>([&](entt::entity town, const components::Town& component) {
		if (component.owner != player)
		{
			return;
		}
		// Only the towns that worship at this temple
		const auto site = std::ranges::find(survey.sites, component.worshipSite, &Site::site);
		if (site == survey.sites.end())
		{
			return;
		}
		const auto headcount = ecs::villager_worship::HeadcountOf(town);
		Town entry {
		    .town = town,
		    .id = component.id,
		    .site = component.worshipSite,
		    .share = component.worshipShare,
		    .population = headcount.population,
		    .worshipping = headcount.worshipping,
		    .onTheWay = headcount.onTheWay,
		    .needed = ecs::villager_worship::WorshippersNeeded(town, true, true).count,
		};
		for (const auto abode : component.abodes)
		{
			if (const auto* placed = registry.Valid(abode) ? registry.TryGet<const components::Transform>(abode) : nullptr)
			{
				entry.home = placed->position;
				break;
			}
		}
		if (!entry.home.has_value() && component.congregationPos.has_value())
		{
			entry.home = glm::vec3(component.congregationPos->x, survey.templePosition.y, component.congregationPos->y);
		}
		survey.towns.push_back(entry);
	});
}

void SurveyVillagers(const ecs::Registry& registry, float radius, Survey& survey)
{
	const auto* actions = Locator::livingActionSystem::has_value() ? &Locator::livingActionSystem::value() : nullptr;
	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const auto atTemple = [&survey](entt::entity site) {
		return std::ranges::find(survey.sites, site, &Site::site) != survey.sites.end();
	};
	registry.Each<const components::Villager, const components::Transform>(
	    [&](entt::entity villager, const components::Villager& person, const components::Transform& transform) {
		    const auto site = ecs::villager_worship::WorshipSiteOf(villager);
		    const auto* worshipper = registry.TryGet<const components::Worshipper>(villager);
		    const bool ours = site != entt::null && atTemple(site);
		    const bool counted = ours && worshipper != nullptr && worshipper->atWorship;
		    const bool onTheWay = ours && worshipper != nullptr && worshipper->onTheWay;
		    const float distance = glm::distance(glm::xz(transform.position), glm::xz(survey.templePosition));
		    const auto standing = StandingOf(counted, onTheWay, distance, radius);
		    if (!standing.has_value())
		    {
			    return;
		    }
		    Villager entry {
		        .villager = villager,
		        .standing = *standing,
		        .tribe = person.tribe,
		        .number = person.number,
		        .child = person.lifeStage == components::Villager::LifeStage::Child,
		        .male = person.sex == components::Villager::Sex::MALE,
		        .age = ecs::villager_age::AgeOf(turn, person.birthTurn),
		        .life = person.life,
		        .town = person.town,
		        .site = site,
		        .position = transform.position,
		        .distance = distance,
		        .walk = WalkOf(registry, villager),
		    };
		    if (const auto* action = registry.TryGet<const components::LivingAction>(villager);
		        action != nullptr && actions != nullptr)
		    {
			    entry.top = actions->VillagerGetState(*action, components::LivingAction::Index::Top);
			    entry.final = actions->VillagerGetFinalState(*action);
			    entry.turnsInState = action->turnsSinceStateChange;
		    }
		    survey.villagers.push_back(entry);
	    });
	std::ranges::sort(survey.villagers, [](const Villager& a, const Villager& b) {
		return ListedBefore(a.standing, a.distance, a.villager, b.standing, b.distance, b.villager);
	});
}

void SurveyInside(bool floorEdges, Survey& survey)
{
	if (!Locator::temple::has_value())
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	survey.inside.active = temple.Active();
	const auto* people = temple.GetWalkingPeople();
	if (!survey.inside.active || people == nullptr)
	{
		return;
	}
	const auto placement = temple.GetPlacement();
	const auto toWorld = [&placement](const glm::vec3& point) { return glm::vec3(placement * glm::vec4(point, 1.0f)); };
	for (const auto& visitor : people->GetVisitors())
	{
		const auto& walker = visitor.walker;
		survey.inside.walkers.push_back({
		    .villager = visitor.villager,
		    .position = toWorld(walker.position),
		    .target = toWorld(walker.target),
		    .walking = walker.gait == temple_people::Gait::Walking,
		    .timer = walker.timer,
		    .pause = walker.pause,
		});
	}
	if (!floorEdges)
	{
		return;
	}
	const auto& mesh = people->GetWalkMesh();
	for (const auto& triangle : mesh.triangles)
	{
		for (size_t k = 0; k < triangle.corners.size(); ++k)
		{
			// Edge k faces corner k. Each edge shared by two triangles is taken once, the way its lower corner leads.
			const auto beyond = triangle.beyond.at(k);
			const auto from = triangle.corners.at((k + 1) % 3);
			const auto to = triangle.corners.at((k + 2) % 3);
			if (beyond >= 0 && from > to)
			{
				continue;
			}
			survey.inside.floorEdges.push_back(toWorld(mesh.corners.at(from)));
			survey.inside.floorEdges.push_back(toWorld(mesh.corners.at(to)));
		}
	}
}
} // namespace

std::string_view temple_villagers::Name(Standing standing)
{
	switch (standing)
	{
	case Standing::Worshipping:
		return "worshipping";
	case Standing::OnTheWay:
		return "on the way";
	case Standing::Nearby:
		return "nearby";
	}
	return "?";
}

std::string_view temple_villagers::Name(SitePlace place)
{
	constexpr std::array<std::string_view, static_cast<size_t>(SitePlace::_Count)> k_Names {"gate", "rest", "altar", "door",
	                                                                                        "centre"};
	return place < SitePlace::_Count ? k_Names.at(static_cast<size_t>(place)) : "?";
}

std::optional<Standing> temple_villagers::StandingOf(bool countedAtSite, bool onTheWay, float distance, float radius)
{
	if (countedAtSite)
	{
		return Standing::Worshipping;
	}
	if (onTheWay)
	{
		return Standing::OnTheWay;
	}
	if (distance <= radius)
	{
		return Standing::Nearby;
	}
	return std::nullopt;
}

bool temple_villagers::ListedBefore(Standing a, float distanceA, entt::entity entityA, Standing b, float distanceB,
                                    entt::entity entityB)
{
	return std::tie(a, distanceA, entityA) < std::tie(b, distanceB, entityB);
}

Survey temple_villagers::Take(PlayerNames player, float radius, bool floorEdges)
{
	Survey survey;
	if (!Locator::entitiesRegistry::has_value())
	{
		return survey;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const components::Temple, const components::Transform>(
	    [&](entt::entity temple, const components::Temple& component, const components::Transform& transform) {
		    if (component.owner == player && survey.temple == entt::null)
		    {
			    survey.temple = temple;
			    survey.templePosition = transform.position;
		    }
	    });
	if (survey.temple == entt::null)
	{
		return survey;
	}
	if (const auto* worship = registry.TryGet<const components::CitadelWorship>(survey.temple); worship != nullptr)
	{
		SurveySites(registry, *worship, survey);
	}
	SurveyTowns(registry, player, survey);
	SurveyVillagers(registry, radius, survey);
	SurveyInside(floorEdges, survey);
	return survey;
}
