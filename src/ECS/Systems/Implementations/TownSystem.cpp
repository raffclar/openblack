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
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Breeding.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/DiscipleWorkRules.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBreeding.h"
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

void TownSystem::OnAbodeGone(entt::registry& registry, entt::entity abode)
{
	for (auto [entity, town] : registry.view<Town>().each())
	{
		std::erase(town.abodes, abode);
	}
}

void TownSystem::OnVillagerGone(entt::registry& registry, entt::entity villager)
{
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
	const auto& registry = Locator::entitiesRegistry::value();

	entt::entity result = entt::null;
	auto closest = std::numeric_limits<float>::infinity();

	registry.Each<const Town, const Transform>(
	    [&point, &result, &closest](entt::entity entity, [[maybe_unused]] auto& town, [[maybe_unused]] auto& transform) {
		    const auto delta = point - transform.position;
		    const float distance2 = glm::dot(delta, delta);
		    if (distance2 < closest)
		    {
			    closest = distance2;
			    result = entity;
		    }
	    });

	return result;
}

entt::entity TownSystem::NearestBuilding(glm::vec2 point) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	// The players' towns in turn, in the order each gained them, then the neutral towns
	std::vector<std::pair<entt::entity, const Town*>> towns;
	registry.Each<const Town>([&towns](entt::entity entity, const Town& town) { towns.emplace_back(entity, &town); });
	std::ranges::stable_sort(towns, [](const auto& a, const auto& b) {
		const auto rank = [](const Town& town) {
			return std::tuple(town.owner == PlayerNames::NEUTRAL, static_cast<uint8_t>(town.owner), town.gained, town.id);
		};
		return rank(*a.second) < rank(*b.second);
	});
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
			const float distance = glm::distance(glm::xz(transform->position), point);
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
				const float gap = glm::distance(glm::vec2(transform->position.x, transform->position.z), point) -
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
	// TODO(villagers): a town with fewer than three counts its planned buildings too; openblack has none yet
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
	if (town.congregationPos.has_value() && glm::distance(position, *town.congregationPos) - radius < k_CongregationBuildingGap)
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
			if (action != nullptr && villager_routine::AnswersTownEmergency(living.VillagerGetFinalState(*action)))
			{
				living.VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::GotoCongregateInTownAfterEmergency,
				                        false);
			}
		}
	}
	// The town counts as in an emergency only once its people have been called
	town.emergencyTurn = turn;
}

void TownSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> towns;
	registry.Each<const Town>([&towns](entt::entity town, const Town&) { towns.push_back(town); });
	const auto& infos = Locator::infoConstants::value().abode;
	const auto* fire = Locator::fireSystem::has_value() ? &Locator::fireSystem::value() : nullptr;
	for (const auto townEntity : towns)
	{
		if (IsInStateOfEmergency(townEntity))
		{
			// TODO(villagers): an attack on the town renews it, and its worship stops while it lasts
			continue;
		}
		// Its storage pit or village centre on fire calls one; otherwise one that ran its time is over
		bool burning = false;
		for (const auto abode : registry.Get<const Town>(townEntity).abodes)
		{
			const auto* data = registry.TryGet<const Abode>(abode);
			if (data == nullptr || fire == nullptr)
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
		if (burning)
		{
			SetInStateOfEmergency(townEntity);
			continue;
		}
		registry.Get<Town>(townEntity).emergencyTurn = 0;
	}
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	// Every tenth turn each town short of worshippers sends more
	if (Locator::worshipSiteSystem::has_value())
	{
		for (const auto townEntity : towns)
		{
			if (registry.Valid(townEntity))
			{
				ecs::villager_worship::TownTurn(townEntity, turn);
			}
		}
	}
	// Now and then each town evens out its homes, moving a man for a woman between two, or one villager
	const auto every = Locator::infoConstants::value().town.shuffleVillagersEvery;
	const auto homes = GameHomes();
	for (const auto townEntity : towns)
	{
		if (registry.Valid(townEntity) && town_homes::EvenOutDue(registry.Get<const Town>(townEntity).id, turn, every))
		{
			town_homes::EvenOutHomes(registry, homes, townEntity);
		}
	}
	// The missionaries preaching in the towns follow their villagers, or stop; then a mark of changed wants that has
	// been up a whole turn comes down
	villager_disciple_work::ProcessMissionaries();
	for (const auto townEntity : towns)
	{
		if (!registry.Valid(townEntity))
		{
			continue;
		}
		auto& town = registry.Get<Town>(townEntity);
		disciple_work::EndTownTurn(town.desiresChanged, town.desiresChangedLastTurn);
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
		const auto local = Locator::playerSystem::value().GetLocalPlayer();
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
}

void TownSystem::UpdateFrame(float gameSeconds)
{
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
		particles.ProcessByFrame(found->second, gameSeconds);
	});
}
