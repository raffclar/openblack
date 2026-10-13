/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpQueryFacts.h"

#include <algorithm>

#include <LNDFile.h>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureAudio.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs::components;
using help_query::Texts;

namespace
{

constexpr Texts k_None {.first = 0, .last = 0};

template <typename Row>
Texts TextsOfRow(const Row& row)
{
	return {.first = static_cast<uint32_t>(row.helpStartEnum), .last = static_cast<uint32_t>(row.helpEndEnum)};
}

/// A row of a table by an index, or none past its end
template <typename Rows, typename Index>
std::optional<Texts> RowTexts(const Rows& rows, Index index)
{
	const auto i = static_cast<size_t>(index);
	if (i >= rows.size())
	{
		return std::nullopt;
	}
	return TextsOfRow(rows[i]);
}

bool IsLocalPlayers(std::optional<PlayerNames> player)
{
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	return player.has_value() && *player == local;
}

/// The texts of the thing's kind, from its row in the info tables
Texts KindTexts(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	if (const auto* row = ecs::world_objects::InfoOf(thing); row != nullptr)
	{
		return TextsOfRow(*row);
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return RowTexts(info.animal, animal->type).value_or(k_None);
	}
	if (const auto* flowers = registry.TryGet<const Flowers>(thing))
	{
		return RowTexts(info.flowers, flowers->type).value_or(k_None);
	}
	if (const auto* reward = registry.TryGet<const Reward>(thing))
	{
		return RowTexts(info.reward, reward->type).value_or(k_None);
	}
	if (const auto* highlight = registry.TryGet<const ScriptHighlight>(thing))
	{
		return RowTexts(info.scriptHighlight, highlight->kind).value_or(k_None);
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		// A worship site is its tribe's
		const auto tribe = static_cast<size_t>(site->tribe);
		if (tribe < info.tribe.size())
		{
			return RowTexts(info.worshipSite, info.tribe.at(tribe).worshipSiteInfo).value_or(k_None);
		}
		return k_None;
	}
	if (const auto* totem = registry.TryGet<const VillageTotem>(thing))
	{
		// The totem stands in the town centre, of the centre's tribe
		if (const auto* centre = ecs::world_objects::AbodeInfoOf(totem->townCentre); centre != nullptr)
		{
			return RowTexts(info.totemStatue, centre->tribeType).value_or(k_None);
		}
		return k_None;
	}
	if (registry.AllOf<FishFarm>(thing))
	{
		return TextsOfRow(info.fishFarm);
	}
	if (registry.AllOf<Firefly>(thing))
	{
		constexpr size_t k_FireflyRow = 2;
		return TextsOfRow(info.singleMapFixed.at(k_FireflyRow));
	}
	return k_None;
}

bool IsDead(entt::entity villager)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<VillagerDeath>(villager))
	{
		return true;
	}
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return false;
	}
	const auto top = static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Top)));
	return top == VillagerStates::SetDying || top == VillagerStates::Dying || top == VillagerStates::Dead;
}

/// Belongs to no town: neither housed nor among its town's homeless
bool IsVagrant(entt::entity entity, const Villager& villager)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (villager.abode != entt::null && registry.Valid(villager.abode))
	{
		return false;
	}
	if (villager.town == entt::null || !registry.Valid(villager.town))
	{
		return true;
	}
	const auto* town = registry.TryGet<const Town>(villager.town);
	return town == nullptr || std::ranges::find(town->homelessVillagers, entity) == town->homelessVillagers.end();
}

/// The text of what a villager is doing, by the state it works towards
uint32_t ActivityText(entt::entity villager)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return 0;
	}
	const auto state = static_cast<size_t>(Locator::livingActionSystem::value().VillagerGetFinalState(*action));
	const auto& states = Locator::infoConstants::value().villagerStateTable;
	return state < states.size() ? states.at(state).queryText : 0;
}

/// Is a pile of food in a finished storage pit
bool InFinishedStore(entt::entity pile)
{
	const auto& registry = Locator::entitiesRegistry::value();
	bool found = false;
	registry.Each<const StoragePit>([&](entt::entity store, const StoragePit& pit) {
		if (pit.foodPile != pile)
		{
			return;
		}
		const auto* progress = registry.TryGet<const BuildProgress>(store);
		found = found || progress == nullptr || progress->built >= 1.0f;
	});
	return found;
}

} // namespace

Texts ecs::help_query_facts::TextsOf(entt::entity thing)
{
	if (!Locator::infoConstants::has_value())
	{
		return k_None;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();

	if (const auto* flag = registry.TryGet<const TownDesireFlag>(thing))
	{
		return RowTexts(info.townDesire, flag->desire).value_or(k_None);
	}
	if (registry.AllOf<Villager>(thing))
	{
		const auto* disciple = registry.TryGet<const Disciple>(thing);
		return help_query::VillagerTexts(ActivityText(thing),
		                                 disciple != nullptr ? std::optional(disciple->type) : std::nullopt, KindTexts(thing));
	}
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		const auto row = creature::InfoRow(creature->species);
		if (row >= info.creature.size())
		{
			return k_None;
		}
		return help_query::CreatureTexts(TextsOfRow(info.creature.at(row)), info.creature.at(row).row,
		                                 IsLocalPlayers(creature->owner));
	}
	if (const auto* pot = registry.TryGet<const Pot>(thing))
	{
		const auto row = static_cast<size_t>(pot->type);
		if (row < info.pot.size() && info.pot.at(row).potType == PotType::PileFood)
		{
			return help_query::FoodPileTexts(InFinishedStore(thing), KindTexts(thing));
		}
	}
	if (const auto* seed = registry.TryGet<const OneOffSpellSeed>(thing))
	{
		const auto miracle = RowTexts(info.spellSeed, seed->seedType).value_or(k_None);
		const auto object = RowTexts(info.mobileObject, MobileObjectInfo::OneOffSpellSeed).value_or(k_None);
		return help_query::SeedTexts(miracle, object);
	}
	if (const auto* spell = registry.TryGet<const Spell>(thing))
	{
		if (static_cast<size_t>(spell->magicType) < info.magicEffect.size())
		{
			return TextsOfRow(magic::GetMagicEffectInfo(info, spell->magicType));
		}
		return k_None;
	}
	if (registry.AllOf<TempleEntrance>(thing))
	{
		return help_query::k_TempleEntranceTexts;
	}
	if (const auto* feature = registry.TryGet<const Feature>(thing))
	{
		const auto* progress = registry.TryGet<const BuildProgress>(thing);
		return help_query::FeatureTexts(feature->type, progress != nullptr ? progress->built : 1.0f, KindTexts(thing));
	}
	return KindTexts(thing);
}

help_query::Thing ecs::help_query_facts::ThingOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	help_query::Thing facts {.texts = TextsOf(thing)};

	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		const auto* disciple = registry.TryGet<const Disciple>(thing);
		facts.person = help_query::Person {
		    .scriptControlled = registry.AllOf<ScriptControlled>(thing),
		    .workingDisciple = disciple != nullptr && disciple::KindOf(disciple->type).waitsWhenIdle,
		    .dead = IsDead(thing),
		    .child = villager->lifeStage == Villager::LifeStage::Child,
		    .vagrant = IsVagrant(thing, *villager),
		    .players = IsLocalPlayers(world_objects::PlayerOf(thing)),
		};
		return facts;
	}
	if (const auto* abode = world_objects::AbodeInfoOf(thing); abode != nullptr)
	{
		facts.othersWonder = abode->abodeType == AbodeType::Wonder && !IsLocalPlayers(world_objects::PlayerOf(thing));
		if (abode->abodeType == AbodeType::TownCentre)
		{
			facts.role = help_query::Role::Town;
		}
		return facts;
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		facts.role = help_query::Role::Town;
		return facts;
	}
	if (const auto* flag = registry.TryGet<const TownDesireFlag>(thing))
	{
		facts.role = help_query::Role::TownNeed;
		const auto& rows = Locator::infoConstants::value().townDesire;
		if (const auto row = static_cast<size_t>(flag->desire); row < rows.size())
		{
			facts.needText = static_cast<uint32_t>(rows.at(row).helpStatEnum);
		}
		if (flag->town != entt::null && registry.Valid(flag->town) && Locator::townDesireSystem::has_value())
		{
			facts.rawDesire = Locator::townDesireSystem::value().GetRawDesire(flag->town, flag->desire);
		}
	}
	return facts;
}

help_query::Ground ecs::help_query_facts::GroundAt(std::optional<glm::vec3> point)
{
	help_query::Ground ground;
	if (!point.has_value() || !Locator::terrainSystem::has_value())
	{
		return ground;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto coords = map_coords::FromWorld(land, *point);
	// The game takes a point at the map's very corner, on the ground, for no point at all
	if (coords.x == 0 && coords.z == 0 && coords.altitude == 0.0f)
	{
		return ground;
	}
	ground.hasPoint = true;

	const auto world = map_coords::ToWorld(land, coords);
	ground.altitude = world.y;
	if (Locator::fishFarmSystem::has_value())
	{
		ground.fishFarm = Locator::fishFarmSystem::value().FarmWithFishAt({world.x, world.z}).has_value();
	}

	const auto cellX = map_coords::CellOf(coords.x);
	const auto cellZ = map_coords::CellOf(coords.z);
	const auto onMap = cellX < LandIslandInterface::k_MapCellsPerSide && cellZ < LandIslandInterface::k_MapCellsPerSide;
	const auto* cell = onMap ? land.FindCell({cellX, cellZ}) : nullptr;
	ground.onLand = cell != nullptr;

	// What the ground is made of: deep snow at the cell's middle is snow, else the second of the two materials the
	// cell's country blends at its altitude; nothing there is deep water
	std::optional<uint16_t> type;
	if (cell != nullptr)
	{
		const auto& countries = land.GetCountries();
		const auto types = land.GetMaterialTypes();
		if (cell->properties.country < countries.size())
		{
			const auto index = countries[cell->properties.country].materials.at(cell->altitude).indices[1];
			if (index < types.size())
			{
				type = types[index];
			}
		}
	}
	const auto middleX = (static_cast<float>(cellX) + 0.5f) * LandIslandInterface::k_CellSize;
	const auto middleZ = (static_cast<float>(cellZ) + 0.5f) * LandIslandInterface::k_CellSize;
	const float snow = Locator::snowSystem::has_value() ? Locator::snowSystem::value().GetDepth({middleX, middleZ}) : 0.0f;
	const auto material = creature_audio::TerrainMaterial(type, snow);
	if (Locator::infoConstants::has_value())
	{
		ground.material = RowTexts(Locator::infoConstants::value().terrainMaterial, material).value_or(k_None);
	}
	return ground;
}
