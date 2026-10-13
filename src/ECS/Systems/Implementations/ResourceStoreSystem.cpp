/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The stores of food and wood. A storage pit stores anything, in its one pile of food and its five of wood; a pile
// stores only through the pit it is part of. Giving a town's store a resource counts for the giver: their alignment
// moves by how much less the town now wants it, and the town believes in them by that, less when they took that
// resource from it lately and less again when the town isn't theirs. Taking from a store counts against the town's
// owner and is remembered of the taker.

#define LOCATOR_IMPLEMENTATIONS

#include "ResourceStoreSystem.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <limits>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Common/GUtilsDistance.h"
#include "Common/MachineClock.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourceLastTaken.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Map.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/StoreRules.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerWorkRules.h"
#include "ECS/WorldObjects.h"
#include "ECS/WorshipSiteConstruction.h"
#include "ECS/WorshipSites.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/AreaEffect.h"
#include "Magic/ResourcePiles.h"
#include "Magic/TownBelief.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace piles = openblack::magic::piles;

namespace
{
/// The in-game bank's wood mulch, four samples played one after another as wood goes into a store
/// The game's map is 512 cells each way
constexpr int k_MapCells = 512;
constexpr uint32_t k_MulchSample = 155;
constexpr uint32_t k_MulchSamples = 4;
/// The piles a storage pit holds its food and its wood in, for how much one can hold
constexpr auto k_PitFoodPile = PotInfo::StoragePitFoodPile;
constexpr auto k_PitWoodPile = PotInfo::WoodPile_1;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const GPotInfo& PotInfoOf(PotInfo type)
{
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(type));
}

uint32_t TickMs()
{
	return machine_clock::Ticks();
}

void PlayInGame(uint32_t sample, glm::vec3 position)
{
	if (Locator::audio::has_value())
	{
		audio::PlayGameSoundEffect(entt::hashed_string(fmt::format("InGame.sad/{}", sample).c_str()).value(), position);
	}
}

/// The thud of a pile, where it is
void Thud(ResourceType type, uint32_t amount, glm::vec3 position)
{
	PlayInGame(piles::PileSoundSample(type, amount, TickMs()), position);
}

/// The pile a storage pit keeps its food or wood in, made empty where the pit's model says if it has none
entt::entity StorePile(entt::entity pit, entt::entity& slot, PotInfo type, size_t place)
{
	auto& registry = Entities();
	if (registry.Valid(slot))
	{
		return slot;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& mesh = registry.Get<const Mesh>(pit);
	if (!meshes.Contains(mesh.id))
	{
		return entt::null;
	}
	const auto& metrics = meshes.Handle(mesh.id)->GetExtraMetrics();
	if (place >= metrics.size())
	{
		return entt::null;
	}
	// Turned as the pit is, which is turned about the vertical
	const auto& transform = registry.Get<const Transform>(pit);
	const float yaw = std::atan2(transform.rotation[0].z, transform.rotation[0].x);
	const auto offset = glm::vec3(glm::mat4(transform.rotation) * metrics.at(place)[3]);
	slot = ecs::archetypes::PotArchetype::CreateEmpty(transform.position + offset, yaw, type);
	return slot;
}

/// A building with a site for its builders, going up or being mended, which takes wood brought for it onto its newest
/// site, while one takes it
bool IsBuildingSite(entt::entity object)
{
	return ecs::construction::BroughtWoodSite(object) != nullptr;
}

/// The town an abode belongs to
entt::entity TownOf(entt::entity store)
{
	auto& registry = Entities();
	const auto* abode = registry.TryGet<const Abode>(store);
	entt::entity town = entt::null;
	if (abode != nullptr)
	{
		registry.Each<const Town>([&](entt::entity entity, const Town& candidate) {
			if (candidate.id == abode->townId)
			{
				town = entity;
			}
		});
	}
	return town;
}

TownDesireInfo DesireFor(ResourceType type)
{
	return type == ResourceType::Food ? TownDesireInfo::ForFood : TownDesireInfo::ForWood;
}

float Desire(entt::entity town, ResourceType type)
{
	return Locator::townDesireSystem::has_value() ? Locator::townDesireSystem::value().RecomputeDesire(town, DesireFor(type))
	                                              : 0.0f;
}

/// A player's alignment moves by a change to a store, scaled as giving or taking and damped by how far it already is
/// that way
void MoveAlignment(PlayerNames player, int64_t amount, float value)
{
	if (!Locator::alignmentSystem::has_value() || player == PlayerNames::NEUTRAL)
	{
		return;
	}
	const auto& town = Locator::infoConstants::value().town;
	const float multiplier = amount < 1 ? town.takeResourceAligmnetChangeMultiplier : town.giveResourceAligmnetChangeMultiplier;
	auto& alignment = Locator::alignmentSystem::value();
	alignment.AddPendingAlignment(player, magic::DampAlignmentChange(value * multiplier, alignment.GetPlayerAlignment(player)));
}

uint32_t& CountOf(Abode& abode, ResourceType type)
{
	return type == ResourceType::Food ? abode.foodAmount : abode.woodAmount;
}

bool IsMushroom(MobileObjectInfo type)
{
	return type == MobileObjectInfo::Champi || type == MobileObjectInfo::MagicMushroom || type == MobileObjectInfo::Toadstool;
}

/// Whether a point is on the land's map
bool InMap(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto extent = Locator::terrainSystem::value().GetExtent();
	return point.x >= extent.minimum.x && point.z >= extent.minimum.y && point.x < extent.maximum.x &&
	       point.z < extent.maximum.y;
}

float LandHeight(glm::vec2 xz)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(xz) : 0.0f;
}

/// Whether a point is in the water, as off the land is
bool IsWater(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value() || point.x < 0.0f || point.z < 0.0f)
	{
		return true;
	}
	const auto cell = piles::CellOf({point.x, point.z});
	if (cell.x >= k_MapCells || cell.y >= k_MapCells)
	{
		return true;
	}
	const auto* landCell = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
	return landCell == nullptr || landCell->properties.hasWater != 0;
}

/// The half width and depth of an entity's model, unscaled
glm::vec2 HalfExtentOf(const Mesh& mesh)
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh.id))
	{
		return glm::vec2(0.0f);
	}
	const auto size = meshes.Handle(mesh.id)->GetBoundingBox().Size();
	return glm::vec2(size.x, size.z) * 0.5f;
}

/// A worship site's food pot, the store of its food; none for anything else or a site without one
Pot* WorshipSitePot(entt::entity site)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* data = registry.Valid(site) ? registry.TryGet<const WorshipSite>(site) : nullptr;
	if (data == nullptr || data->foodPot == entt::null || !registry.Valid(data->foodPot))
	{
		return nullptr;
	}
	return registry.TryGet<Pot>(data->foodPot);
}

bool IsWorshipSite(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(thing) && registry.AllOf<WorshipSite>(thing);
}

/// A worship site going up is a building site, taking wood brought for it onto the one pile of its newest town's site,
/// while one takes it
bool IsWorshipSiteGoingUp(entt::entity site)
{
	return ecs::construction::BroughtWoodSite(site) != nullptr;
}

/// A worship site takes food into its pot, as much as it is given, and while it goes up wood onto its building site's
/// pile, made for the first wood brought
uint32_t AddToWorshipSite(ResourceStoreSystem& stores, entt::entity site, ResourceType type, uint32_t amount, bool poisoned)
{
	namespace wsc = ecs::worship_site_construction;
	switch (wsc::WhereGiven(type, IsWorshipSiteGoingUp(site)))
	{
	case wsc::Destination::SitePile:
	{
		auto* building = ecs::construction::BroughtWoodSite(site);
		return building != nullptr ? stores.AddToSite(site, *building, type, amount, std::nullopt) : 0u;
	}
	case wsc::Destination::FoodPot:
	{
		auto* pot = WorshipSitePot(site);
		if (pot == nullptr)
		{
			return 0;
		}
		pot->amount += amount;
		pot->poisoned = pot->poisoned || poisoned;
		return amount;
	}
	case wsc::Destination::Nowhere:
		break;
	}
	return 0;
}

/// A worship site gives food from its pot, and while it goes up wood from its building site's pile
uint32_t TakeFromWorshipSite(entt::entity site, ResourceType type, uint32_t amount)
{
	namespace wsc = ecs::worship_site_construction;
	auto& registry = Locator::entitiesRegistry::value();
	Pot* pot = nullptr;
	switch (wsc::WhereGiven(type, IsWorshipSiteGoingUp(site)))
	{
	case wsc::Destination::SitePile:
	{
		const auto* building = ecs::construction::BroughtWoodSite(site);
		const auto pile = building != nullptr ? building->piles.front() : entt::null;
		pot = registry.Valid(pile) ? registry.TryGet<Pot>(pile) : nullptr;
		break;
	}
	case wsc::Destination::FoodPot:
		pot = WorshipSitePot(site);
		break;
	case wsc::Destination::Nowhere:
		break;
	}
	if (pot == nullptr)
	{
		return 0;
	}
	const auto taken = std::min(amount, pot->amount);
	pot->amount -= taken;
	return taken;
}
} // namespace

ResourceStoreSystemInterface::ObjectResource ResourceStoreSystem::ResourceOf(entt::entity object) const
{
	auto& registry = Entities();
	if (!registry.Valid(object) || !Locator::infoConstants::has_value())
	{
		return {};
	}
	const auto& info = Locator::infoConstants::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	const float scale = transform != nullptr ? transform->scale.x : 1.0f;
	const float life = ecs::world_objects::LifeOf(object);
	if (const auto* tree = registry.TryGet<const Tree>(object))
	{
		// A forest miracle's tree is worth more than an ordinary one
		const auto* magic = registry.TryGet<const MagicTree>(object);
		const float multiplier = magic != nullptr ? magic->woodMultiplier : 1.0f;
		const float balance = registry.Context().mapScriptGlobals.landBalance.at(5);
		return {ResourceType::Wood,
		        ecs::store_rules::TreeWood(life, multiplier, info.tree.at(static_cast<size_t>(tree->type)).woodValue, scale,
		                                   balance)};
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(object))
	{
		return {ResourceType::Wood, ecs::store_rules::DeadTreeWood(
		                                scale, info.tree.at(static_cast<size_t>(dead->type)).woodValue, dead->woodMultiplier)};
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(object))
	{
		const auto& row = info.mobileStatic.at(static_cast<size_t>(still->type));
		if (!physics_classes::IsFenceModel(row.meshId))
		{
			return {};
		}
		return {ResourceType::Wood, ecs::store_rules::FenceWood(life, row.woodValue, scale)};
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		if (!IsMushroom(mobile->type))
		{
			return {};
		}
		const auto food = info.mobileObject.at(static_cast<size_t>(mobile->type)).foodValue;
		// Of the mushrooms only the toadstool is poisoned
		return {ResourceType::Food, food > 0.0f ? static_cast<uint32_t>(food) : 0u,
		        mobile->type == MobileObjectInfo::Toadstool};
	}
	if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto& row = info.animal.at(static_cast<size_t>(animal->type));
		return {ResourceType::Food, ecs::store_rules::AnimalFood(row.foodValue, static_cast<uint32_t>(row.foodType))};
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return {PotInfoOf(pot->type).resourceType, pot->amount, pot->poisoned};
	}
	return {};
}

std::optional<entt::entity> ResourceStoreSystem::StoreOf(entt::entity pile) const
{
	std::optional<entt::entity> store;
	Entities().Each<const StoragePit>([pile, &store](entt::entity pit, const StoragePit& data) {
		if (data.foodPile == pile || std::ranges::find(data.woodPiles, pile) != data.woodPiles.end())
		{
			store = pit;
		}
	});
	// A worship site's food pot is its store
	Entities().Each<const WorshipSite>([pile, &store](entt::entity site, const WorshipSite& data) {
		if (data.foodPot == pile)
		{
			store = site;
		}
	});
	return store;
}

bool ResourceStoreSystem::IsStore(entt::entity store, ResourceType type) const
{
	auto& registry = Entities();
	if (!registry.Valid(store) || (type != ResourceType::Food && type != ResourceType::Wood && type != ResourceType::Any))
	{
		return false;
	}
	if (registry.AllOf<StoragePit, Abode>(store))
	{
		return true;
	}
	// A worship site stores food in its pot, and wood on its building site while it goes up
	if (IsWorshipSite(store))
	{
		return ecs::worship_site_construction::Stores(type, IsWorshipSiteGoingUp(store));
	}
	// A building going up stores wood on its site
	if (IsBuildingSite(store))
	{
		return ecs::store_rules::BuildingSiteStores(type);
	}
	// A pile stores only its own resource, and only through the store it is part of
	// TODO(stores): workshops store wood; openblack has no workshops yet
	if (const auto* pot = registry.TryGet<const Pot>(store))
	{
		const auto own = PotInfoOf(pot->type).resourceType;
		const auto structure = StoreOf(store);
		return structure.has_value() && (type == own || type == ResourceType::Any) && IsStore(*structure, type);
	}
	return false;
}

uint32_t ResourceStoreSystem::AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned)
{
	auto& registry = Entities();
	auto* pot = registry.TryGet<Pot>(pile);
	if (pot == nullptr)
	{
		return 0;
	}
	const auto& info = PotInfoOf(pot->type);
	if (piles::Thuds(pot->type))
	{
		Thud(type, amount, registry.Get<const Transform>(pile).position);
	}
	const auto taken = piles::AmountTaken(pot->amount, amount, pot->maxAmount, piles::IsCapped(info.nextPotForResource));
	pot->amount += taken;
	// A poisoned gift poisons the pile, which stays poisoned whatever is added after
	pot->poisoned = pot->poisoned || poisoned;
	return taken;
}

uint32_t ResourceStoreSystem::FillPit(entt::entity store, ResourceType type, uint32_t amount, bool poisoned)
{
	auto& registry = Entities();
	auto* abode = registry.TryGet<Abode>(store);
	auto* pit = registry.TryGet<StoragePit>(store);
	if (abode == nullptr || pit == nullptr)
	{
		return 0;
	}
	const auto& abodeInfo = Locator::infoConstants::value().abode.at(static_cast<size_t>(abode->type));
	uint32_t left = amount;
	if (type == ResourceType::Food)
	{
		// Its one pile of food
		const auto pile = StorePile(store, pit->foodPile, abodeInfo.potForResourceFood, pit->woodPiles.size());
		if (pile != entt::null && left > 0)
		{
			left -= AddToPile(pile, type, left, poisoned);
		}
	}
	else
	{
		// Its piles of wood, each in turn
		size_t place = 0;
		for (auto potType = abodeInfo.potForResourceWood;
		     potType != PotInfo::_COUNT && place < pit->woodPiles.size() && left > 0;
		     potType = PotInfoOf(potType).nextPotForResource, ++place)
		{
			const auto pile = StorePile(store, pit->woodPiles.at(place), potType, place);
			if (pile != entt::null)
			{
				left -= AddToPile(pile, type, left, poisoned);
			}
		}
	}
	return amount - left;
}

uint32_t ResourceStoreSystem::AddToStore(entt::entity store, ResourceType type, uint32_t amount,
                                         std::optional<PlayerNames> giver, bool poisoned, std::optional<glm::vec3> at)
{
	auto& registry = Entities();
	// A store's pile passes what it is given to its store
	if (const auto structure = registry.AllOf<Pot>(store) ? StoreOf(store) : std::nullopt)
	{
		return AddToStore(*structure, type, amount, giver, poisoned, at);
	}
	if (IsWorshipSite(store))
	{
		return AddToWorshipSite(*this, store, type, amount, poisoned);
	}
	// A building going up takes wood onto its site, where the builders take it from
	if (IsBuildingSite(store))
	{
		return ecs::store_rules::BuildingSiteStores(type) ? AddResource(store, ResourceType::Wood, amount, at) : 0u;
	}
	auto* abode = registry.TryGet<Abode>(store);
	if (abode == nullptr || !registry.AllOf<StoragePit>(store) || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	const auto town = TownOf(store);
	// A store stocked from empty changes what its town wants
	if (CountOf(*abode, type) == 0 && amount != 0 && town != entt::null && Locator::townSystem::has_value())
	{
		Locator::townSystem::value().MarkDesiresChanged(town);
	}
	const auto added = FillPit(store, type, amount, poisoned);
	if (!giver.has_value() || town == entt::null)
	{
		CountOf(*abode, type) += added;
		return added;
	}
	// The town wants less of what it was given; that, against how lately the giver took it, is what the gift counts for
	const float before = Desire(town, type);
	CountOf(*abode, type) += added;
	const float after = Desire(town, type);
	const auto& townInfo = Locator::infoConstants::value().town;
	const auto* lastTaken = registry.TryGet<const ResourceLastTaken>(town);
	const auto now = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	const auto taken =
	    lastTaken != nullptr ? lastTaken->turn.at(static_cast<size_t>(*giver)).at(static_cast<size_t>(type)) : std::nullopt;
	float value = ecs::store_rules::LastTakenModifier(taken, now, townInfo.maxGameturnsForBeliefAfterRemovingFromStoragePit) *
	              (before - after);
	MoveAlignment(*giver, added, value);
	if (registry.Get<const Town>(town).owner != *giver)
	{
		value *= townInfo.multiplierForNonOwnerAddingResource;
	}
	if (auto* impression = registry.TryGet<TownImpression>(town))
	{
		magic::town_belief::Add(impression->belief, *giver, value * townInfo.multiplierForAddingResourceToTown);
	}
	// TODO(stores): the giver's creature may copy the giving; openblack's creature has no copying of gifts yet
	return added;
}

uint32_t ResourceStoreSystem::RemovedFromStore(entt::entity store, ResourceType type, uint32_t amount,
                                               std::optional<PlayerNames> taker)
{
	auto& registry = Entities();
	auto* abode = registry.TryGet<Abode>(store);
	if (abode == nullptr)
	{
		return 0;
	}
	const auto town = TownOf(store);
	auto& count = CountOf(*abode, type);
	if (town == entt::null)
	{
		const auto removed = std::min(count, amount);
		count -= removed;
		return removed;
	}
	const float before = Desire(town, type);
	const auto removed = std::min(count, amount);
	count -= removed;
	if (!taker.has_value())
	{
		return removed;
	}
	// The taker is remembered, and the town's owner is blamed for what the town now wants
	auto& lastTaken = registry.AllOf<ResourceLastTaken>(town) ? registry.Get<ResourceLastTaken>(town)
	                                                          : registry.Assign<ResourceLastTaken>(town);
	lastTaken.turn.at(static_cast<size_t>(*taker)).at(static_cast<size_t>(type)) =
	    Locator::time::has_value() ? std::optional(Locator::time::value().GetTurn()) : std::nullopt;
	const float after = Desire(town, type);
	MoveAlignment(registry.Get<const Town>(town).owner, -static_cast<int64_t>(amount), before - after);
	return removed;
}

uint32_t ResourceStoreSystem::TakeFromPit(entt::entity store, ResourceType type, uint32_t amount,
                                          std::optional<PlayerNames> taker)
{
	auto& registry = Entities();
	auto* pit = registry.TryGet<StoragePit>(store);
	if (pit == nullptr)
	{
		return 0;
	}
	uint32_t taken = 0;
	const auto takeFrom = [&](entt::entity pile) {
		if (amount == 0 || !registry.Valid(pile))
		{
			return;
		}
		auto& pot = registry.Get<Pot>(pile);
		const auto from = std::min(amount, pot.amount);
		pot.amount -= from;
		taken += from;
		amount -= from;
	};
	if (type == ResourceType::Food)
	{
		takeFrom(pit->foodPile);
	}
	else if (type == ResourceType::Wood)
	{
		// The last of its wood piles first
		for (auto pile = pit->woodPiles.rbegin(); pile != pit->woodPiles.rend(); ++pile)
		{
			takeFrom(*pile);
		}
	}
	if (taken != 0)
	{
		RemovedFromStore(store, type, taken, taker);
	}
	return taken;
}

uint32_t ResourceStoreSystem::TakeFromPile(entt::entity pile, ResourceType type, uint32_t amount,
                                           std::optional<PlayerNames> taker)
{
	auto& registry = Entities();
	auto* pot = registry.TryGet<Pot>(pile);
	if (pot == nullptr)
	{
		return 0;
	}
	const auto store = StoreOf(pile);
	if (!store.has_value())
	{
		// A pile on its own goes once it has nothing left
		const auto taken = std::min(amount, pot->amount);
		pot->amount -= taken;
		if (pot->amount == 0)
		{
			ecs::world_objects::Remove(pile);
		}
		return taken;
	}
	// What is over what the store's piles can hold isn't this pile's to give
	const auto* abode = registry.TryGet<const Abode>(*store);
	const bool wood = type == ResourceType::Wood;
	const auto held = abode != nullptr ? (wood ? abode->woodAmount : abode->foodAmount) : 0u;
	const auto over =
	    ecs::store_rules::AmountOverMaximum(wood, held, PotInfoOf(wood ? k_PitWoodPile : k_PitFoodPile).maxAmountInPot);
	const auto asked = ecs::store_rules::AskedOfPile(amount, over);
	uint32_t taken = 0;
	if (asked != 0)
	{
		taken = std::min(asked, pot->amount);
		pot->amount -= taken;
	}
	if (taken != 0)
	{
		RemovedFromStore(*store, type, taken, taker);
	}
	// What is still wanted comes from the store's other piles
	if (taken < amount)
	{
		taken += TakeFromPit(*store, type, amount - taken, taker);
	}
	return taken;
}

bool ResourceStoreSystem::TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver)
{
	auto& registry = Entities();
	if (!registry.Valid(store) || !registry.Valid(object))
	{
		return false;
	}
	const auto resource = ResourceOf(object);
	if (resource.type == ResourceType::None || !IsStore(store, resource.type))
	{
		return false;
	}
	// TODO(stores): a thing thrown into a storage pit by the local player shows the help for giving; openblack has no
	// help system yet
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto at = transform != nullptr ? transform->position : glm::vec3(0.0f);
	const auto added = AddToStore(store, resource.type, resource.amount, giver, resource.poisoned, at);
	// TODO(stores): the advisor's resource-drop sound for the local player; openblack has no advisor yet
	static_cast<void>(added);
	// Wood that isn't a pot mulches as it goes in
	if (resource.type == ResourceType::Wood && !registry.AllOf<Pot>(object))
	{
		_mulch = (_mulch + 1) % k_MulchSamples;
		PlayInGame(k_MulchSample + _mulch, at);
	}
	ecs::world_objects::LeaveGhost(object);
	ecs::world_objects::Remove(object);
	// A storage pit calls its people to what was put in it
	if (registry.AllOf<StoragePit>(store) && Locator::reactionSystem::has_value())
	{
		const auto& position = registry.Get<const Transform>(store).position;
		Locator::reactionSystem::value().Create({.initiator = store,
		                                         .type = Reaction::ReactToHandPuttingStuffInStoragePit,
		                                         .player = giver.value_or(PlayerNames::NEUTRAL),
		                                         .position = position});
	}
	return true;
}

bool ResourceStoreSystem::PourAt(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player,
                                 bool poisoned)
{
	if (amount == 0 || (type != ResourceType::Food && type != ResourceType::Wood) || !InMap(point))
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 at {point.x, point.z};
	const auto near = [&](entt::entity entity, float multiplier) {
		const auto& transform = registry.Get<const Transform>(entity);
		float radius = piles::RadiusOnGround(HalfExtentOf(registry.Get<const Mesh>(entity)), transform.scale.x);
		// A pile of food reaches as far as it is raised
		if (const auto* pot = registry.TryGet<const Pot>(entity); pot != nullptr && type == ResourceType::Food)
		{
			radius *= piles::ProportionRaised(type, pot->amount, PotInfoOf(pot->type).maxAmountInPot);
		}
		// A worship site is measured from its middle, which stands well out from where the site is set down; everything
		// else from where it stands
		const auto from = IsWorshipSite(entity)
		                      ? ecs::worship_site::CentreOnLand(transform.position, transform.rotation, transform.scale.x)
		                      : glm::vec2(transform.position.x, transform.position.z);
		return piles::WithinReach(at, from, radius, multiplier);
	};

	const auto giver = player != PlayerNames::NEUTRAL ? std::optional(player) : std::nullopt;
	uint32_t left = amount;
	const auto origin = map_coords::CellOf(at);
	for (const auto& step : piles::k_SearchCells)
	{
		if (left == 0)
		{
			break;
		}
		// Everything in the cell, as the cell keeps it: whatever stores this, or is a pile of it, takes what it will
		for (const auto entity : Locator::entitiesMap::value().GetAllInCell(origin + step))
		{
			if (left == 0)
			{
				break;
			}
			// What a hand holds isn't on the land
			if (!registry.Valid(entity) || registry.AllOf<InHand>(entity))
			{
				continue;
			}
			// Anything that stores it (a storage pit, a worship site's food, a building going up's wood), but not a
			// store's own piles, which take it as piles do
			if (!registry.AllOf<Pot>(entity) && IsStore(entity, type))
			{
				if (near(entity, piles::k_StoreReachMultiplier))
				{
					left -= AddToStore(entity, type, left, giver, poisoned, point);
				}
			}
			else if (const auto* pot = registry.TryGet<const Pot>(entity);
			         pot != nullptr && registry.AllOf<ResourcePile>(entity) && PotInfoOf(pot->type).resourceType == type)
			{
				if (near(entity, piles::k_PotReachMultiplier))
				{
					left -= AddToPile(entity, type, left, poisoned);
				}
			}
		}
	}
	if (left == 0 || IsWater(point))
	{
		return left != amount;
	}

	// What is left makes a new pile, which thuds as it lands
	const auto potType = type == ResourceType::Food ? PotInfo::MagicFood : PotInfo::MagicWood;
	point.y = LandHeight(at);
	const auto created = ecs::archetypes::PotArchetype::Create(point, 0.0f, potType, static_cast<int32_t>(left));
	if (created == entt::null)
	{
		return left != amount;
	}
	registry.Get<Transform>(created).scale =
	    glm::vec3(type == ResourceType::Food ? piles::k_MagicFoodScale : piles::k_MagicWoodScale);
	registry.Assign<MagicPile>(created, type, player);
	registry.Get<Pot>(created).poisoned = poisoned;
	Thud(type, left, point);
	// A power-up's food speeds up the people who take from the new pile, and sparkles over it for as long as it lasts
	auto& pile = registry.Get<ResourcePile>(created);
	if (speedUp && type == ResourceType::Food)
	{
		pile.speedUp = true;
		if (Locator::particleSystem::has_value())
		{
			pile.speedUpVisual =
			    Locator::particleSystem::value().StartSpotVisual(SpotVisualType::PilefoodSpeedup, point, -1, created, 1.0f);
		}
	}
	// The people come to look at it
	const auto reaction = PotInfoOf(potType).associatedReaction;
	if (reaction != Reaction::None && Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().Create({.initiator = created,
		                                         .type = reaction,
		                                         .player = player,
		                                         .position = point,
		                                         .impressiveValue = PotInfoOf(potType).impressiveValue,
		                                         .power = 1.0f,
		                                         .magicType = MagicType::None,
		                                         .casterCreature = entt::null});
	}
	return true;
}

namespace
{
uint32_t PileHolds(const Pot& pot, ResourceType type)
{
	return PotInfoOf(pot.type).resourceType == type ? pot.amount : 0u;
}

/// A pile of a building site that is still there
Pot* LivePile(entt::entity pile)
{
	auto& registry = Entities();
	return registry.Valid(pile) ? registry.TryGet<Pot>(pile) : nullptr;
}

/// A building going up whose site keeps its wood in six piles: a temple's site taking brought wood
BuildingSite* PiledSite(entt::entity object)
{
	return Entities().AllOf<Temple>(object) ? ecs::construction::BroughtWoodSite(object) : nullptr;
}

/// Any other building with a site, going up or being mended, keeps its wood in the site's one pile: its site taking
/// brought wood
BuildingSite* StandardSite(entt::entity object)
{
	return Entities().AllOf<Abode>(object) ? ecs::construction::BroughtWoodSite(object) : nullptr;
}

TownResourceTally* TallyOf(entt::entity town)
{
	auto& registry = Entities();
	return town != entt::null && registry.Valid(town) ? registry.TryGet<TownResourceTally>(town) : nullptr;
}
} // namespace

uint32_t ResourceStoreSystem::GetResource(entt::entity object, ResourceType type) const
{
	auto& registry = Entities();
	if (!registry.Valid(object))
	{
		return 0;
	}
	if (IsWorshipSite(object))
	{
		const auto* pot = WorshipSitePot(object);
		return pot != nullptr && type == ResourceType::Food ? pot->amount : 0u;
	}
	if (auto* abode = registry.TryGet<Abode>(object))
	{
		// A building keeps a count of each, and nothing of anything else
		return type == ResourceType::Food || type == ResourceType::Wood ? CountOf(*abode, type) : 0u;
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// A pile of a store answers for its store
		if (const auto store = StoreOf(object))
		{
			return GetResource(*store, type);
		}
		return PileHolds(*pot, type);
	}
	if (registry.AllOf<Villager>(object))
	{
		return 0;
	}
	// A temple, built or going up, holds nothing itself
	if (registry.AllOf<Temple>(object))
	{
		return 0;
	}
	const auto resource = ResourceOf(object);
	return resource.type == type ? resource.amount : 0u;
}

uint32_t ResourceStoreSystem::AddResource(entt::entity object, ResourceType type, uint32_t amount, std::optional<glm::vec3> at)
{
	auto& registry = Entities();
	if (!registry.Valid(object))
	{
		return 0;
	}
	if (auto* villager = registry.TryGet<Villager>(object))
	{
		// Food or wood goes into its load, the wood showing as the plain log; it says it took none
		if (type == ResourceType::Food || type == ResourceType::Wood)
		{
			const auto picked = ecs::villager_carry::PickUp(*villager, type, static_cast<int16_t>(amount), 0);
			if (auto* tally = TallyOf(villager->town))
			{
				auto& carried = type == ResourceType::Food ? tally->foodCarried : tally->woodCarried;
				carried += static_cast<float>(picked);
			}
		}
		return 0;
	}
	if (IsWorshipSite(object))
	{
		return AddToWorshipSite(*this, object, type, amount, false);
	}
	if (auto* site = PiledSite(object))
	{
		return AddToSite(object, *site, type, amount, at);
	}
	if (registry.AllOf<Temple>(object))
	{
		return 0;
	}
	// A building with a site takes wood, and only wood, into the site's one pile, made for the first wood
	if (auto* site = StandardSite(object))
	{
		return AddToSite(object, *site, type, amount, at);
	}
	if (auto* abode = registry.TryGet<Abode>(object))
	{
		if (registry.AllOf<StoragePit>(object))
		{
			return AddToStore(object, type, amount, std::nullopt, false, at);
		}
		// TODO(temple-builders): a workshop keeps its wood in a pile of its own; openblack has no workshop store yet
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// A workshop given wood when it had none changes what its town wants
		if (abode->type == AbodeNumber::Workshop && CountOf(*abode, type) == 0 && Locator::townSystem::has_value())
		{
			Locator::townSystem::value().MarkDesiresChanged(TownOf(object));
		}
		CountOf(*abode, type) += amount;
		return amount;
	}
	if (registry.AllOf<Pot>(object))
	{
		if (const auto store = StoreOf(object))
		{
			return AddResource(*store, type, amount, at);
		}
		return AddToPile(object, type, amount, false);
	}
	return 0;
}

uint32_t ResourceStoreSystem::RemoveResource(entt::entity object, ResourceType type, uint32_t amount)
{
	auto& registry = Entities();
	if (!registry.Valid(object))
	{
		return 0;
	}
	if (IsWorshipSite(object))
	{
		return TakeFromWorshipSite(object, type, amount);
	}
	if (auto* site = PiledSite(object))
	{
		return TakeFromSite(object, *site, type, amount);
	}
	if (registry.AllOf<Temple>(object))
	{
		return 0;
	}
	// A building with a site gives wood from the site's one pile
	if (auto* site = StandardSite(object))
	{
		return TakeFromSite(object, *site, type, amount);
	}
	if (auto* abode = registry.TryGet<Abode>(object))
	{
		if (registry.AllOf<StoragePit>(object))
		{
			return type == ResourceType::Food || type == ResourceType::Wood ? TakeFromPit(object, type, amount, std::nullopt)
			                                                                : 0u;
		}
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		auto& count = CountOf(*abode, type);
		const auto removed = std::min(count, amount);
		count -= removed;
		return removed;
	}
	if (registry.AllOf<Pot>(object))
	{
		// A pile a building's site keeps its wood in gives it up as the site gives wood
		entt::entity building = entt::null;
		BuildingSite* keeper = nullptr;
		registry.Each<BuildingSites>([&](entt::entity entity, BuildingSites& sites) {
			for (auto& site : sites.sites)
			{
				if (keeper == nullptr && std::ranges::find(site.piles, object) != site.piles.end())
				{
					building = entity;
					keeper = &site;
				}
			}
		});
		if (keeper != nullptr)
		{
			return TakeFromSite(building, *keeper, type, amount);
		}
		return TakeFromPile(object, type, amount, std::nullopt);
	}
	if (auto* dead = registry.TryGet<DeadTree>(object))
	{
		// A fallen trunk gives its wood: used up it goes, else it shrinks to what is left
		if (type != ResourceType::Wood)
		{
			return 0;
		}
		const auto woodValue = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type)).woodValue;
		const auto take = ecs::villager_work::TakeFromFallenTree(GetResource(object, ResourceType::Wood), amount, woodValue,
		                                                         dead->woodMultiplier);
		if (!take.scale.has_value())
		{
			ecs::world_objects::Remove(object);
			registry.SetLayoutDirty();
			return take.taken;
		}
		registry.Get<Transform>(object).scale = glm::vec3(*take.scale);
		registry.SetDirty();
		return take.taken;
	}
	if (registry.AllOf<BigForest>(object))
	{
		return type == ResourceType::Wood && Locator::forestSystem::has_value()
		           ? Locator::forestSystem::value().TakeBigForestWood(object, amount)
		           : 0u;
	}
	// TODO(temple-builders): rocks and fish farms give up what they hold; not yet
	return 0;
}

uint32_t ResourceStoreSystem::AddToSite(entt::entity building, BuildingSite& site, ResourceType type, uint32_t amount,
                                        std::optional<glm::vec3> at)
{
	auto& registry = Entities();
	if (type != ResourceType::Wood && type != ResourceType::Any)
	{
		return 0;
	}
	// A worship site's site keeps the wood in its one pile, made for the first wood brought
	if (IsWorshipSite(building))
	{
		ecs::construction::MakeSitePiles(building, site);
		const auto pile = site.piles.front();
		return registry.Valid(pile) ? AddToPile(pile, type, amount, false) : 0u;
	}
	uint32_t added = 0;
	if (registry.AllOf<Temple>(building))
	{
		// A temple's site puts it on its pile nearest where it is brought, making its piles again if none is left
		if (at.has_value())
		{
			const auto nearest = [&]() -> entt::entity {
				entt::entity best = entt::null;
				float bestDistance = std::numeric_limits<float>::max();
				for (const auto pile : site.piles)
				{
					if (LivePile(pile) == nullptr)
					{
						continue;
					}
					const auto& position = registry.Get<const Transform>(pile).position;
					const float distance = gutils::GetDistanceInMetres(*at, position);
					if (distance < bestDistance)
					{
						bestDistance = distance;
						best = pile;
					}
				}
				return best;
			};
			auto pile = nearest();
			if (pile == entt::null)
			{
				ecs::construction::MakeSitePiles(building, site);
				pile = nearest();
			}
			if (pile != entt::null)
			{
				added = AddToPile(pile, type, amount, false);
			}
		}
	}
	else if (type == ResourceType::Wood)
	{
		// Any other site takes wood, and only wood, into its one pile, made for the first wood
		ecs::construction::MakeSitePiles(building, site);
		if (LivePile(site.piles.front()) != nullptr)
		{
			added = AddToPile(site.piles.front(), type, amount, false);
		}
	}
	if (auto* tally = TallyOf(ecs::construction::TownOfSite(building)))
	{
		tally->woodAtSites += static_cast<float>(added);
	}
	return added;
}

uint32_t ResourceStoreSystem::TakeFromSite(entt::entity building, BuildingSite& site, ResourceType type, uint32_t amount)
{
	auto& registry = Entities();
	if (type != ResourceType::Wood && type != ResourceType::Any)
	{
		return 0;
	}
	// A worship site's site gives from its one pile
	if (IsWorshipSite(building))
	{
		auto* pot = LivePile(site.piles.front());
		if (pot == nullptr)
		{
			return 0;
		}
		const auto taken = std::min(amount, pot->amount);
		pot->amount -= taken;
		return taken;
	}
	if (registry.AllOf<Temple>(building))
	{
		// Its piles in turn, each giving what it has; an emptied pile stays where it is
		uint32_t removed = 0;
		for (const auto pile : site.piles)
		{
			if (amount == 0)
			{
				break;
			}
			if (auto* pot = LivePile(pile))
			{
				const auto taken = std::min(amount, pot->amount);
				pot->amount -= taken;
				if (pot->amount == 0)
				{
					pot->poisoned = false;
				}
				removed += taken;
				amount -= taken;
			}
		}
		return removed;
	}
	// Any other site gives wood from its one pile, and its town counts that much less wood at its sites
	uint32_t removed = 0;
	if (auto* pot = LivePile(site.piles.front()); pot != nullptr && type == ResourceType::Wood)
	{
		removed = std::min(amount, pot->amount);
		pot->amount -= removed;
		if (pot->amount == 0)
		{
			pot->poisoned = false;
		}
	}
	if (auto* tally = TallyOf(ecs::construction::TownOfSite(building)))
	{
		tally->woodAtSites -= static_cast<float>(removed);
	}
	return removed;
}

uint32_t ResourceStoreSystem::AddToTownsSite(entt::entity building, entt::entity town, ResourceType type, uint32_t amount,
                                             std::optional<glm::vec3> at)
{
	auto* site = ecs::construction::SiteOf(town, building);
	return site != nullptr ? AddToSite(building, *site, type, amount, at) : 0u;
}

uint32_t ResourceStoreSystem::RemoveFromTownsSite(entt::entity building, entt::entity town, ResourceType type, uint32_t amount)
{
	auto* site = ecs::construction::SiteOf(town, building);
	return site != nullptr ? TakeFromSite(building, *site, type, amount) : 0u;
}
