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

#include <chrono>
#include <limits>
#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Map.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

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

/// How tall a creature is at size 1
constexpr float k_CreatureHeightOfSizeOne = 15.0f;

/// Whether a thing moved in its last game turn, as a creature sees it:
/// - A creature moves while it walks to a place or to a thing.
/// - A firefly always moves.
/// - A thing in the air moves.
/// - An animal or a villager moves when its last turn took it across the land.
/// - Nothing else moves.
bool MovedLastTurn(const ecs::Registry& registry, entt::entity thing)
{
	if (registry.AllOf<Creature>(thing))
	{
		const auto* walk = registry.TryGet<const CreatureLocomotion>(thing);
		return walk != nullptr && walk->destination.has_value() && !walk->facingOnly;
	}
	if (registry.AnyOf<Firefly, InPhysics>(thing))
	{
		return true;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return glm::xz(animal->position) != glm::xz(animal->previousPosition);
	}
	if (const auto* pose = registry.TryGet<const VillagerPose>(thing))
	{
		return pose->movedLastTurn;
	}
	return false;
}

/// How tall a thing is: a creature by its size, anything else by its model as the thing is scaled, and 0 without a model
float HeightOf(const ecs::Registry& registry, entt::entity thing)
{
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		return k_CreatureHeightOfSizeOne * ShownSize(*creature);
	}
	const auto* transform = registry.TryGet<const Transform>(thing);
	const auto* mesh = registry.TryGet<const Mesh>(thing);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return 0.0f;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform->scale.y;
}

/// The game turns in one second
constexpr uint32_t k_TurnsPerSecond =
    static_cast<uint32_t>(std::chrono::milliseconds(std::chrono::seconds(1)) / TimeSystemInterface::k_TurnDuration);

/// How much a thing catches a creature's eye, by what it is. The temple, fields, features and the other things that
/// stand on the land are fixed things.
creature_look::Interest LookInterestOf(const ecs::Registry& registry, entt::entity thing)
{
	using Interest = creature_look::Interest;
	if (registry.AllOf<Creature>(thing))
	{
		return Interest::Creature;
	}
	if (registry.AllOf<Villager>(thing))
	{
		return Interest::Villager;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return animal->type == AnimalInfo::SpellDove ? Interest::Dove : Interest::Animal;
	}
	if (registry.AllOf<Abode>(thing))
	{
		return Interest::Abode;
	}
	if (registry.AllOf<Tree>(thing))
	{
		return Interest::Tree;
	}
	if (registry.AllOf<MobileStatic>(thing))
	{
		return Locator::infoConstants::has_value() &&
		               ecs::physics_classes::IsToy(registry, thing, Locator::infoConstants::value())
		           ? Interest::Toy
		           : Interest::Static;
	}
	if (registry.AllOf<MobileObject>(thing))
	{
		return Interest::None;
	}
	return Interest::Fixed;
}

/// How much things on the land catch the eye of a creature of a species, from its creature info
float LookWeightOf(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? creatures.at(row).lookAtObjectsWeight : 0.0f;
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

void CreatureMindSystem::ComeAcross(entt::entity creature, components::CreatureMindState& mind, entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return;
	}
	// The last moving thing that it comes across is the one that it noticed last, whatever the thing is
	if (creature_look::Notices(thing == creature, MovedLastTurn(registry, thing)))
	{
		// An animal is drawn between its last two turns: the creature notices where the animal is now
		const auto* animal = registry.TryGet<const Animal>(thing);
		const auto at = animal != nullptr ? animal->position : PlaceOf(registry, thing);
		mind.noticedMover = {.at = glm::xz(at), .height = HeightOf(registry, thing), .turn = mind.turn};
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
		ComeAcross(creature, mind, *owner);
	}
}

void CreatureMindSystem::ForgetGoneThings(components::CreatureMindState& mind)
{
	const auto& registry = Locator::entitiesRegistry::value();
	mind.beliefs.ForgetIf([&registry](entt::entity thing) { return !registry.Valid(thing); });
}

void CreatureMindSystem::NoteNearestTemple(entt::entity creature, components::CreatureMindState& mind,
                                           const Transform& transform)
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
		ComeAcross(creature, mind, temple);
	}
}

void CreatureMindSystem::SweepEyes(entt::entity entity, const Creature& creature, components::CreatureMindState& mind,
                                   const CreatureAnimation& animation, const Transform& transform)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto from = map_coords::FromMetres(glm::xz(transform.position));
	const auto centre = map_coords::Cell(from);
	const auto looking = LookingAngle(transform, animation.lookAt);
	const auto facing = LookingAngle(transform, std::nullopt);
	const auto range = creature_look::LookRange(ShownSize(creature));
	const auto weight = LookWeightOf(creature.species);
	// Whether a thing is in view, and how far it is along the ground
	const auto view = [&](entt::entity thing) {
		const auto at = PlaceOf(registry, thing);
		const auto coords = map_coords::FromMetres(glm::xz(at));
		const bool inView =
		    creature_look::InView(looking, facing, gutils::GetAngleFromXZ(from, coords), map_coords::Cell(coords) == centre);
		return std::pair {inView, glm::distance(glm::xz(at), glm::xz(transform.position))};
	};
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
			ComeAcross(entity, mind, thing);
			// Each thing in view that it sees may take over what it watches
			if (thing == entity || !registry.Valid(thing))
			{
				continue;
			}
			const auto [inView, distance] = view(thing);
			if (!inView)
			{
				continue;
			}
			const auto watched = mind.look.id.has_value() ? static_cast<entt::entity>(*mind.look.id) : entt::null;
			const auto watchedDistance = registry.Valid(watched) ? view(watched).second : 0.0f;
			creature_look::Consider(
			    mind.look, {.id = entt::to_integral(thing), .kind = LookInterestOf(registry, thing), .distance = distance},
			    watchedDistance, range, weight, k_TurnsPerSecond);
		}
	}
	// Each turn it comes across what it carries
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(entity); held.has_value())
		{
			ComeAcross(entity, mind, *held);
		}
	}
	// It watches what it watches a turn longer while the thing is there and in view
	const auto watched = mind.look.id.has_value() ? static_cast<entt::entity>(*mind.look.id) : entt::null;
	creature_look::Watch(mind.look, registry.Valid(watched) && view(watched).first);
	if (mind.look.id.has_value())
	{
		// The head looks at the thing's place, as high above the ground as the thing is tall
		const auto at = PlaceOf(registry, watched);
		const auto ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(at)) : at.y;
		mind.look.point = {at.x, ground + HeightOf(registry, watched), at.z};
	}
}
