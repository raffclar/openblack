/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature comes to believe about the things about it: each turn its eyes sweep a few of the map cells about it
// and it notes every thing in the cells it can see; going into another cell it notes the nearest temple; each new
// belief leads it on to what the thing belongs to (a tree's forest, an animal's flock, a house's town). Beliefs about
// things gone from the world are forgotten each turn.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <limits>
#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace beliefs = openblack::creature_object_beliefs;
namespace kinds = openblack::creature_tree::belief_types;

namespace
{
/// The kind of belief a creature keeps about a thing, by the game's belief types; none for the things it doesn't yet
/// know the kind of here
std::optional<uint32_t> BeliefKindOf(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Temple>(thing))
	{
		// The temple here is one thing: the game's temple and the building that stands for it
		return kinds::k_Citadel;
	}
	if (registry.AllOf<ForestTurns>(thing))
	{
		return kinds::k_Forest;
	}
	if (registry.AllOf<Flock>(thing))
	{
		return kinds::k_Flock;
	}
	if (registry.AllOf<Town>(thing))
	{
		return kinds::k_Town;
	}
	if (registry.AllOf<Creature>(thing))
	{
		return kinds::k_Creature;
	}
	if (registry.AllOf<Villager>(thing))
	{
		return kinds::k_Villager;
	}
	if (registry.AllOf<Animal>(thing))
	{
		return kinds::k_Animal;
	}
	if (registry.AllOf<Tree>(thing))
	{
		return kinds::k_Tree;
	}
	if (registry.AllOf<Abode>(thing))
	{
		return kinds::k_Abode;
	}
	if (registry.AllOf<Field>(thing))
	{
		return kinds::k_Field;
	}
	if (registry.AllOf<Feature>(thing))
	{
		return kinds::k_Feature;
	}
	return std::nullopt;
}

/// Where a thing is: where it stands, or a flock's own place
glm::vec3 PlaceOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* at = registry.TryGet<const Transform>(thing))
	{
		return at->position;
	}
	if (const auto* flock = registry.TryGet<const Flock>(thing))
	{
		const auto metres = map_coords::ToMetres(flock->place);
		return {metres.x, flock->place.altitude, metres.y};
	}
	return glm::vec3(0.0f);
}

/// The way a creature looks, as a game angle: towards what its head is turned to, or else the way its body faces
uint16_t LookingAngle(const Transform& transform, const std::optional<glm::vec3>& lookAt)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	const auto direction = lookAt.has_value() && *lookAt != transform.position ? *lookAt - transform.position : ahead;
	// Turned as the game keeps a creature's turn, a quarter turn on from the map's, and nothing for no direction across
	// the land
	constexpr float k_NoDirection = 1e-6f;
	const float scawen =
	    direction.x * direction.x + direction.z * direction.z > k_NoDirection
	        ? static_cast<float>(std::atan2(static_cast<double>(direction.x), -static_cast<double>(direction.z)))
	        : 0.0f;
	return static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(scawen));
}

/// The temple nearest a point, of every player's in turn and then the neutral one's; the first of equals
entt::entity NearestTemple(const ecs::Registry& registry, const map_coords::MapCoords& from)
{
	if (!Locator::worshipSiteSystem::has_value())
	{
		return entt::null;
	}
	entt::entity nearest = entt::null;
	auto within = std::numeric_limits<float>::max();
	for (uint8_t player = 0; player < static_cast<uint8_t>(PlayerNames::_COUNT); ++player)
	{
		const auto temple = Locator::worshipSiteSystem::value().TempleOf(static_cast<PlayerNames>(player));
		const auto* at = temple != entt::null ? registry.TryGet<const Transform>(temple) : nullptr;
		if (at == nullptr)
		{
			continue;
		}
		const auto distance = gutils::GetDistanceInMetres(from, map_coords::FromMetres(glm::xz(at->position)));
		if (distance < within)
		{
			nearest = temple;
			within = distance;
		}
	}
	return nearest;
}
} // namespace

void CreatureMindSystem::ComeAcross(components::CreatureMindState& mind, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	const auto kind = BeliefKindOf(registry, thing);
	if (!kind.has_value())
	{
		return;
	}
	const auto added = mind.beliefs.Add(thing, *kind, PlaceOf(registry, thing));
	// A new belief leads it on to what the thing belongs to, but for another creature, which is its own
	if (!added.isNew || *kind == kinds::k_Creature)
	{
		return;
	}
	if (const auto owner = BelongsTo(thing); owner.has_value() && *owner != thing)
	{
		ComeAcross(mind, *owner);
	}
}

void CreatureMindSystem::ForgetGoneThings(components::CreatureMindState& mind)
{
	const auto& registry = Locator::entitiesRegistry::value();
	mind.beliefs.ForgetIf([&registry](entt::entity thing) { return !registry.Valid(thing); });
}

void CreatureMindSystem::NoteNearestTemple(components::CreatureMindState& mind, const Transform& transform)
{
	// Each time it goes into another cell it looks for the nearest temple, which it comes across if it is another than
	// the last it found
	const auto cell = map_coords::CellOf(transform.position);
	if (mind.beliefCell == cell)
	{
		return;
	}
	mind.beliefCell = cell;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto temple = NearestTemple(registry, map_coords::FromMetres(glm::xz(transform.position)));
	if (temple != entt::null && temple != mind.nearestTemple)
	{
		mind.nearestTemple = temple;
		ComeAcross(mind, temple);
	}
}

void CreatureMindSystem::SweepEyes(const Creature& creature, components::CreatureMindState& mind,
                                   const CreatureAnimation& animation, const Transform& transform)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto& map = Locator::entitiesMap::value();
	const auto from = map_coords::FromMetres(glm::xz(transform.position));
	const auto centre = map_coords::Cell(from);
	const auto looking = LookingAngle(transform, animation.lookAt);
	for (const auto cell : beliefs::NextCells(mind.sweep, centre, ShownSize(creature)))
	{
		if (!map_coords::InBounds(cell))
		{
			continue;
		}
		const map_coords::MapCoords corner {cell.x * map_coords::k_FixedPerCell, cell.y * map_coords::k_FixedPerCell, 0.0f};
		if (!beliefs::CanSee(looking, gutils::GetAngleFromXZ(from, corner), cell == centre))
		{
			continue;
		}
		// The things that stay put, then the things that move
		for (const auto thing : map.GetAllInCell(cell))
		{
			ComeAcross(mind, thing);
		}
	}
}
