/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObstacles.h"
#include "CreatureFightSystem.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/ArenaArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/LightSheet.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;
using namespace openblack::ecs::systems::fight_detail;

namespace
{
/// The sound of a fight taking its arena
constexpr auto k_ArenaDrawnSound = entt::hashed_string("InGame.sad/176");

/// The species' running share of its top speed, as the game's tables have it
float RunShareOf(CreatureType species)
{
	if (Locator::infoConstants::has_value())
	{
		const auto& creatures = Locator::infoConstants::value().creature;
		const auto row = creature::InfoRow(species);
		if (row < creatures.size())
		{
			return creatures.at(row).runSpeed;
		}
	}
	return 0.0f;
}

/// What a thing standing fixed in a land cell is, as an arena looking for room sees it
arena::ThingInCell ThingInCellOf(const ecs::Registry& registry, entt::entity thing)
{
	arena::ThingInCell seen {.kind = arena::ThingKind::Fixed,
	                         .living = registry.AnyOf<Villager, Creature, Animal>(thing),
	                         .hasModel = ecs::thing_model::ModelOf(registry, thing).has_value(),
	                         .height = ecs::world_objects::SizeOf(thing).height,
	                         .circles = {}};
	if (registry.AllOf<Temple>(thing))
	{
		seen.kind = arena::ThingKind::Temple;
	}
	else if (const auto* info = ecs::world_objects::AbodeInfoOf(thing);
	         info != nullptr && (info->abodeType == AbodeType::StoragePit || info->abodeType == AbodeType::TownCentre))
	{
		seen.kind = arena::ThingKind::StoragePitOrTownCentre;
	}
	else if (registry.AnyOf<Abode, Field, SpellDispenser>(thing))
	{
		seen.kind = arena::ThingKind::Building;
	}
	else if (registry.AnyOf<Tree, DeadTree, MobileStatic>(thing))
	{
		// Trees, dead trees and statics can all be thrown about
		seen.kind = arena::ThingKind::Movable;
	}
	// A worship site keeps creatures off with ten balls that keep their height in the world
	if (const auto* transform = registry.TryGet<const Transform>(thing);
	    transform != nullptr && registry.AllOf<WorshipSite>(thing))
	{
		const auto spheres = creature_cast_moves::WorshipSiteSpheres(
		    transform->position, creature_obstacles::YAngleOf(transform->rotation), transform->scale.x);
		for (const auto& sphere : spheres)
		{
			seen.circles.push_back(
			    {.centre = {sphere.centre.x, sphere.centre.z}, .radius = sphere.radius, .height = sphere.centre.y});
		}
		return seen;
	}
	for (const auto& circle : ecs::thing_circles::CirclesOf(registry, thing))
	{
		seen.circles.push_back({.centre = circle.centre, .radius = circle.radius});
	}
	return seen;
}

/// Whether an arena for a creature of a size may lie over a land cell: one on the map, of land without water, with
/// nothing standing in the way, that creatures can walk to
bool UsableForArena(const ecs::Registry& registry, glm::ivec2 cell, const land_avoid::Map& avoid, float creatureSize)
{
	constexpr auto k_Side = LandIslandInterface::k_MapCellsPerSide;
	if (cell.x < 0 || cell.y < 0 || cell.x >= k_Side || cell.y >= k_Side || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto* land = Locator::terrainSystem::value().FindCell({static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)});
	if (land == nullptr || land->properties.hasWater != 0)
	{
		return false;
	}
	// Only things standing fixed cover circles on the ground; things that move about never keep an arena off
	if (Locator::entitiesMap::has_value())
	{
		const auto things = Locator::entitiesMap::value().GetFixedInGridCell(
		    ecs::MapInterface::CellId(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)));
		const bool blocked = std::ranges::any_of(things, [&](entt::entity thing) {
			return registry.Valid(thing) && arena::KeepsArenaOff(ThingInCellOf(registry, thing), creatureSize, cell);
		});
		if (blocked)
		{
			return false;
		}
	}
	return avoid.At(cell) == land_avoid::k_Land;
}

/// The nearest arena strictly closer than a distance in metres to a point in map units, if any
std::optional<entt::entity> NearestArena(const ecs::Registry& registry, glm::ivec2 point, float within)
{
	std::vector<entt::entity> entities;
	std::vector<glm::ivec2> places;
	registry.Each<const CreatureArena>([&](entt::entity entity, const CreatureArena& found) {
		entities.push_back(entity);
		places.push_back(found.place);
	});
	if (const auto nearest = arena::Nearest(places, point, within))
	{
		return entities.at(*nearest);
	}
	return std::nullopt;
}

/// A new arena of a radius wanted, or smaller, at the nearest place clear for a creature of a size round a point in map
/// units; none without room. It lasts only for the fight that takes it.
std::optional<entt::entity> MakeArena(ecs::Registry& registry, glm::ivec2 start, float wanted, float creatureSize)
{
	const auto& avoid = LandAvoidOf(registry);
	const auto placed = arena::Place(start, wanted, [&registry, &avoid, creatureSize](glm::ivec2 cell) {
		return UsableForArena(registry, cell, avoid, creatureSize);
	});
	if (!placed.has_value())
	{
		return std::nullopt;
	}
	return ecs::archetypes::ArenaArchetype::Create(placed->centre, placed->radius, true);
}

/// The arena two creatures about to fight take: the nearest one near enough to the middle between them, shrunk to the
/// size wanted, or a new one for this fight where there is room
std::optional<entt::entity> TakeArena(ecs::Registry& registry, entt::entity creature, entt::entity opponent)
{
	const auto& body = registry.Get<const Creature>(creature);
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(opponent).position;
	// The middle in map units, as the game works it out without rounding on the way
	const auto middle = [](float a, float b) {
		return static_cast<int32_t>((static_cast<double>(a) + static_cast<double>(b)) * 0.5 *
		                            static_cast<double>(map_coords::k_FixedPerMetre));
	};
	const glm::ivec2 start {middle(from.x, to.x), middle(from.z, to.z)};
	const float wanted = fight::ArenaRadius(ShownSize(body), ShownSize(registry.Get<const Creature>(opponent)));

	if (const auto nearest = NearestArena(registry, start, arena::ReuseDistance(RunShareOf(body.species), ShownSize(body))))
	{
		auto& taken = registry.Get<CreatureArena>(*nearest);
		taken.radius = std::min(taken.radius, wanted);
		taken.temporary = false;
		return nearest;
	}
	return MakeArena(registry, start, wanted, ShownSize(body));
}

/// A fight is on in the arena: its ring of light stands and its sound plays
void StartArenaFight(ecs::Registry& registry, entt::entity entity, entt::entity first, entt::entity second)
{
	auto& taken = registry.Get<CreatureArena>(entity);
	taken.fightOn = true;
	taken.first = first;
	taken.second = second;
	// An arena of a loaded file of the game has no ring. Its first fight makes a white ring of its radius now.
	if (taken.ringRgb == 0)
	{
		taken.ringRgb = arena::k_LoadedRingRgb;
		taken.ring = ecs::archetypes::ArenaArchetype::MakeRing(taken.place, taken.radius, taken.ringRgb);
	}
	if (taken.ring != nullptr)
	{
		taken.ring->SetHidden(false);
	}
	if (Locator::sounds::has_value())
	{
		audio::StartGameSoundEffect(k_ArenaDrawnSound.value(),
		                            {.position = registry.Get<const Transform>(entity).position, .owner = entity});
	}
}

/// Where the creature is at the end of this turn, as the fight rules see it. The drawn place moves to it during the turn.
glm::vec3 FightPlaceOf(const ecs::Registry& registry, entt::entity creature)
{
	if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature))
	{
		return locomotion->toPosition;
	}
	return registry.Get<const Transform>(creature).position;
}
} // namespace

namespace openblack::ecs::systems::fight_detail
{
const land_avoid::Map& LandAvoidOf(ecs::Registry& registry)
{
	auto& context = registry.Context();
	if (!context.landAvoid.has_value())
	{
		if (!Locator::terrainSystem::has_value())
		{
			context.landAvoid.emplace();
			return *context.landAvoid;
		}
		const auto& island = Locator::terrainSystem::value();
		const auto find = [&island](glm::ivec2 cell) {
			return island.FindCell({static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y)});
		};
		context.landAvoid = land_avoid::Build(
		    LandIslandInterface::k_MapCellsPerSide,
		    [&find](glm::ivec2 corner) {
			    const auto* cell = find(corner);
			    return cell != nullptr ? cell->altitude : uint8_t {0};
		    },
		    [&find](glm::ivec2 cell) {
			    const auto* found = find(cell);
			    return found == nullptr || found->properties.hasWater != 0;
		    });
	}
	return *context.landAvoid;
}

void ReleaseArena(ecs::Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return;
	}
	auto* taken = registry.TryGet<CreatureArena>(entity);
	if (taken == nullptr)
	{
		return;
	}
	if (taken->temporary)
	{
		if (Locator::reactionSystem::has_value())
		{
			Locator::reactionSystem::value().RemoveFrom(entity);
		}
		registry.Destroy(entity);
		return;
	}
	taken->fightOn = false;
	taken->first = entt::null;
	taken->second = entt::null;
	if (taken->ring != nullptr)
	{
		taken->ring->SetHidden(true);
	}
}

bool DrawArenaFor(ecs::Registry& registry, entt::entity creature, entt::entity opponent)
{
	const auto arenaEntity = TakeArena(registry, creature, opponent);
	if (!arenaEntity.has_value())
	{
		return false;
	}
	const auto& taken = registry.Get<const CreatureArena>(*arenaEntity);
	const fight::Arena arena {.centre = {map_coords::ToMetres(taken.place.x), map_coords::ToMetres(taken.place.y)},
	                          .radius = taken.radius};
	for (const auto self : {creature, opponent})
	{
		auto& fighting = registry.Get<CreatureFighting>(self);
		fighting.arena = arena;
		fighting.arenaEntity = *arenaEntity;
	}
	StartArenaFight(registry, *arenaEntity, creature, opponent);
	fight_transcript::Note(creature, "arena", [&] {
		using fight_transcript::Id;
		return fight_transcript::Fields {{"arena", Id(*arenaEntity)},
		                                 {"arena_x", arena.centre.x},
		                                 {"arena_z", arena.centre.y},
		                                 {"arena_radius", arena.radius},
		                                 {"arena_made_by", Id(creature)}};
	});
	return true;
}

bool KeepInArena(ecs::Registry& registry, entt::entity creature, CreatureFighting& fighting)
{
	const auto& arena = fighting.arena;
	const auto selfAt = FightPlaceOf(registry, creature);
	const auto otherAt = FightPlaceOf(registry, fighting.opponent);
	if (!fight::KeepsInArena(arena, Flat(selfAt), Flat(otherAt)))
	{
		return false;
	}
	auto& fighter = fighting.fighter;
	std::optional<fight::Step> step;
	std::string_view stepName {"none"};
	if (fighter.state == fight::State::Block)
	{
		fight::Enter(fighter, fight::State::BlockEnd);
	}
	else if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	         fighter.state == fight::State::Stance && locomotion != nullptr)
	{
		const glm::vec3 middle {arena.centre.x, GroundAt(arena.centre), arena.centre.y};
		step = fight::StepToMiddle(WorldToLocal(arena.centre - Flat(selfAt), locomotion->heading),
		                           glm::distance(middle, selfAt), locomotion->shownRadius);
		// A species with no animation for the step stays in its stance
		if (step.has_value() && !Has(creature, fight::StepAnimation(*step)))
		{
			step.reset();
		}
		if (step.has_value())
		{
			fight::Enter(fighter, fight::State::Action, fight::StepAnimation(*step));
			stepName = fight::Name(*step);
		}
	}
	fight_transcript::Note(creature, "keep_in_arena", [&] {
		return fight_transcript::Fields {
		    {"creature", fight_transcript::Id(creature)}, {"state", fight::Name(fighter.state)}, {"step", stepName}};
	});
	return true;
}
} // namespace openblack::ecs::systems::fight_detail

std::optional<CreatureFightSystemInterface::FoundArena>
CreatureFightSystem::FindOrMakeArena(const glm::vec3& point, entt::entity creature, entt::entity other, float within)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* first = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto* second = registry.Valid(other) ? registry.TryGet<const Creature>(other) : nullptr;
	if (first == nullptr || second == nullptr)
	{
		return std::nullopt;
	}
	const glm::ivec2 start {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z)};
	// An arena found is taken as it is, whatever its size
	if (const auto nearest = NearestArena(registry, start, within))
	{
		return FoundArena {.arena = *nearest, .made = false};
	}
	const float wanted = fight::ArenaRadius(ShownSize(*first), ShownSize(*second));
	if (const auto made = MakeArena(registry, start, wanted, ShownSize(*first)))
	{
		return FoundArena {.arena = *made, .made = true};
	}
	return std::nullopt;
}
