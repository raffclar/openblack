/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <ranges>
#include <vector>

#include <VanillaWorld.h>
#include <spdlog/spdlog.h>

#include "ECS/Archetypes/BigForestArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
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
