/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The emergencies that a creature looks at each turn before it decides what to do: low life, a fire on its body, and a
// fire in the nearest town

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <iterator>
#include <string_view>
#include <tuple>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureEmergencies.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{
/// The actions that a creature does in an emergency
constexpr std::string_view k_CastWaterOnItselfAction = "CastMagicWater";
constexpr std::string_view k_DrinkFromTheSeaAction = "DrinkFromTheSea";
constexpr std::string_view k_PutOutFireAction = "PutOutFireWithMagicWater";

/// The nearest town strictly nearer than the reach. The search takes the towns of each player in turn and the neutral
/// towns last. The first town that it finds wins a tie.
entt::entity NearestTownWithin(const ecs::Registry& registry, glm::vec3 point, float reach)
{
	struct Entry
	{
		PlayerNames owner;
		uint32_t id;
		ecs::script_find::Candidate candidate;
	};
	std::vector<Entry> towns;
	registry.Each<const Town, const Transform>([&towns](entt::entity town, const Town& data, const Transform& transform) {
		towns.push_back(
		    {.owner = data.owner,
		     .id = data.id,
		     .candidate = {.entity = town, .at = map_coords::FromMetres({transform.position.x, transform.position.z})}});
	});
	std::ranges::sort(towns, [](const Entry& a, const Entry& b) { return std::tie(a.owner, a.id) < std::tie(b.owner, b.id); });
	std::vector<ecs::script_find::Candidate> ordered;
	ordered.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(ordered), &Entry::candidate);
	return ecs::script_find::FindNearestTown(map_coords::FromMetres({point.x, point.z}), reach, ordered);
}

/// The burning building of a town that the creature goes to, if a building burns
std::optional<entt::entity> BurningBuildingOf(const ecs::Registry& registry, const Town& town)
{
	if (!Locator::fireSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& fires = Locator::fireSystem::value();
	std::vector<creature_emergencies::Building> buildings;
	buildings.reserve(town.abodes.size());
	std::ranges::transform(town.abodes, std::back_inserter(buildings), [&](entt::entity abode) {
		if (!registry.Valid(abode))
		{
			return creature_emergencies::Building {};
		}
		const auto* info = ecs::world_objects::AbodeInfoOf(abode);
		const bool builtPit = info != nullptr && info->abodeType == AbodeType::StoragePit && ecs::construction::IsBuilt(abode);
		const bool centre = info != nullptr && info->abodeType == AbodeType::TownCentre;
		return creature_emergencies::Building {.onFire = fires.IsOnFire(abode), .comesFirst = builtPit || centre};
	});
	const auto picked = creature_emergencies::PickBurningBuilding(buildings);
	return picked.has_value() ? std::optional(town.abodes[*picked]) : std::nullopt;
}
} // namespace

void CreatureMindSystem::StepEmergencies(entt::entity creature, const Creature& body, CreatureMindState& mind, bool scripted)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!mind.desires.has_value() ||
	    !creature_emergencies::LooksAtEmergencies({.emergencyRunning = mind.emergencyRunning,
	                                               .scripted = scripted,
	                                               .planForced = mind.planForced,
	                                               .fighting = registry.AllOf<CreatureFighting>(creature)}))
	{
		return;
	}
	const auto sightings = MiracleSightings(creature, MagicType::Water);
	const bool onFire = Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(creature);
	if (!onFire)
	{
		// Badly hurt, it wants above all to restore its health for a short time
		const auto* needs = registry.TryGet<const CreatureNeeds>(creature);
		if (needs != nullptr && creature_emergencies::IsHurt(needs->needs.life))
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is badly hurt: it wants to restore its health",
			                    entt::to_integral(creature));
			MakeDesireDominant(creature, Desire::RestoreHealth, creature_emergencies::k_RestoreHealthSeconds, true);
			mind.emergencyRunning = true;
			return;
		}
	}
	else
	{
		// On fire, it casts the water miracle on itself, or else it goes to drink from the sea without the leash
		const auto choice = sightings.has_value()
		                        ? creature_emergencies::ChooseOwnFire(sightings->first, sightings->second,
		                                                              mind_detail::CanPayForMiracle(creature, MagicType::Water))
		                        : creature_emergencies::OwnFire::DrinkFromTheSea;
		const bool drinks = choice == creature_emergencies::OwnFire::DrinkFromTheSea;
		if (drinks && Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(creature))
		{
			Locator::leashSystem::value().TakeOff(creature);
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} is on fire: it {}", entt::to_integral(creature),
		                    drinks ? "goes to drink from the sea" : "casts water on itself");
		const bool planned = ForceActivity(creature, {.desire = Desire::RestoreHealth,
		                                              .action = drinks ? k_DrinkFromTheSeaAction : k_CastWaterOnItselfAction,
		                                              .actionObject = creature,
		                                              .ordered = true});
		if (planned && mind.idle.step < mind.idle.agenda.size())
		{
			// A creature of no player or of the computer keeps the run speed for each walk of the plan
			const auto owner = body.owner;
			const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(owner);
			if (creature_emergencies::KeepsRunSpeed(owner != PlayerNames::NEUTRAL, computer))
			{
				for (auto& step : mind.idle.agenda)
				{
					step.movement.run = true;
				}
			}
			mind.planForced = true;
			mind.emergencyRunning = true;
			return;
		}
	}
	// A thing that burns in the nearest town: it goes to put the fire out with the water miracle when it cares enough
	const auto* at = registry.TryGet<const Transform>(creature);
	const auto townEntity =
	    at != nullptr ? NearestTownWithin(registry, at->position, creature_emergencies::k_TownReach) : entt::null;
	if (townEntity == entt::null || !sightings.has_value() || !mind.learnt.has_value())
	{
		return;
	}
	const auto& known = mind.learnt->knowledge.miraclesKnown;
	const auto water = static_cast<size_t>(MagicType::Water);
	const auto& desires = *mind.desires;
	const creature_emergencies::TownFireFacts facts {
	    .knowsWater = water < known.size() && known[water],
	    .seen = sightings->first,
	    .needed = sightings->second,
	    .canPay = mind_detail::CanPayForMiracle(creature, MagicType::Water),
	    .compassionActivated = desires[Desire::Compassion].activated,
	    .compassionSuppressed = desires[Desire::Compassion].suppressedTurns > 0,
	    .compassion = desires[Desire::Compassion].value,
	    .anger = desires[Desire::Anger].value,
	};
	if (!creature_emergencies::PutsOutTownFire(facts))
	{
		return;
	}
	const auto burning = BurningBuildingOf(registry, registry.Get<const Town>(townEntity));
	if (!burning.has_value())
	{
		return;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} goes to put out the fire on {}", entt::to_integral(creature),
	                    entt::to_integral(*burning));
	// It comes across the burning building and its town, so the plan has them as they are now
	ComeAcrossWithWhole(creature, mind, *burning);
	ForceActivity(creature, {.desire = Desire::Compassion,
	                         .activityObject = BelongsTo(*burning),
	                         .action = k_PutOutFireAction,
	                         .actionObject = *burning,
	                         .ordered = true});
	// The emergency starts and the plan counts as forced, also when the creature could not make the plan
	mind.planForced = true;
	mind.emergencyRunning = true;
}
