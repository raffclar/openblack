/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Creature/CreatureExplorationAgendas.h"
#include "CreatureMindSystem.h"
#include "CreatureMindSystemDetail.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using creature_exploration::Kind;
using creature_plan_actions::Build;
namespace land = openblack::ecs::creature_exploration_land;

void CreatureMindSystem::SituateExploration(entt::entity creature, const creature_plan_actions::Executor& executor,
                                            creature_plan_actions::Situation& situation)
{
	const auto from = situation.position;
	// A region of the kind that the creature did not go into, else a region of land that it did not go into
	const auto unvisited = [&](Kind kind) {
		auto place = land::FindPlace(creature, kind, from, true);
		if (!place.has_value())
		{
			place = land::FindPlace(creature, Kind::Land, from, true);
		}
		return place;
	};
	const auto nearest = [&](Kind kind, glm::vec2 start) { return land::FindPlace(creature, kind, start, false); };
	// Where the map has no place at all, the creature goes to the low corner of the map, as in the game
	constexpr glm::vec2 k_Corner {0.0f};
	switch (executor.build)
	{
	case Build::GoToHillAndLook:
	case Build::WalkAlongRidge:
		situation.explorePlace = unvisited(Kind::Hill).value_or(k_Corner);
		break;
	case Build::GoToHillAndSit:
		// Where the map has no place at all, it stays where it is
		situation.explorePlace = unvisited(Kind::Hill).value_or(from);
		break;
	case Build::ExploreCoast:
		situation.explorePlace = unvisited(Kind::Coast).value_or(k_Corner);
		break;
	case Build::ExploreTowns:
		situation.explorePlace = unvisited(Kind::Town).value_or(k_Corner);
		break;
	case Build::LookAtMountains:
		situation.explorePlace = nearest(Kind::Hill, from);
		break;
	case Build::LookOutToSea:
		// The sea nearest to the coast, not to the creature
		situation.explorePlace = nearest(Kind::Coast, from);
		situation.exploreSea = situation.explorePlace.has_value() ? nearest(Kind::Sea, *situation.explorePlace) : std::nullopt;
		break;
	case Build::ThrowInTheSea:
	case Build::DanceByTheSea:
		// The coast and the sea that are nearest to the creature
		situation.explorePlace = nearest(Kind::Coast, from).value_or(k_Corner);
		situation.exploreSea = nearest(Kind::Sea, from).value_or(k_Corner);
		break;
	case Build::SitDownOnBeach:
	{
		// A clear place near the nearest sea; one not under a script's control also wants nothing living there
		const auto sea = nearest(Kind::Sea, from).value_or(k_Corner);
		situation.exploreSea = sea;
		situation.explorePlace = mind_detail::ClearAreaFor(Locator::entitiesRegistry::value(), creature, sea,
		                                                   creature_exploration::k_BeachClearRadii * situation.radius,
		                                                   !situation.controlledByScript);
		break;
	}
	default:
		break;
	}
}
