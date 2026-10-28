/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownSystem.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <numeric>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/AbodeNeglectRules.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Breeding.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/DiscipleWorkRules.h"
#include "ECS/GameEndConditions.h"
#include "ECS/Map.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TemporaryStoreRules.h"
#include "ECS/TownAggression.h"
#include "ECS/TownAreaRules.h"
#include "ECS/TownCentreParts.h"
#include "ECS/TownDesireAlignment.h"
#include "ECS/TownEmpty.h"
#include "ECS/TownHomes.h"
#include "ECS/TownPlanning.h"
#include "ECS/TownTurnOrder.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/AreaEffect.h"
#include "Magic/ResourcePiles.h"
#include "Magic/TownBelief.h"
#include "Particles/DrawnEffectSteps.h"
#include "VillagerBreeding.h"
#include "VillagerCreatureReactions.h"
#include "VillagerDiscipleWork.h"
#include "VillagerDisciples.h"
#include "VillagerWorship.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

TownSystem::TownSystem()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	_registry = &Locator::entitiesRegistry::value();
	_connections.push_back(_registry->OnDestroy<Abode>().connect<&TownSystem::OnAbodeGone>(*this));
	// A miracle dispenser is one of its town's buildings too
	_connections.push_back(_registry->OnDestroy<SpellDispenser>().connect<&TownSystem::OnAbodeGone>(*this));
	_connections.push_back(_registry->OnDestroy<Field>().connect<&TownSystem::OnFieldGone>(*this));
	_connections.push_back(_registry->OnDestroy<Villager>().connect<&TownSystem::OnVillagerGone>(*this));
}

TownSystem::~TownSystem()
{
	// The registry may have gone first, taking its signals with it
	if (Locator::entitiesRegistry::has_value() && &Locator::entitiesRegistry::value() == _registry)
	{
		for (auto& connection : _connections)
		{
			connection.release();
		}
	}
}

void TownSystem::OnFieldGone(entt::registry& registry, entt::entity field)
{
	const auto& data = registry.get<const Field>(field);
	const auto& towns = _registry->Context().towns;
	if (const auto town = towns.find(static_cast<uint32_t>(data.town)); town != towns.end() && registry.valid(town->second))
	{
		StructuresChanged(town->second, field);
	}
}

void TownSystem::StructuresChanged(entt::entity town)
{
	StructuresChanged(town, entt::null);
}

void TownSystem::StructuresChanged(entt::entity townEntity, entt::entity leaving)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(townEntity) || !registry.AllOf<Town>(townEntity))
	{
		return;
	}
	auto area = town_area::Emptied();
	const auto widen = [&registry, &area](entt::entity thing) {
		if (const auto* at = registry.TryGet<const Transform>(thing); at != nullptr)
		{
			town_area::Widen(area, map_coords::FromMetres(glm::xz(at->position)), world_objects::SizeOf(thing).radius);
		}
	};
	// Its buildings, the newest first, then its fields
	auto& town = registry.Get<Town>(townEntity);
	for (const auto abode : town.abodes)
	{
		if (registry.Valid(abode))
		{
			widen(abode);
		}
	}
	registry.Each<const Field>([&](entt::entity field, const Field& data) {
		if (field != leaving && data.town == static_cast<int>(town.id))
		{
			widen(field);
		}
	});
	town.area = area;
}

void TownSystem::OnAbodeGone(entt::registry& registry, entt::entity abode)
{
	std::vector<entt::entity> left;
	for (auto [entity, town] : registry.view<Town>().each())
	{
		if (std::erase(town.abodes, abode) != 0)
		{
			left.push_back(entity);
		}
	}
	// Each town that the building left makes its ground again
	for (const auto town : left)
	{
		StructuresChanged(town);
	}
	// A storage pit that goes makes its town let go of any temporary piles it keeps
	const auto* data = registry.try_get<const Abode>(abode);
	if (data == nullptr || !registry.all_of<StoragePit>(abode))
	{
		return;
	}
	const auto& towns = _registry->Context().towns;
	const auto town = towns.find(data->townId);
	if (town == towns.end() || !registry.valid(town->second))
	{
		return;
	}
	const auto* component = registry.try_get<const Town>(town->second);
	if (component != nullptr && std::ranges::any_of(component->temporaryStores, [&registry](entt::entity pile) {
		    return pile != entt::null && registry.valid(pile);
	    }))
	{
		SetStoragePit(town->second);
	}
}

void TownSystem::OnVillagerGone(entt::registry& registry, entt::entity villager)
{
	// One taken from the world while still in a town leaves it, which can leave the town with nobody
	if (const auto* gone = registry.try_get<const Villager>(villager);
	    gone != nullptr && gone->town != entt::null && registry.valid(gone->town))
	{
		if (auto* town = registry.try_get<Town>(gone->town))
		{
			const auto others = PopulationOf(gone->town) - 1;
			town->emptyTownTimer = town_empty::AfterLeaving(town->emptyTownTimer, others);
		}
	}
	for (auto [entity, town] : registry.view<Town>().each())
	{
		std::erase(town.homelessVillagers, villager);
	}
	// One taken from the world without dying leaves its home too, ending the couple it was one of
	for (auto [entity, abode] : registry.view<Abode>().each())
	{
		if (std::ranges::find(abode.couple, villager) != abode.couple.end())
		{
			abode.couple = {entt::null, entt::null};
		}
		std::erase(abode.inhabitants, villager);
	}
	// The towns and worship sites counting it at worship forget it
	ecs::villager_worship::Forget(villager);
	// Its children lose their mother, and its lover forgets it
	for (auto [entity, person] : registry.view<Villager>().each())
	{
		if (person.mother == villager)
		{
			person.mother = entt::null;
		}
	}
	std::vector<entt::entity> forgetting;
	for (auto [entity, lover] : registry.view<Lover>().each())
	{
		if (lover.partner == villager)
		{
			forgetting.push_back(entity);
		}
	}
	registry.remove<Lover>(forgetting.begin(), forgetting.end());
}

namespace
{
/// The room of the game's buildings, from their rows in the buildings' table, and their people coming out
/// Whether a building has been built: one never under construction always has
bool IsBuilt(entt::entity abode)
{
	const auto* progress = Locator::entitiesRegistry::value().TryGet<const BuildProgress>(abode);
	return progress == nullptr || progress->built >= 1.0f;
}

town_homes::Homes GameHomes()
{
	return {
	    .room = [](entt::entity abode) -> std::optional<town_homes::AbodeRoom> {
		    const auto* info = world_objects::AbodeInfoOf(abode);
		    if (info == nullptr)
		    {
			    return std::nullopt;
		    }
		    return town_homes::AbodeRoom {.maxAdults = info->maxVillagersInAbode,
		                                  .maxChildren = info->maxChildrenInAbode,
		                                  .functional = villager_home::IsFunctional(abode),
		                                  .built = IsBuilt(abode),
		                                  .tooCrowded = info->percentTooCrowded};
	    },
	    .leaving = [](entt::entity villager) { villager_home::LeavingHome(villager); },
	    .atHome = [](entt::entity villager) { return Locator::entitiesRegistry::value().AllOf<AtHome>(villager); },
	    .leavingTown = [](entt::entity villager, entt::entity town) { villager_breeding::OrphanChildrenOf(villager, town); }};
}

/// Buildings further off than this are never the nearest
constexpr float k_FarthestAbode = 1e12f;

/// A town with no homes built lets in this many people
uint32_t EmptyTownAllows()
{
	return Locator::infoConstants::value().town.emptyTownAllowedThisManyVillagers;
}
} // namespace

entt::entity TownSystem::FindAbodeWithSpace(entt::entity town, entt::entity villager, float leastScore) const
{
	return town_homes::FindAbodeWithSpace(Locator::entitiesRegistry::value(), GameHomes(), town, villager, leastScore);
}

entt::entity TownSystem::FindClosestTown(const glm::vec3& point) const
{
	// The game's nearest town to a place, of any player and tribe, with no limit on the distance
	return NearestTown(glm::xz(point), std::numeric_limits<float>::max(), [](entt::entity /*town*/) { return true; });
}

namespace
{
/// The players' towns in turn, in the order each gained them, then the neutral towns
std::vector<std::pair<entt::entity, const Town*>> TownsInTurn(const ecs::Registry& registry)
{
	std::vector<std::pair<entt::entity, const Town*>> towns;
	registry.Each<const Town>([&towns](entt::entity entity, const Town& town) { towns.emplace_back(entity, &town); });
	std::ranges::stable_sort(towns, [](const auto& a, const auto& b) {
		const auto rank = [](const Town& town) {
			return std::tuple(town.owner == PlayerNames::NEUTRAL, static_cast<uint8_t>(town.owner), town.gained, town.id);
		};
		return rank(*a.second) < rank(*b.second);
	});
	return towns;
}
} // namespace

entt::entity TownSystem::NearestTown(glm::vec2 point, float reach, const std::function<bool(entt::entity)>& accepts) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	entt::entity closest = entt::null;
	float nearest = reach;
	for (const auto& [entity, town] : TownsInTurn(registry))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (transform == nullptr)
		{
			continue;
		}
		// The game's map distance
		const float distance = gutils::GetDistanceInMetres(point, glm::xz(transform->position));
		if (distance < nearest && accepts(entity))
		{
			nearest = distance;
			closest = entity;
		}
	}
	return closest;
}

entt::entity TownSystem::NearestBuilding(glm::vec2 point) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto towns = TownsInTurn(registry);
	entt::entity closest = entt::null;
	float nearest = k_FarthestAbode;
	for (const auto& [entity, town] : towns)
	{
		for (const auto abode : town->abodes)
		{
			const auto* transform = registry.Valid(abode) ? registry.TryGet<const Transform>(abode) : nullptr;
			if (transform == nullptr)
			{
				continue;
			}
			// The game's map distance
			const float distance = gutils::GetDistanceInMetres(glm::xz(transform->position), point);
			if (distance < nearest)
			{
				nearest = distance;
				closest = abode;
			}
		}
	}
	return closest;
}

bool TownSystem::AddVillagerToTown(entt::entity town, entt::entity villager)
{
	return town_homes::AddVillagerToTown(Locator::entitiesRegistry::value(), GameHomes(), town, villager);
}

void TownSystem::AddVillagerToAbode(entt::entity abode, entt::entity villager)
{
	town_homes::AddVillagerToAbode(Locator::entitiesRegistry::value(), GameHomes(), abode, villager);
}

bool TownSystem::MakeHomeless(entt::entity villager)
{
	return town_homes::MakeHomeless(Locator::entitiesRegistry::value(), GameHomes(), villager);
}

void TownSystem::MoveInAnyway(entt::entity abode, entt::entity villager)
{
	town_homes::MoveInAnyway(Locator::entitiesRegistry::value(), GameHomes(), abode, villager);
}

bool TownSystem::MoveInByBuilding(entt::entity abode, entt::entity villager)
{
	return town_homes::MoveInByBuilding(Locator::entitiesRegistry::value(), GameHomes(), abode, villager, EmptyTownAllows());
}

bool TownSystem::MoveIntoTown(entt::entity town, entt::entity villager)
{
	return town_homes::MoveIntoTown(Locator::entitiesRegistry::value(), GameHomes(), town, villager, EmptyTownAllows());
}

town_homes::NewHome TownSystem::LookForBetterHome(entt::entity villager)
{
	return town_homes::LookForBetterHome(Locator::entitiesRegistry::value(), GameHomes(), villager);
}

bool TownSystem::HasRoomForNewcomer(entt::entity town) const
{
	return town_homes::HasRoomForNewcomer(Locator::entitiesRegistry::value(), GameHomes(), town, EmptyTownAllows());
}

void TownSystem::RemoveDeadVillager(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Get<const Villager>(villager).town;
	villager_breeding::OrphanChildrenOf(villager, town);
	// A death changes what its town wants
	MarkDesiresChanged(town);
	town_homes::RemoveDeadVillager(registry, villager);
	StartEmptyTownTimerIfEmpty(town);
}

void TownSystem::RemoveVillager(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* person = registry.TryGet<Villager>(villager);
	if (person == nullptr || person->town == entt::null)
	{
		return;
	}
	// Worship first, while its town is still known
	ecs::villager_worship::LeaveTown(villager);
	const auto town = person->town;
	town_homes::RemoveVillagerFromTown(registry, GameHomes(), villager);
	StartEmptyTownTimerIfEmpty(town);
	registry.Get<Villager>(villager).mother = entt::null;
}

uint32_t TownSystem::PopulationOf(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	uint32_t people = 0;
	registry.Each<const Villager>([&people, town](const Villager& villager) {
		if (villager.town == town)
		{
			++people;
		}
	});
	return people;
}

void TownSystem::StartEmptyTownTimerIfEmpty(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* component = town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr)
	{
		// The last person to leave starts the countdown to an empty town
		component->emptyTownTimer = town_empty::AfterLeaving(component->emptyTownTimer, PopulationOf(town));
	}
}

std::optional<TownSystemInterface::TemporaryStore> TownSystem::TemporaryStorePile(entt::entity town, glm::vec2 from,
                                                                                  ResourceType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto slot = temporary_store::SlotOf(type);
	if (!slot.has_value() || town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return std::nullopt;
	}
	auto pile = registry.Get<const Town>(town).temporaryStores.at(*slot);
	if (pile == entt::null || !registry.Valid(pile))
	{
		// A new empty pile near the gathering place, the wood a little to one side, at a place clear of fixed things
		auto coords = map_coords::FromMetres(GetCongregationPos(town));
		coords += gutils::GetPosFromAngle(0.0f, temporary_store::OffsetOf(type));
		const auto wanted = map_coords::ToMetres(coords);
		const auto place = FindClearArea(wanted, temporary_store::k_ClearSearch, temporary_store::k_ClearStep,
		                                 temporary_store::k_ClearRadius, ClearAreaFilter::AnyFixed, entt::null)
		                       .value_or(wanted);
		const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(place) : 0.0f;
		pile = archetypes::PotArchetype::CreateEmpty({place.x, height, place.y}, 0.0f, temporary_store::PileOf(type));
		if (pile == entt::null)
		{
			return std::nullopt;
		}
		// A miracle's pile of the neutral player, drawn at a miracle pile's size
		registry.Get<Transform>(pile).scale =
		    glm::vec3(type == ResourceType::Food ? magic::piles::k_MagicFoodScale : magic::piles::k_MagicWoodScale);
		registry.Assign<MagicPile>(pile, type, PlayerNames::NEUTRAL);
		registry.Get<Town>(town).temporaryStores.at(*slot) = pile;
	}
	return TemporaryStore {.pile = pile, .edge = construction::NearestEdgeToPos(pile, from)};
}

void TownSystem::SetStoragePit(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = town != entt::null && registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	for (size_t slot = 0; slot < temporary_store::k_Kinds; ++slot)
	{
		const auto pile = std::exchange(component->temporaryStores.at(slot), entt::null);
		const auto* pot = pile != entt::null && registry.Valid(pile) ? registry.TryGet<const Pot>(pile) : nullptr;
		switch (temporary_store::OnNewStoragePit(pot != nullptr, pot != nullptr ? pot->amount : 0))
		{
		case temporary_store::NewPitStep::None:
			break;
		case temporary_store::NewPitStep::Remove:
			world_objects::Remove(pile);
			break;
		case temporary_store::NewPitStep::Release:
		{
			// It calls those about to come for it, as a pile of the neutral player
			const auto reaction = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).associatedReaction;
			if (reaction != Reaction::None && Locator::reactionSystem::has_value())
			{
				Locator::reactionSystem::value().Create({.initiator = pile,
				                                         .type = reaction,
				                                         .player = PlayerNames::NEUTRAL,
				                                         .position = registry.Get<const Transform>(pile).position});
			}
			break;
		}
		}
		component = registry.TryGet<Town>(town);
		if (component == nullptr)
		{
			return;
		}
	}
}

void TownSystem::TemporaryStoresTurn(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = construction::StoragePitOf(town);
	const bool pitWorks = pit.has_value() && villager_home::IsFunctional(*pit);
	for (size_t slot = 0; slot < temporary_store::k_Kinds; ++slot)
	{
		const auto pile = registry.Get<const Town>(town).temporaryStores.at(slot);
		if (pile == entt::null)
		{
			continue;
		}
		const auto* pot = registry.Valid(pile) ? registry.TryGet<const Pot>(pile) : nullptr;
		switch (temporary_store::AtTownTurn(pot != nullptr, pot != nullptr ? pot->amount : 0, pitWorks))
		{
		case temporary_store::TurnStep::Keep:
			continue;
		case temporary_store::TurnStep::Remove:
			world_objects::Remove(pile);
			break;
		case temporary_store::TurnStep::Forget:
			break;
		}
		registry.Get<Town>(town).temporaryStores.at(slot) = entt::null;
	}
}

void TownSystem::SetTownEmpty(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* impression = registry.TryGet<TownImpression>(town))
	{
		// It starts over in belief: it believes in every player what it keeps for the neutral player, and nothing bores
		// it
		const auto& desires = Locator::infoConstants::value().townDesire;
		std::array<magic::town_belief::DesireDecay, magic::town_belief::k_DesireCount> table {};
		std::ranges::transform(desires, table.begin(), [](const GTownDesireInfo& row) {
			return magic::town_belief::DesireDecay {.toBelief = row.desireToBeliefScale,
			                                        .levelAfterGain = row.desireAffectsBeliefAfter,
			                                        .levelFall = row.desireToBeliefThresholdDecay};
		});
		magic::town_belief::ResetForEmptyTown(impression->belief, table);
		impression->boredom.clear();
	}
	// A town of a player goes back to the neutral player
	if (registry.Get<const Town>(town).owner != PlayerNames::NEUTRAL)
	{
		ClaimTown(town, PlayerNames::NEUTRAL);
	}
}

std::vector<entt::entity> TownSystem::ChildrenOf(entt::entity town, entt::entity mother) const
{
	return town_homes::ChildrenOf(Locator::entitiesRegistry::value(), town, mother);
}

namespace
{
/// Whether a thing stands in the way of ground kept clear
bool Blocks(const Registry& registry, entt::entity thing, TownSystemInterface::ClearAreaFilter filter)
{
	if (filter == TownSystemInterface::ClearAreaFilter::AnyObject)
	{
		return true;
	}
	if (filter == TownSystemInterface::ClearAreaFilter::AnyFixed)
	{
		return registry.AllOf<Fixed>(thing);
	}
	// Of the things that stay put, all but trees
	return registry.AllOf<Fixed>(thing) && !registry.AllOf<Tree>(thing);
}

/// A gathering place is looked for this far round the middle of a town's buildings, in steps, clear this far round
constexpr float k_CongregationSearch = 130.0f;
constexpr float k_CongregationStep = 3.0f;
constexpr float k_CongregationClear = 10.0f;
/// Failing that it is near a building, this far and up to as far again
constexpr float k_CongregationNearLeast = 10.0f;
constexpr float k_CongregationNearRange = 10.0f;
/// With fewer buildings than this, the gathering place counts the town's planned buildings too
constexpr size_t k_CongregationLeastBuildings = 3;
/// A new building this close to the gathering place, past its own size, moves it
constexpr float k_CongregationBuildingGap = 7.5f;
} // namespace

bool TownSystem::CheckForClearArea(glm::vec2 point, float radius, ClearAreaFilter filter, entt::entity ignore) const
{
	if (!Locator::entitiesMap::has_value())
	{
		return true;
	}
	const auto& map = Locator::entitiesMap::value();
	const auto& registry = Locator::entitiesRegistry::value();
	auto cell = map_coords::CellOf(point);
	map_coords::Spiral spiral;
	for (auto count = map_coords::CellSpiralSize(radius); count != 0; --count)
	{
		if (map_coords::InBounds(cell))
		{
			for (const auto thing : map.GetAllInCell(cell))
			{
				const auto* transform = registry.TryGet<const Transform>(thing);
				if (transform == nullptr || thing == ignore)
				{
					continue;
				}
				const float gap = gutils::GetDistanceInMetres(glm::vec2(transform->position.x, transform->position.z), point) -
				                  world_objects::SizeOf(thing).radius;
				if (gap < radius && Blocks(registry, thing, filter))
				{
					return false;
				}
			}
		}
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
	}
	return true;
}

std::optional<glm::vec2> TownSystem::FindClearArea(glm::vec2 point, float searchRadius, float step, float radius,
                                                   ClearAreaFilter filter, entt::entity ignore) const
{
	auto coords = map_coords::FromMetres(point);
	map_coords::Spiral spiral;
	for (auto count = map_coords::IncrementSpiralSize(searchRadius, step); count != 0; --count)
	{
		const auto at = map_coords::ToMetres(coords);
		if (CheckForClearArea(at, radius, filter, ignore))
		{
			return at;
		}
		map_coords::SpiralIncrement(coords, spiral, step);
	}
	return std::nullopt;
}

glm::vec2 TownSystem::GetCongregationPos(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& town = registry.Get<Town>(townEntity);
	if (town.congregationPos.has_value())
	{
		return *town.congregationPos;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	// The town's buildings other than fields, in the order the town keeps them
	std::vector<glm::vec2> buildings;
	for (const auto abode : town.abodes)
	{
		const auto* data = registry.TryGet<const Abode>(abode);
		const auto* transform = registry.TryGet<const Transform>(abode);
		if (data == nullptr || transform == nullptr || infos.at(static_cast<size_t>(data->type)).abodeType == AbodeType::Field)
		{
			continue;
		}
		buildings.emplace_back(transform->position.x, transform->position.z);
	}
	// A town with fewer than three counts its planned buildings too
	if (buildings.size() < k_CongregationLeastBuildings)
	{
		for (const auto planned : town.planned)
		{
			if (const auto* transform = registry.Valid(planned) ? registry.TryGet<const Transform>(planned) : nullptr)
			{
				buildings.emplace_back(transform->position.x, transform->position.z);
			}
		}
	}
	std::optional<glm::vec2> found;
	const auto& townTransform = registry.Get<const Transform>(townEntity);
	glm::vec2 centre {townTransform.position.x, townTransform.position.z};
	if (buildings.size() > 1)
	{
		// The middle of the buildings, on the map's grid; all of them are used up finding it, so failing a clear area
		// there it is near the town itself
		uint32_t x = 0;
		uint32_t z = 0;
		for (const auto& building : buildings)
		{
			const auto coords = map_coords::FromMetres(building);
			x += static_cast<uint32_t>(coords.x);
			z += static_cast<uint32_t>(coords.z);
		}
		const auto count = static_cast<float>(buildings.size());
		const glm::vec2 middle {map_coords::ToMetres(map_coords::FtoL(static_cast<float>(x) / count)),
		                        map_coords::ToMetres(map_coords::FtoL(static_cast<float>(z) / count))};
		found = FindClearArea(middle, k_CongregationSearch, k_CongregationStep, k_CongregationClear,
		                      ClearAreaFilter::BlocksTown, entt::null);
	}
	else if (!buildings.empty())
	{
		centre = buildings.front();
	}
	if (!found.has_value())
	{
		auto& random = Locator::gameRandom::value();
		const float distance = random.GameFloatRand(k_CongregationNearRange) + k_CongregationNearLeast;
		const float angle = random.GameFloatRand(villager_routine::k_TwoPi);
		found = villager_routine::Polar(centre, angle, distance);
	}
	town.congregationPos = found;
	return *found;
}

void TownSystem::BuildingCreated(entt::entity townEntity, glm::vec2 position, float radius)
{
	auto& town = Locator::entitiesRegistry::value().Get<Town>(townEntity);
	if (town.congregationPos.has_value() &&
	    gutils::GetDistanceInMetres(*town.congregationPos, position) - radius < k_CongregationBuildingGap)
	{
		town.congregationPos.reset();
	}
}

bool TownSystem::IsInStateOfEmergency(entt::entity townEntity) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* town = registry.TryGet<const Town>(townEntity);
	return town != nullptr &&
	       villager_routine::IsInStateOfEmergency(town->emergencyTurn, Locator::time::value().GetTurn(),
	                                              Locator::infoConstants::value().town.gameTurnsAfterEmergencyVillagersReact);
}

void TownSystem::SetInStateOfEmergency(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& town = registry.Get<Town>(townEntity);
	const auto turn = Locator::time::value().GetTurn();
	if (town.emergencyTurn != 0 || !Locator::livingActionSystem::has_value())
	{
		town.emergencyTurn = turn;
		return;
	}
	// Everyone living in its buildings who is about something an emergency breaks into goes to gather; those inside, in
	// bed or sitting about are left be
	auto& living = Locator::livingActionSystem::value();
	for (const auto abode : town.abodes)
	{
		const auto* data = registry.TryGet<const Abode>(abode);
		if (data == nullptr)
		{
			continue;
		}
		for (const auto villager : data->inhabitants)
		{
			auto* action = registry.TryGet<LivingAction>(villager);
			if (action != nullptr &&
			    villager_routine::AnswersTownEmergency(
			        living.VillagerGetFinalState(*action),
			        static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Previous)))))
			{
				living.VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::GotoCongregateInTownAfterEmergency,
				                        false);
			}
		}
	}
	// The town counts as in an emergency only once its people have been called
	town.emergencyTurn = turn;
}

namespace
{
/// The town examines one of its buildings for neglect. An empty building loses life when its neglect timer is full.
/// Nobody did this damage, thus no player counts it as a building that the player destroyed.
void NeglectAbode(entt::entity town, entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* info = world_objects::AbodeInfoOf(abode);
	if (info == nullptr)
	{
		return;
	}
	const auto household = town_homes::HouseholdOf(registry, abode);
	const abode_neglect::NeglectFacts facts {.adults = household.adults,
	                                         .children = household.children,
	                                         .maxAdults = info->maxVillagersInAbode,
	                                         .maxChildren = info->maxChildrenInAbode,
	                                         .built = IsBuilt(abode),
	                                         .inScript = registry.AllOf<InScript>(abode),
	                                         .townUninhabitable = registry.Get<const Town>(town).uninhabitable};
	auto& data = registry.Get<Abode>(abode);
	const auto step = abode_neglect::Examine(data.neglect, facts);
	data.neglect = step.timer;
	if (step.losesLife)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Town: empty building #{} loses life from neglect", entt::to_integral(abode));
		world_objects::ReduceLife(abode, info->emptyAbodeLifeReducer);
	}
}
} // namespace

void TownSystem::ProcessTurn()
{
	for (uint8_t player = 0; player < static_cast<uint8_t>(PlayerNames::_COUNT); ++player)
	{
		for (const auto town : TownsInGainedOrder(static_cast<PlayerNames>(player)))
		{
			ProcessTownBuildings(town);
			ProcessTownTurn(town);
		}
	}
}

namespace
{
/// The adults and children that the town counted for its desires this turn
uint32_t CountedPeopleOf(const ecs::Registry& registry, entt::entity town)
{
	const auto* stats = registry.TryGet<const TownStats>(town);
	return stats != nullptr ? stats->adults + stats->children : 0;
}

/// The town's wants that stay over their levels move its player's alignment, by the share of all the players' people
/// that live in it, as the players' turn has counted them so far
void MoveAlignmentByWants(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* wants = registry.TryGet<TownDesire>(townEntity);
	if (wants == nullptr || !registry.AllOf<Town>(townEntity) || !Locator::alignmentSystem::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	std::array<town_desire_alignment::DesireRule, k_TownDesireCount> rules {};
	for (size_t d = 0; d < rules.size(); ++d)
	{
		const auto& row = info.townDesire.at(d);
		rules.at(d) = {.affectsAfter = row.desireAffectsAlignmentAfter,
		               .importance = row.howImportantDesireIsToAlignment,
		               .maxTurns = row.maxTimeForAlignmentChange};
	}
	const auto& counted = registry.Context().peopleCountedInTurn;
	const auto all = std::accumulate(counted.begin(), counted.end(), uint32_t {0});
	const auto change = town_desire_alignment::Change(
	    wants->turnsOverAlignmentLevel, {.boostA = wants->boostA, .desire = wants->desire, .boost = wants->boost}, rules,
	    {.town = CountedPeopleOf(registry, townEntity), .all = all}, info.town.maxAlignmentChangePerGameTurnForDesires);
	auto& alignment = Locator::alignmentSystem::value();
	const auto owner = registry.Get<const Town>(townEntity).owner;
	const float damped = magic::DampAlignmentChange(change, alignment.GetPlayerAlignment(owner));
	if (damped != 0.0f)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Town {}: its wants move player {} by {} (of {} people)",
		                    registry.Get<const Town>(townEntity).id, static_cast<int>(owner), damped, all);
	}
	alignment.AddPendingAlignment(owner, damped);
}
} // namespace

void TownSystem::ProcessPlayerTowns(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& counted = registry.Context().peopleCountedInTurn.at(static_cast<size_t>(player));
	counted = 0;
	for (const auto town : TownsInGainedOrder(player))
	{
		if (!registry.Valid(town))
		{
			continue;
		}
		// First it removes its building sites that have no use
		construction::PruneSites(town);
		if (Locator::influenceSystem::has_value())
		{
			Locator::influenceSystem::value().ProcessTownInfluence(town);
		}
		ProcessTownBuildings(town);
		if (Locator::townDesireSystem::has_value())
		{
			Locator::townDesireSystem::value().ProcessTown(town);
		}
		if (Locator::artefactSystem::has_value())
		{
			Locator::artefactSystem::value().ProcessTown(town);
		}
		ProcessTownTurn(town);
		if (!registry.Valid(town) || !registry.AllOf<Town>(town))
		{
			continue;
		}
		// Its people count for the player whose turn it is, even when another player took it in its turn
		counted += CountedPeopleOf(registry, town);
		// A town taken in its own turn goes to the end of its new player's towns: the old player's towns that come after
		// it take no turn this game turn, and a new player whose turn is still to come gives it a second turn
		if (registry.Get<const Town>(town).owner != player)
		{
			break;
		}
	}
}

std::vector<entt::entity> TownSystem::TownsInGainedOrder(PlayerNames player) const
{
	return town_turns::TownsInGainedOrder(Locator::entitiesRegistry::value(), player);
}

void TownSystem::ProcessTownBuildings(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(townEntity) || !registry.AllOf<Town>(townEntity))
	{
		return;
	}
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	// Every so many turns it passes over its buildings, which age by it; a football pitch never ages
	if (!statistics::TownPassesItsBuildings(turn, Locator::infoConstants::value().town.processAbodeEvery))
	{
		return;
	}
	const auto abodes = registry.Get<const Town>(townEntity).abodes;
	for (const auto abode : abodes)
	{
		// A miracle dispenser ages as any building does. Nobody lives in it, thus it is never empty and not neglected.
		if (auto* dispenser = registry.Valid(abode) ? registry.TryGet<SpellDispenser>(abode) : nullptr; dispenser != nullptr)
		{
			dispenser->age = statistics::AgedOnePass(dispenser->age);
			continue;
		}
		if (const auto* data = registry.TryGet<const Abode>(abode); data == nullptr || data->type == AbodeNumber::FootballPitch)
		{
			continue;
		}
		// A workshop makes its scaffolds before it ages as any other building does
		if (registry.AllOf<Workshop>(abode) && Locator::workshopSystem::has_value())
		{
			Locator::workshopSystem::value().ProcessWorkshop(abode);
		}
		NeglectAbode(townEntity, abode);
		// The loss of life can remove the building
		if (auto* data = registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr)
		{
			data->age = statistics::AgedOnePass(data->age);
		}
	}
}

void TownSystem::ProcessTownTurn(entt::entity townEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(townEntity) || !registry.AllOf<Town>(townEntity))
	{
		return;
	}
	const auto& infos = Locator::infoConstants::value().abode;
	const auto* fire = Locator::fireSystem::has_value() ? &Locator::fireSystem::value() : nullptr;
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	// Every tenth turn a town short of worshippers sends more
	if (Locator::worshipSiteSystem::has_value())
	{
		ecs::villager_worship::TownTurn(townEntity, turn);
	}
	if (!registry.Valid(townEntity))
	{
		return;
	}
	// The attacks on it fade, and are summed for its next turn's desires
	if (auto* attacks = registry.TryGet<TownAggression>(townEntity))
	{
		town_aggression::ProcessTurn(attacks->record, registry.Get<const Town>(townEntity).owner);
	}
	// It puts up at most one of its buildings, or a finished one that went, for mending
	town_planning::MendBest(townEntity);
	const bool inEmergency = IsInStateOfEmergency(townEntity);
	// Out of an emergency, its storage pit or village centre on fire calls one
	bool burning = false;
	for (const auto abode : registry.Get<const Town>(townEntity).abodes)
	{
		const auto* data = registry.TryGet<const Abode>(abode);
		if (inEmergency || fire == nullptr || data == nullptr)
		{
			continue;
		}
		const auto type = infos.at(static_cast<size_t>(data->type)).abodeType;
		if ((type == AbodeType::StoragePit || type == AbodeType::TownCentre) && fire->IsOnFire(abode))
		{
			burning = true;
			break;
		}
	}
	const auto* aggression = registry.TryGet<const TownAggression>(townEntity);
	const auto& town = registry.Get<const Town>(townEntity);
	const auto outcome = villager_routine::EmergencyTurn({
	    .inEmergency = inEmergency,
	    .attackedThisTurn = aggression != nullptr && aggression->record.lastTurn == turn,
	    .storeOrCentreBurning = burning,
	    .worshipShare = town.worshipShare,
	    .worshipShareBefore = town.worshipShareBeforeEmergency,
	});
	if (outcome.call)
	{
		SetInStateOfEmergency(townEntity);
	}
	if (outcome.setWorshipShare.has_value() && Locator::villageTotemSystem::has_value())
	{
		// Its worshippers leave the worship site while it lasts, and come back after
		Locator::villageTotemSystem::value().SetTownShare(townEntity, *outcome.setWorshipShare);
	}
	auto& changed = registry.Get<Town>(townEntity);
	changed.worshipShareBeforeEmergency = outcome.worshipShareBefore;
	if (outcome.end)
	{
		changed.emergencyTurn = 0;
	}
	// Then it judges the creatures it watches
	ecs::villager_creature::TownTurn(townEntity, turn);
	// It forgets its temporary piles that have gone, and removes the empty ones once its storage pit works
	if (registry.Valid(townEntity))
	{
		TemporaryStoresTurn(townEntity);
	}
	// The missionaries preaching in it follow their villagers, or stop
	villager_disciple_work::ProcessMissionaries(townEntity);
	if (!registry.Valid(townEntity))
	{
		return;
	}
	// Its turn of belief: the player it believes in most takes it at once
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().BelieveInTown(townEntity);
	}
	// Its wants that stay unmet move its player's alignment
	MoveAlignmentByWants(townEntity);
	// A mark of changed wants that has been up a whole turn comes down
	auto& marked = registry.Get<Town>(townEntity);
	disciple_work::EndTownTurn(marked.desiresChanged, marked.desiresChangedLastTurn);
	// A town that its last person left becomes empty after its countdown, unless people came back first
	if (marked.emptyTownTimer != 0 &&
	    town_empty::Turn(marked.emptyTownTimer, marked.emptyTownTimer > 1 ? PopulationOf(townEntity) : 0))
	{
		SetTownEmpty(townEntity);
	}
	if (!registry.Valid(townEntity))
	{
		return;
	}
	// A change of its influence marks the borders to be drawn again
	if (Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::value().MarkTownInfluence(townEntity);
	}
	auto& shuffled = registry.Get<Town>(townEntity);
	// Now and then it evens out its homes, moving a man for a woman between two, or one villager
	const auto every = Locator::infoConstants::value().town.shuffleVillagersEvery;
	if (town_homes::EvenOutDue(shuffled.id, turn, every))
	{
		town_homes::EvenOutHomes(registry, GameHomes(), townEntity);
	}
}

void TownSystem::MarkDesiresChanged(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* component = registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr)
	{
		disciple_work::MarkDesiresChanged(component->desiresChanged, component->desiresChangedLastTurn);
	}
}

bool TownSystem::DesiresChanged(entt::entity town) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	return component != nullptr && component->desiresChanged;
}

void TownSystem::ClaimTown(entt::entity town, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* component = registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
	if (component == nullptr || component->owner == player)
	{
		return;
	}
	const auto previous = component->owner;
	// The player at this machine hears a town won from someone else
	if (Locator::playerSystem::has_value())
	{
		const auto local =
		    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE);
		if (player == local && previous != local)
		{
			audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_TakeOverTown_01), std::nullopt);
		}
	}
	// Its people stop worshipping
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().SetTownShare(town, 0.0f);
	}
	// Its people living in its homes give up the work the old player set them to
	villager_disciple::TownClaimed(town);
	// It becomes the player's, gained after the towns they have already
	component->owner = player;
	component->gained = registry.Context().nextTownGained++;
	// Its people count as converted to the player
	if (const auto* stats = registry.TryGet<const TownStats>(town))
	{
		player_statistics::CountConverted(registry, player, stats->adults + stats->children);
	}
	// The miracles it offers go from its old player to its new one
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().TownChangedHands(town, previous);
	}
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().TownChangedHands(town);
	}
	// Its centre's totem turns to its new player
	entt::entity centre = entt::null;
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (centre == entt::null && abode.type == AbodeNumber::TownCentre && abode.townId == component->id)
		{
			centre = entity;
		}
	});
	if (centre != entt::null && Locator::villageTotemSystem::has_value())
	{
		std::vector<entt::entity> totems;
		registry.Each<const VillageTotem>([&](entt::entity totem, const VillageTotem& standing) {
			if (standing.townCentre == centre)
			{
				totems.push_back(totem);
			}
		});
		for (const auto totem : totems)
		{
			Locator::villageTotemSystem::value().AddToPlayer(totem);
		}
	}
	if (Locator::influenceSystem::has_value())
	{
		Locator::influenceSystem::value().BordersChanged();
	}
	// Its people celebrate at its centre, or about the town without one, impressed by the centre itself
	const auto initiator = centre != entt::null ? centre : town;
	const auto* at = registry.TryGet<const Transform>(initiator);
	const glm::vec3 position = at != nullptr ? at->position : glm::vec3(0.0f);
	if (Locator::reactionSystem::has_value())
	{
		const auto* info = centre != entt::null ? world_objects::InfoOf(centre) : nullptr;
		Locator::reactionSystem::value().Create({.initiator = initiator,
		                                         .type = Reaction::ReactToTownCelebration,
		                                         .player = player,
		                                         .position = position,
		                                         .impressiveValue = info != nullptr ? info->impressiveValue : 0.0f});
	}
	// Fireworks and a fountain of the new player's symbols rise from its centre
	if (player != PlayerNames::NEUTRAL && Locator::particleSystem::has_value())
	{
		auto& particles = Locator::particleSystem::value();
		particles.StartSpotVisual(SpotVisualType::TownFireworks, position, std::nullopt, entt::null);
		const auto fountain = particles.StartSpotVisual(SpotVisualType::PlayerIconFountain, position, std::nullopt, entt::null);
		if (fountain != ParticleSystemInterface::k_NoEffect)
		{
			particles.SetPlayer(fountain, static_cast<int>(player));
		}
	}
	// In a network game the new player counts a town taken over, for the winning condition
	if (Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().AddToCondition(player, game_end::Condition::TownsTakenOver, 1);
	}
}

void TownSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::Save(out, _beliefSymbols);
}

bool TownSystem::LoadState(reflection::BinaryReader& in)
{
	// The effects of the symbols come back with the particles, so each village centre keeps its own effect and does not
	// start another
	std::unordered_map<entt::entity, uint32_t> symbols;
	reflection::Load(in, symbols);
	if (in.Failed())
	{
		return false;
	}
	_beliefSymbols = std::move(symbols);
	return true;
}

void TownSystem::UpdateFrame(std::chrono::milliseconds frameRealTime, bool landDrawn)
{
	// The symbols step by a whole turn's time at the pace of the screen's refresh
	const auto step =
	    particles::drawn_steps::TurnStepSeconds(_beliefPace, frameRealTime, TimeSystemInterface::k_TurnDuration, landDrawn);
	if (!Locator::particleSystem::has_value() || _registry == nullptr)
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	// A centre gone takes its symbols with it
	std::erase_if(_beliefSymbols, [this, &particles](const auto& entry) {
		if (_registry->Valid(entry.first))
		{
			return false;
		}
		particles.Delete(entry.second);
		return true;
	});
	_registry->Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.type != AbodeNumber::TownCentre)
		{
			return;
		}
		// A town centre shows the symbols from when its town finishes it
		const auto* progress = _registry->TryGet<const BuildProgress>(entity);
		if (!ecs::town_centre_parts::ShowsBeliefSymbols(progress != nullptr ? std::optional(progress->built) : std::nullopt))
		{
			return;
		}
		auto found = _beliefSymbols.find(entity);
		if (found == _beliefSymbols.end() || !particles.IsRunning(found->second))
		{
			const auto effect = particles.Start(ParticleType::TownBelief, glm::vec3(0.0f), 1.0f);
			if (effect == ParticleSystemInterface::k_NoEffect)
			{
				return;
			}
			particles.AddTarget(effect, entity);
			found = _beliefSymbols.insert_or_assign(entity, effect).first;
		}
		if (step.has_value())
		{
			particles.ProcessByFrame(found->second, *step);
		}
	});
}
