/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <algorithm>
#include <map>
#include <ranges>
#include <utility>
#include <vector>

#include <VanillaWorld.h>
#include <spdlog/spdlog.h>

#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/DeadTreeArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/VanillaSceneryImport.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace world_import = openblack::ecs::vanilla_world_import;
using openblack::ecs::systems::vanilla_import::EntityOf;
using openblack::ecs::systems::vanilla_import::GroundPosition;

namespace
{
std::shared_ptr<spdlog::logger> ImportLog()
{
	return spdlog::get("game");
}

/// The game clears the map before it reads the world of a file. The import removes the trees, the big forests and the
/// forests of the land, before it makes them from the file. The trees of a forest miracle stay.
void RemoveLandForests(ecs::Registry& registry)
{
	std::vector<entt::entity> things;
	registry.Each<const Tree>([&registry, &things](entt::entity entity, const Tree&) {
		if (!registry.AllOf<MagicTree>(entity))
		{
			things.push_back(entity);
		}
	});
	registry.Each<const BigForest>([&things](entt::entity entity, const BigForest&) { things.push_back(entity); });
	for (const auto thing : things)
	{
		ecs::world_objects::Remove(thing);
	}
	std::vector<entt::entity> forests;
	registry.Each<const LandForest>([&forests](entt::entity entity, const LandForest&) { forests.push_back(entity); });
	for (const auto forest : forests)
	{
		registry.Destroy(forest);
	}
	registry.Each<TownForests>([](TownForests& listed) { listed.forests.clear(); });
	registry.SetDirty();
}

/// The kinds of things of the land that a still thing or a loose thing of the file can be. A rock is a static thing of
/// the land, so the static things and the rocks of the file look among the same things.
enum class LandFamily : uint8_t
{
	Static,
	Lantern,
	DeadTree,
	Feature,
	AnimatedStatic,
	MobileObject,
};

LandFamily FamilyOf(world_import::StaticKind kind)
{
	switch (kind)
	{
	case world_import::StaticKind::MobileStatic:
	case world_import::StaticKind::Rock:
	case world_import::StaticKind::BaseOnly:
		return LandFamily::Static;
	case world_import::StaticKind::StreetLantern:
		return LandFamily::Lantern;
	case world_import::StaticKind::DeadTree:
		return LandFamily::DeadTree;
	case world_import::StaticKind::Feature:
		return LandFamily::Feature;
	case world_import::StaticKind::AnimatedStatic:
		return LandFamily::AnimatedStatic;
	case world_import::StaticKind::MobileObject:
		return LandFamily::MobileObject;
	}
	return LandFamily::Static;
}

using LandKey = std::pair<LandFamily, uint32_t>;

/// The still things and the loose things of the land, by their kind and their row
std::map<LandKey, std::vector<entt::entity>> LandStatics(ecs::Registry& registry)
{
	std::map<LandKey, std::vector<entt::entity>> things;
	const auto add = [&things](LandFamily family, auto row, entt::entity entity) {
		things[{family, static_cast<uint32_t>(row)}].push_back(entity);
	};
	registry.Each<const MobileStatic>(
	    [&add](entt::entity entity, const MobileStatic& thing) { add(LandFamily::Static, thing.type, entity); });
	registry.Each<const StreetLantern>(
	    [&add](entt::entity entity, const StreetLantern& thing) { add(LandFamily::Lantern, thing.info, entity); });
	registry.Each<const DeadTree>(
	    [&add](entt::entity entity, const DeadTree& thing) { add(LandFamily::DeadTree, thing.type, entity); });
	registry.Each<const Feature>(
	    [&add](entt::entity entity, const Feature& thing) { add(LandFamily::Feature, thing.type, entity); });
	registry.Each<const AnimatedStatic>(
	    [&add](entt::entity entity, const AnimatedStatic& thing) { add(LandFamily::AnimatedStatic, thing.type, entity); });
	registry.Each<const MobileObject>(
	    [&add](entt::entity entity, const MobileObject& thing) { add(LandFamily::MobileObject, thing.type, entity); });
	return things;
}

/// Takes the thing of the land that stands where the file puts its thing: at the same map position, or one map unit
/// from it, because the metres of a thing cannot keep every map position. Nothing when no thing of the land is there.
entt::entity TakeLandThing(const ecs::Registry& registry, std::vector<entt::entity>& things, const gamesave::MapCoords& place)
{
	const auto found = std::ranges::find_if(things, [&registry, &place](entt::entity thing) {
		const auto at = ecs::thing_map_place::Of(registry, thing);
		return std::abs(at.x - place.x) <= 1 && std::abs(at.z - place.z) <= 1;
	});
	if (found == things.end())
	{
		return entt::null;
	}
	const auto thing = *found;
	things.erase(found);
	return thing;
}

/// Makes a still thing or a loose thing of the file
entt::entity MakeStatic(const world_import::ImportedStatic& file)
{
	using namespace ecs::archetypes;
	const auto position = GroundPosition(file.position);
	switch (file.kind)
	{
	case world_import::StaticKind::MobileStatic:
	case world_import::StaticKind::Rock:
		return MobileStaticArchetype::Create(position, static_cast<MobileStaticInfo>(file.row), 0.0f, file.xAngle, file.heading,
		                                     file.zAngle, file.scale);
	case world_import::StaticKind::BaseOnly:
	{
		// It shows only as a print on the land, the newest of the prints
		const auto entity = MobileStaticArchetype::Create(position, static_cast<MobileStaticInfo>(file.row), 0.0f, file.xAngle,
		                                                  file.heading, file.zAngle, file.scale);
		auto& registry = Locator::entitiesRegistry::value();
		uint32_t prints = 0;
		registry.Each<const GroundPrint>([&prints](const GroundPrint& print) { prints = std::max(prints, print.made + 1); });
		registry.AssignOrReplace<GroundPrint>(entity, GroundPrint {.made = prints});
		return entity;
	}
	case world_import::StaticKind::StreetLantern:
		return StreetLanternArchetype::Create(position, static_cast<MobileStaticInfo>(file.row));
	case world_import::StaticKind::DeadTree:
		return DeadTreeArchetype::Create(position, static_cast<TreeInfo>(file.row), file.xAngle, file.heading, file.zAngle,
		                                 file.scale);
	case world_import::StaticKind::Feature:
		return FeatureArchetype::Create(position, static_cast<FeatureInfo>(file.row), file.heading, file.scale);
	case world_import::StaticKind::AnimatedStatic:
		return AnimatedStaticArchetype::Create(position, static_cast<AnimatedStaticInfo>(file.row), file.heading, file.scale);
	case world_import::StaticKind::MobileObject:
		return MobileObjectArchetype::Create(position, static_cast<MobileObjectInfo>(file.row), file.heading, file.scale);
	}
	return entt::null;
}

} // namespace

void SaveGameSystem::ImportVanillaForests(const gamesave::VanillaWorld& world, VanillaMade& made)
{
	if (!Locator::forestSystem::has_value())
	{
		return;
	}
	const auto imported = world_import::ReadForests(world);
	for (const auto& why : imported.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not take {}", why);
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& forests = Locator::forestSystem::value();
	RemoveLandForests(registry);

	for (const auto& bigForest : imported.bigForests)
	{
		const auto entity = ecs::archetypes::BigForestArchetype::CreateLoaded(
		    GroundPosition(bigForest.position), bigForest.row, bigForest.heading, bigForest.scale, bigForest.wood);
		ecs::world_objects::SetLife(entity, bigForest.life);
		made.emplace(bigForest.number, entity);
	}

	// The forest that the game meets first is the newest, so the import makes the forests from the last to the first
	for (const auto& forest : std::views::reverse(imported.forests))
	{
		forests.MakeLandForest(forest.id, GroundPosition(forest.position), EntityOf(made, forest.bigForest), forest.scenic);
		made.emplace(forest.number, forests.LandForests().front());
	}

	for (const auto& tree : imported.trees)
	{
		const auto entity = ecs::archetypes::TreeArchetype::CreateLoaded(GroundPosition(tree.position), tree.row, tree.heading,
		                                                                 tree.maxSize, tree.scale, tree.growthCountdown);
		made.emplace(tree.number, entity);
	}
	// Each forest takes its grown trees, then its trees that grow, in the order of the file
	for (const auto& forest : imported.forests)
	{
		for (const auto& [trees, growing] : {std::pair {&forest.grown, false}, std::pair {&forest.growing, true}})
		{
			for (const auto number : *trees)
			{
				if (const auto tree = EntityOf(made, number); tree != entt::null && registry.AllOf<Tree>(tree))
				{
					forests.ListInForest(tree, forest.id, growing);
				}
			}
		}
	}
	// A tree with no life left goes from the world, so the import sets the life after the forests have their trees
	for (const auto& tree : imported.trees)
	{
		if (tree.life != 1.0f)
		{
			ecs::world_objects::SetLife(EntityOf(made, tree.number), tree.life);
		}
	}
	forests.ResumeCounts(imported.lastTreeAddedTurn, imported.nextForestId);

	// Each town takes the forests of its list in the file
	for (const auto* record : world.OfType(gamesave::VanillaType::Town))
	{
		const auto town = EntityOf(made, record->number);
		if (town == entt::null)
		{
			continue;
		}
		auto& listed = registry.AssignOrReplace<TownForests>(town);
		for (const auto number : world.Refs(*record, "forests"))
		{
			if (const auto forest = EntityOf(made, number); forest != entt::null)
			{
				listed.forests.push_back(forest);
			}
		}
	}
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(ImportLog(), "Imported {} forests, {} big forests and {} trees of the game's file",
	                   imported.forests.size(), imported.bigForests.size(), imported.trees.size());
}

void SaveGameSystem::ImportVanillaFarms(const gamesave::VanillaWorld& world, VanillaMade& made, const VanillaWorks& works)
{
	const auto imported = world_import::ReadFarms(world);
	for (const auto& why : imported.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not take {}", why);
	}
	auto& registry = Locator::entitiesRegistry::value();

	// The import of the towns removed the fields of the land. The fish farms of the land go here.
	std::vector<entt::entity> farms;
	registry.Each<const FishFarm>([&farms](entt::entity entity, const FishFarm&) { farms.push_back(entity); });
	for (const auto farm : farms)
	{
		ecs::world_objects::Remove(farm);
	}

	size_t fields = 0;
	for (const auto& field : imported.fields)
	{
		const auto town = EntityOf(made, field.town);
		const auto* component = town != entt::null ? registry.TryGet<Town>(town) : nullptr;
		if (component == nullptr)
		{
			SPDLOG_LOGGER_WARN(ImportLog(), "The field {} of the file has no town of the file, so it is not made",
			                   field.number);
			continue;
		}
		const auto entity = ecs::archetypes::FieldArchetype::CreateLoaded(
		    static_cast<int>(component->id), GroundPosition(field.position), field.row, field.type, field.heading, field.scale,
		    field.built, {.timesSown = field.timesSown, .age = field.age, .food = field.food}, field.growthTurn);
		if (entity == entt::null)
		{
			continue;
		}
		ecs::world_objects::SetLife(entity, field.life);
		made.emplace(field.number, entity);
		++fields;
	}

	if (Locator::fishFarmSystem::has_value())
	{
		for (const auto& farm : imported.fishFarms)
		{
			const auto entity = ecs::archetypes::FishFarmArchetype::CreateLoaded(GroundPosition(farm.position),
			                                                                     EntityOf(made, farm.town), farm.fish);
			made.emplace(farm.number, entity);
		}
	}

	// Each villager that keeps a state of work takes its field or its fish farm, and the spot in its field
	for (const auto& [villager, work] : works)
	{
		registry.AssignOrReplace<WorkTarget>(villager, WorkTarget {.thing = EntityOf(made, work.target)});
		if (!work.spot.has_value())
		{
			continue;
		}
		const auto x = world_import::MetresOfWhole(work.spot->x);
		const auto z = world_import::MetresOfWhole(work.spot->z);
		if (x.has_value() && z.has_value())
		{
			registry.AssignOrReplace<FieldWorkSpot>(villager, FieldWorkSpot {.position = {*x, *z}});
		}
	}
	// A field or a fish farm keeps the workers of its list in the file that work at it
	const auto workers = [&made, &works](uint32_t place, const std::vector<uint32_t>& listed) {
		std::vector<entt::entity> kept;
		for (const auto number : listed)
		{
			const auto villager = EntityOf(made, number);
			if (std::ranges::any_of(works, [villager, place](const auto& work) {
				    return work.first == villager && work.second.target == place;
			    }))
			{
				kept.push_back(villager);
			}
		}
		return kept;
	};
	for (const auto& field : imported.fields)
	{
		if (auto* component = registry.TryGet<Field>(EntityOf(made, field.number)); component != nullptr)
		{
			component->farmers = workers(field.number, field.farmers);
		}
	}
	for (const auto& farm : imported.fishFarms)
	{
		if (auto* component = registry.TryGet<FishFarm>(EntityOf(made, farm.number)); component != nullptr)
		{
			component->fishermen = workers(farm.number, farm.fishermen);
		}
	}

	registry.SetDirty();
	SPDLOG_LOGGER_INFO(ImportLog(), "Imported {} fields and {} fish farms of the game's file; {} villagers keep their work",
	                   fields, imported.fishFarms.size(), works.size());
}

void SaveGameSystem::ImportVanillaStatics(const gamesave::VanillaWorld& world, VanillaMade& made)
{
	const auto imported = world_import::ReadStatics(world);
	for (const auto& why : imported.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not take {}", why);
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto land = LandStatics(registry);

	// A thing of the land at the place of a thing of the file is that thing: it stays, with the life of the file
	size_t kept = 0;
	std::vector<const world_import::ImportedStatic*> toMake;
	for (const auto& file : imported.statics)
	{
		auto& things = land[{FamilyOf(file.kind), file.row}];
		const auto thing = TakeLandThing(registry, things, file.position);
		if (thing == entt::null)
		{
			toMake.push_back(&file);
			continue;
		}
		ecs::thing_map_place::Keep(registry, thing, {file.position.x, file.position.z, 0.0f});
		if (file.life != 1.0f)
		{
			ecs::world_objects::SetLife(thing, file.life);
		}
		made.emplace(file.number, thing);
		++kept;
	}
	// The other things of the land are not in the file, so they go
	size_t removed = 0;
	for (const auto& [key, things] : land)
	{
		for (const auto thing : things)
		{
			if (registry.Valid(thing))
			{
				const auto at = ecs::thing_map_place::Of(registry, thing);
				SPDLOG_LOGGER_DEBUG(ImportLog(), "The thing {} of kind {} and row {} of the land at {}, {} is not in the file",
				                    entt::to_integral(thing), static_cast<int>(key.first), key.second, at.x, at.z);
				ecs::world_objects::Remove(thing);
				++removed;
			}
		}
	}
	// The file makes the rest
	for (const auto* file : toMake)
	{
		const auto entity = MakeStatic(*file);
		if (entity == entt::null)
		{
			continue;
		}
		SPDLOG_LOGGER_DEBUG(ImportLog(), "The file makes its thing {} of kind {} and row {} at {}, {}", file->number,
		                    static_cast<int>(file->kind), file->row, file->position.x, file->position.z);
		ecs::thing_map_place::Keep(registry, entity, {file->position.x, file->position.z, 0.0f});
		if (auto* deadTree = registry.TryGet<DeadTree>(entity))
		{
			deadTree->woodMultiplier = file->woodMultiplier;
		}
		if (file->life != 1.0f)
		{
			ecs::world_objects::SetLife(entity, file->life);
		}
		made.emplace(file->number, entity);
	}
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(ImportLog(),
	                   "Imported {} still and loose things of the game's file: {} of the land stay, {} are made and {} "
	                   "of the land go",
	                   imported.statics.size(), kept, toMake.size(), removed);
}

void SaveGameSystem::ImportVanillaGameValues(const gamesave::VanillaWorld& world)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The count of things made continues from the count of the file
	if (const auto data = world.GameData(); data.has_value())
	{
		registry.Context().thingsMade = data->thingsMade;
	}
	// The clock of day and night: the hour that shows, the cycle, and whether it runs
	if (const auto cycle = world_import::ReadDayCycle(world); cycle.has_value() && Locator::skyClockSystem::has_value())
	{
		auto& clock = Locator::skyClockSystem::value().GetClock();
		clock.ForceVisualTime(cycle->visualTime);
		clock.SetScale(cycle->scale);
		clock.SetCycle(cycle->duration, cycle->night, cycle->change);
	}
}
