/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerResources.h"

#include <algorithm>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "Common/GUtilsDistance.h"
#include "ECS/CarriedProps.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VillagerStoresInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/Villager/VillagerTrace.h"
#include "InfoConstants.h"
#include "Locator.h"

// Carrying, picking up and dropping resources (VillagerResources.h)

namespace openblack::ecs::villager
{
using namespace components;
namespace tq = town_queries;

namespace
{
/// Carried object ids: 1 none, 6 bag, 12 wood, 13..15 the three tree types; 16 is the first id past the carried
/// ones
constexpr int32_t k_CarriedNone = 1;
constexpr int32_t k_CarriedBag = 6;
constexpr int32_t k_CarriedWood = 12;
constexpr int32_t k_CarriedTree1 = 13;
constexpr int32_t k_CarriedTree2 = 14;
constexpr int32_t k_CarriedTree3 = 15;
constexpr int32_t k_CarriedEnd = 16;
/// A small bias added to both sides of the drop-off fraction
constexpr float k_DropOffEpsilon = 1e-5f;
/// The drop-off score's maximum distance for GetDistanceModifier
constexpr float k_DropOffMaxDistance = 500.0f;
/// The lower clamp of both load factors
constexpr float k_MinLoadFactor = 0.75f;
/// The upper clamp of the town-needs term
constexpr float k_MaxTownNeedsTerm = 0.5f;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// The villager's town, or nullptr without one
Town* TownComponentOf(const Villager& v)
{
	auto& registry = Entities();
	// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
	return v.town != entt::null && ecs::IsAvailable(v.town) ? registry.TryGet<Town>(v.town) : nullptr;
}

entt::entity TownEntityOf(const Villager& v)
{
	return TownComponentOf(v) != nullptr ? v.town : entt::null;
}

/// The villager's position as the MapCoords the temporary store lookup takes (x / z; the altitude is not read)
map_coords::MapCoords MapCoordsOfVillager(entt::entity villager)
{
	const auto me = tq::PosOf(villager);
	return {me.x, me.y, 0.0f};
}

const char* TypeName(ResourceType type)
{
	return type == ResourceType::Food ? "FOOD" : type == ResourceType::Wood ? "WOOD" : "NONE";
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

void TraceCarry(entt::entity villager, const Villager& v, const char* what, ResourceType type, int32_t amount)
{
	if (!TraceOn(villager))
	{
		return;
	}
	const auto* town = TownComponentOf(v);
	Trace(villager, fmt::format("carry: {} {} {} (held {}/{}, town carried {}/{})", what, TypeName(type), amount,
	                            v.resourceHeld.at(0), v.resourceHeld.at(1), town != nullptr ? town->stats.foodCarried : 0.0f,
	                            town != nullptr ? town->stats.woodCarried : 0.0f));
}

/// The temporary stores and the dropped logs, through the Locator
systems::VillagerStoresInterface& Stores()
{
	return Locator::villagerStores::value();
}

/// The town's temporary store of that type, through the villagerStores service
town_stores::TemporaryStore TemporaryStore(entt::entity town, const map_coords::MapCoords& from, ResourceType type)
{
	return Stores().GetTemporaryStore(town, from, type);
}

/// Drops n of the held food (index 0) or wood (index 1): n == 0, or more than it carries (an unsigned 16-bit
/// compare), drops all of it; the town's carried total goes down by the same
uint16_t DropHeld(entt::entity villager, size_t index, uint16_t amount)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto held = static_cast<uint16_t>(v->resourceHeld.at(index));
	if (amount == 0 || amount > held)
	{
		amount = held;
	}
	v->resourceHeld.at(index) = static_cast<int16_t>(static_cast<uint16_t>(held - amount));
	if (auto* town = TownComponentOf(*v))
	{
		auto& carried = index == 0 ? town->stats.foodCarried : town->stats.woodCarried;
		carried = carried - static_cast<float>(amount);
	}
	TraceCarry(villager, *v, "drop", index == 0 ? ResourceType::Food : ResourceType::Wood, amount);
	return amount;
}
} // namespace

float SpeedInMetres(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? wallHug->speed : 0.0f;
}

// ---- the pure layer ----------------------------------------------------------------------------------------------

HeldResource HeldLarger(int16_t food, int16_t wood)
{
	const auto f = static_cast<uint16_t>(food);
	const auto w = static_cast<uint16_t>(wood);
	// Food only when strictly more than the wood (unsigned 16 bits)
	if (f > w)
	{
		return {ResourceType::Food, f};
	}
	// Else any wood
	if (w != 0)
	{
		return {ResourceType::Wood, w};
	}
	// Nothing held: 0 and no type
	return {ResourceType::None, 0};
}

int32_t WoodCarriedObject(uint16_t flags)
{
	// The tree type bits: 1..3 -> the tree logs, else plain wood
	switch (flags >> Villager::k_TreeTypeShift)
	{
	case 1:
		return k_CarriedTree1;
	case 2:
		return k_CarriedTree2;
	case 3:
		return k_CarriedTree3;
	default:
		return k_CarriedWood;
	}
}

bool IsBuildingExitState(VillagerStates state)
{
	// The state's exit handler is the building exit (VillagerOriginalFns.h keeps only the handler ids)
	const auto index = static_cast<size_t>(static_cast<uint8_t>(state));
	return index < k_OriginalStateFns.size() && k_OriginalStateFns.at(index).exit == k_ExitBuilding;
}

int32_t CarriedObjectFor(const CarriedInput& in)
{
	// Final state IN_SCRIPT or top state SCRIPT_PLAY_ANIM: the carried object stays as it was
	if (in.finalState == VillagerStates::InScript || in.topState == VillagerStates::ScriptPlayAnim)
	{
		return in.previous;
	}
	// None before the tests
	int32_t carried = k_CarriedNone;
	// Only above the crawling-wounded life (a NaN life fails too)
	if (in.life > in.lifeWhenCrawlsWounded)
	{
		if (static_cast<int32_t>(in.wood) > static_cast<int32_t>(in.minWoodToShowGraphic))
		{
			// More wood than the graphic threshold: the log of the tree type
			carried = WoodCarriedObject(in.flags);
		}
		else if (static_cast<int32_t>(in.food) > static_cast<int32_t>(in.minFoodToShowGraphic))
		{
			// More food than the threshold and not a builder's final state: the bag
			if (!in.finalIsBuilding)
			{
				carried = k_CarriedBag;
			}
		}
	}
	// The final state's row overrides it when set
	if (in.rowCarriedFinal != 0)
	{
		carried = in.rowCarriedFinal;
	}
	// The top state's row overrides that
	if (in.rowCarriedTop != 0)
	{
		carried = in.rowCarriedTop;
	}
	return carried;
}

LoadFactor LoadFactors(int16_t wood, int16_t food, const GVillagerInfo& info, bool trader)
{
	// A trader uses the trader capacities (at most 500 in info.dat: exact in a float)
	// The original runs the FPU at float precision: every step below rounds to float
	const auto maxWood = static_cast<float>(trader ? info.maxTraderWoodCarried : info.maxWoodCarried);
	const auto maxFood = static_cast<float>(trader ? info.maxTraderFoodCarried : info.maxFoodCarried);
	// Wood: (SpeedModWhenFullLoadOfWood + 1) - held / max
	// (openblack, test-only guard) a capacity of 0 (only the tests' zero-filled infos; info.dat has 150 / 250 / 500 in
	// all 63 records) gives no load term; the original would give 0.75 there (0 / 0 = NaN, then the clamp)
	const float woodLoad = maxWood != 0.0f ? static_cast<float>(wood) / maxWood : 0.0f;
	const float foodLoad = maxFood != 0.0f ? static_cast<float>(food) / maxFood : 0.0f;
	float woodF = (info.speedModWhenFullLoadOfWood + 1.0f) - woodLoad;
	// Food: the same with SpeedModWhenFullLoadOfFood
	float foodF = (info.speedModWhenFullLoadOfFood + 1.0f) - foodLoad;
	// Clamped to [0.75, 1]
	woodF = woodF < k_MinLoadFactor ? k_MinLoadFactor : woodF > 1.0f ? 1.0f : woodF;
	// The same for food, also with 0.75
	foodF = foodF < k_MinLoadFactor ? k_MinLoadFactor : foodF > 1.0f ? 1.0f : foodF;
	return {woodF, foodF};
}

float TownNeedsFactor(float townNeedsSum, const GVillagerInfo& info)
{
	// S / DivisorForTownNeedsSpeedMod clamped to [0, 0.5], plus BaseForTownNeedsSpeedMod
	// (openblack, test-only guard) a divisor of 0 (only the tests' zero-filled infos; info.dat has 2.0) gives no term;
	// the original would give 0.5 there (S / 0 = inf, then the clamp). At float precision
	const float divisor = info.divisorForTownNeedsSpeedMod;
	float term = divisor != 0.0f ? townNeedsSum / divisor : 0.0f;
	term = term < 0.0f ? 0.0f : term > k_MaxTownNeedsTerm ? k_MaxTownNeedsTerm : term;
	return term + info.baseForTownNeedsSpeedMod;
}

float DropOffFraction(int16_t capacity, uint32_t maxFoodCarried)
{
	// 1 - (capacity + bias) / (MaxFoodCarried + bias), every step at float precision
	return 1.0f - (static_cast<float>(capacity) + k_DropOffEpsilon) / (static_cast<float>(maxFoodCarried) + k_DropOffEpsilon);
}

float DropOffScore(int16_t held, uint32_t maxFoodCarried, float distance)
{
	// The 16-bit food capacity: max - held
	const auto capacity = static_cast<int16_t>(static_cast<uint16_t>(maxFoodCarried) - static_cast<uint16_t>(held));
	// The distance modifier (up to 500) times the fraction, at float precision
	return gutils::GetDistanceModifier(distance, k_DropOffMaxDistance) * DropOffFraction(capacity, maxFoodCarried);
}

std::optional<DroppedLog> DroppedLogFor(int32_t carriedObject, int16_t wood, uint32_t minWoodToShowGraphic, float logWoodValue)
{
	// Only a carried object between none and the end id
	if (carriedObject <= k_CarriedNone || carriedObject >= k_CarriedEnd)
	{
		return std::nullopt;
	}
	// Only above the wood graphic threshold
	if (static_cast<int32_t>(wood) <= static_cast<int32_t>(minWoodToShowGraphic))
	{
		return std::nullopt;
	}
	// The log's multiplier: the wood over the new log's wood value
	return DroppedLog {carriedObject, static_cast<float>(wood) / logWoodValue};
}

int32_t DroppedLogValue(uint32_t woodValue, float multiplier, float scale)
{
	// woodValue x multiplier x scale, truncated; each product rounds to float (woodValue is exact in a float)
	return static_cast<int32_t>(static_cast<float>(woodValue) * multiplier * scale);
}

// ---- carrying ----------------------------------------------------------------------------------------------------

int16_t PickupResource(entt::entity villager, ResourceType type, int16_t amount, uint8_t treeType)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return amount;
	}
	auto* town = TownComponentOf(*v);
	if (type == ResourceType::Food)
	{
		// Food: a 16-bit add to the held food, and to the town's carried total
		v->resourceHeld.at(0) =
		    static_cast<int16_t>(static_cast<uint16_t>(v->resourceHeld.at(0)) + static_cast<uint16_t>(amount));
		if (town != nullptr)
		{
			town->stats.foodCarried = town->stats.foodCarried + static_cast<float>(amount);
		}
		TraceCarry(villager, *v, "pickup", ResourceType::Food, amount);
		return amount;
	}
	// Any other type: the same for wood, then the tree type bits
	v->resourceHeld.at(1) = static_cast<int16_t>(static_cast<uint16_t>(v->resourceHeld.at(1)) + static_cast<uint16_t>(amount));
	if (town != nullptr)
	{
		town->stats.woodCarried = town->stats.woodCarried + static_cast<float>(amount);
	}
	v->flags = static_cast<uint16_t>((v->flags & ~Villager::k_FlagTreeTypeMask) |
	                                 ((static_cast<uint16_t>(treeType) & 3u) << Villager::k_TreeTypeShift));
	TraceCarry(villager, *v, "pickup", ResourceType::Wood, amount);
	return amount;
}

void PickupFood(entt::entity villager, int16_t amount)
{
	PickupResource(villager, ResourceType::Food, amount, 0);
}

void PickupWood(entt::entity villager, int16_t amount, uint8_t treeType)
{
	PickupResource(villager, ResourceType::Wood, amount, treeType);
}

uint16_t DropFood(entt::entity villager, uint16_t amount)
{
	return DropHeld(villager, 0, amount);
}

uint16_t DropWood(entt::entity villager, uint16_t amount)
{
	// The tree type bits are not cleared (literal)
	return DropHeld(villager, 1, amount);
}

uint16_t DropResource(entt::entity villager, ResourceType type, uint16_t amount)
{
	// Any other type (also -2) drops nothing
	if (type == ResourceType::Wood)
	{
		return DropWood(villager, amount);
	}
	if (type == ResourceType::Food)
	{
		return DropFood(villager, amount);
	}
	return 0;
}

int16_t GetFoodCapacity(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// MaxFoodCarried - held, kept to 16 bits
	const auto& info = InfoOf(villager);
	return static_cast<int16_t>(static_cast<uint16_t>(info.maxFoodCarried) - static_cast<uint16_t>(v->resourceHeld.at(0)));
}

int16_t GetWoodCapacity(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	// MaxWoodCarried - held, kept to 16 bits
	const auto& info = InfoOf(villager);
	return static_cast<int16_t>(static_cast<uint16_t>(info.maxWoodCarried) - static_cast<uint16_t>(v->resourceHeld.at(1)));
}

uint16_t GetResourceHeld(entt::entity villager, ResourceType& type)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		type = ResourceType::None;
		return 0;
	}
	const auto held = HeldLarger(v->resourceHeld.at(0), v->resourceHeld.at(1));
	type = held.type;
	return held.amount;
}

uint32_t AddResourceToVillager(entt::entity villager, ResourceType type, uint32_t amount, bool poisoned)
{
	if (type == ResourceType::Food)
	{
		// Food, and poisoned food poisons the villager
		PickupFood(villager, static_cast<int16_t>(amount));
		if (poisoned)
		{
			life::TakePoisonedResource(villager);
		}
	}
	else if (type == ResourceType::Wood)
	{
		PickupWood(villager, static_cast<int16_t>(amount), 0);
	}
	// 0 whatever was added (literal)
	return 0;
}

uint16_t GetResourceFrom(entt::entity villager, entt::entity object, ResourceType type, int16_t amount)
{
	// A negative n is a huge unsigned amount: the whole store (literal)
	const auto requested = static_cast<uint32_t>(static_cast<int32_t>(amount));
	const auto taken = static_cast<uint16_t>(object_resources::RemoveResource(object, type, requested));
	if (taken == 0)
	{
		return 0;
	}
	// An object's carried tree type is always 0
	PickupResource(villager, type, static_cast<int16_t>(taken), 0);
	// Abodes and storage pits are never sped up (the piles have their own): no food speed-up here
	// A poisoned store poisons the villager
	if (object_resources::IsPoisoned(object))
	{
		life::TakePoisonedResource(villager);
	}
	return taken;
}

// ---- the carried object ------------------------------------------------------------------------------------------

int32_t GetWoodCarriedObject(entt::entity villager)
{
	const auto* v = VillagerOf(villager);
	return WoodCarriedObject(v != nullptr ? v->flags : 0);
}

bool FinalStateIsBuilding(entt::entity villager)
{
	return IsBuildingExitState(GetFinalState(villager));
}

int32_t SetStateCarriedObject(entt::entity villager, int32_t previous)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr || !Entities().AllOf<LivingAction>(villager))
	{
		return previous;
	}
	const auto& info = InfoOf(villager);
	CarriedInput in;
	in.finalState = GetFinalState(villager);
	in.topState = GetState(villager, Index::Top);
	in.life = v->life;
	in.wood = v->resourceHeld.at(1);
	in.food = v->resourceHeld.at(0);
	in.flags = v->flags;
	in.finalIsBuilding = IsBuildingExitState(in.finalState);
	in.rowCarriedFinal = state_info::CarriedObject(state_info::StateInfo(in.finalState));
	in.rowCarriedTop = state_info::CarriedObject(state_info::StateInfo(in.topState));
	in.previous = previous;
	in.lifeWhenCrawlsWounded = info.lifeWhenCrawlsWounded;
	in.minWoodToShowGraphic = info.minWoodToShowGraphic;
	in.minFoodToShowGraphic = info.minFoodToShowGraphic;
	const auto carried = CarriedObjectFor(in);
	if (carried != previous)
	{
		villager::TraceFormatted(villager, "carried: {} -> {} (final {}, top {})", previous, carried,
		                         static_cast<uint32_t>(in.finalState), static_cast<uint32_t>(in.topState));
	}
	return carried;
}

// ---- where -------------------------------------------------------------------------------------------------------

entt::entity GetStoragePit(entt::entity villager)
{
	// The town's storage pit, else the home
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return entt::null;
	}
	// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
	if (v->town != entt::null && ecs::IsAvailable(v->town))
	{
		if (const auto pit = tq::GetStoragePit(v->town); pit != entt::null)
		{
			return pit;
		}
	}
	// (guard) the original reads the abode link unchecked: this stands for the abode's unlinking on deletion
	return v->abode != entt::null && ecs::IsAvailable(v->abode) ? v->abode : entt::null;
}

glm::ivec2 GetResourceDropoffPos(entt::entity villager, ResourceType type)
{
	const auto* v = VillagerOf(villager);
	const auto me = tq::PosOf(villager);
	if (v == nullptr)
	{
		return me;
	}
	// My storage pit, if functional: its arrive position
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		return abode_queries::GetArrivePos(pit);
	}
	// Else with a town, the town's storage pit if functional: its arrive position
	// (guard) the original reads the town link unchecked: this stands for the missing town unlinking
	const auto town = v->town != entt::null && ecs::IsAvailable(v->town) ? v->town : entt::null;
	if (town != entt::null)
	{
		if (const auto pit = tq::GetStoragePit(town); pit != entt::null && abode_queries::IsFunctional(pit))
		{
			return abode_queries::GetArrivePos(pit);
		}
		// Else the town's temporary pot of that type (made now if it has none: a side effect, literal) and its edge
		// towards me; the point is returned whole
		const auto store = TemporaryStore(town, MapCoordsOfVillager(villager), type);
		return {store.pos.x, store.pos.z};
	}
	// No town: my own position
	return me;
}

town_stores::TemporaryStore GetTemporaryStore(entt::entity town, const map_coords::MapCoords& from, ResourceType type)
{
	return TemporaryStore(town, from, type);
}

glm::ivec2 GetResourceNearestEdge(entt::entity object, [[maybe_unused]] ResourceType type,
                                  [[maybe_unused]] entt::entity villager)
{
	// A storage pit gives its arrive position; any other object its own position
	if (Entities().AllOf<StoragePit>(object))
	{
		return abode_queries::GetArrivePos(object);
	}
	return tq::PosOf(object);
}

uint32_t AtStructureRemoveResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t amount)
{
	const auto pos = GetResourceNearestEdge(object, type, villager);
	// Close enough: within my 2D radius
	const auto me = tq::PosOf(villager);
	if (tq::GetDistanceInMetres(me, pos) <= tq::Get2DRadius(villager))
	{
		// Take it: nothing -> 0, only part of it -> 0x24, all of it -> 1
		const auto c = static_cast<uint32_t>(GetResourceFrom(villager, object, type, static_cast<int16_t>(amount)));
		if (c == 0)
		{
			return 0;
		}
		return c < amount ? 0x24u : 1u;
	}
	// Not there yet: walk to it; 0x24
	SetupMoveToWithHug(villager, tq::ToMetres(pos), GetFinalState(villager));
	return 0x24;
}

uint32_t AtStructureAddResource(entt::entity villager, entt::entity object, ResourceType type, uint32_t& amount)
{
	// TODO(players): the original passes an interface status from the object's player; (inferred) a villager drops
	// into its own town's pit or home, so the status is 0 in every carrying path
	const auto edge = GetResourceNearestEdge(object, type, villager);
	// Close enough: within my speed (not the radius, unlike AtStructureRemoveResource)
	const auto me = tq::PosOf(villager);
	if (tq::GetDistanceInMetres(me, edge) <= SpeedInMetres(villager))
	{
		const uint32_t added = object_resources::AddResource(object, type, amount);
		// Refused -> 0
		if (added == 0)
		{
			return 0;
		}
		// Drop what was added; 1
		DropResource(villager, type, static_cast<uint16_t>(added));
		amount = added;
		return 1;
	}
	// Not there yet: walk to it; n = 0; 0x24
	SetupMoveToWithHug(villager, tq::ToMetres(edge), GetFinalState(villager));
	amount = 0;
	return 0x24;
}

uint32_t ArrivesAtStoragePitForResource(entt::entity villager, ResourceType type, uint32_t amount, VillagerStates ok,
                                        VillagerStates fail)
{
	const auto* v = VillagerOf(villager);
	// Nothing asked for -> fail; 1
	if (amount == 0 || v == nullptr)
	{
		SetTopState(villager, fail);
		return 1;
	}
	const auto pit = GetStoragePit(villager);
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		// m = min(n, what the pit has)
		const uint32_t have = object_resources::GetResource(pit, type);
		const uint32_t m = amount < have ? amount : have;
		// Nothing to take -> fail; 0
		if (m == 0)
		{
			SetTopState(villager, fail);
			return 0;
		}
		const auto r = AtStructureRemoveResource(villager, pit, type, m);
		if (TraceOn(villager))
		{
			Trace(villager,
			      fmt::format("food 34: pit {} has {} need {} -> {:#x}", static_cast<uint32_t>(pit), have, amount, r));
		}
		// 0x24: on the way, or only part of it taken
		if (r == 0x24)
		{
			return 0x24;
		}
		// All of it and an ok state -> walk to the pit's arrive position, then ok; 1
		if (r == 1 && ok != VillagerStates::InvalidState)
		{
			SetupMoveToOnFootpath(villager, pit, abode_queries::GetArrivePos(pit), ok);
			return 1;
		}
		// Else fail; 0
		SetTopState(villager, fail);
		return 0;
	}
	// No functional pit and no town -> fail; 1
	const auto town = TownEntityOf(*v);
	if (town == entt::null)
	{
		TraceIf(villager, "food 34: no functional storage pit, no town -> fail");
		SetTopState(villager, fail);
		return 1;
	}
	// The town's temporary pot of that type (its point is not used here); none -> fail (only without a town in
	// openblack)
	const auto store = TemporaryStore(town, MapCoordsOfVillager(villager), type);
	if (store.pot == entt::null)
	{
		TraceIf(villager, "food 34: no temporary pot -> fail");
		SetTopState(villager, fail);
		return 1;
	}
	// The pot's nearest edge towards me, from both 2D radii (the altitude is dropped). (approximate) without the
	// original's round trip through world coordinates (at most 1 MapCoords unit)
	const auto edge = object::GetNearestPosOfObject(store.pot, villager);
	const auto p = tq::ToMetres(glm::ivec2(edge.x, edge.z));
	if (AreWeThere(villager, p, 0.0f))
	{
		const uint32_t had = TraceOn(villager) ? object_resources::GetResource(store.pot, type) : 0;
		// The result is discarded: the villager gets the whole n even from an empty pot (literal oddity)
		static_cast<void>(object_resources::RemoveResource(store.pot, type, amount));
		// No poison and no food speed-up from a temporary pot (literal)
		PickupResource(villager, type, static_cast<int16_t>(amount), 0);
		villager::TraceFormatted(villager, "pot: {} from 34 took {} (pot had {})", TypeName(type), amount, had);
		// An ok state -> it; 1
		if (ok != VillagerStates::InvalidState)
		{
			SetTopState(villager, ok);
			return 1;
		}
		// Else fail; 0
		SetTopState(villager, fail);
		return 0;
	}
	// Not there yet: walk to it; 0x24
	SetupMoveToWithHug(villager, p, GetFinalState(villager));
	return 0x24;
}

// ---- dropping off ------------------------------------------------------------------------------------------------

uint32_t GotoStoragePitForDropOff(entt::entity villager)
{
	// The storage pit (the town's, else the home) functional -> walk to its arrive position, then 32; 1
	if (const auto pit = GetStoragePit(villager); pit != entt::null && abode_queries::IsFunctional(pit))
	{
		const auto arrive = abode_queries::GetArrivePos(pit);
		villager::TraceFormatted(villager, "drop 31: {} {} ({}, {}) -> 32", Entities().AllOf<StoragePit>(pit) ? "pit" : "home",
		                         static_cast<uint32_t>(pit), arrive.x, arrive.y);
		SetupMoveToOnFootpath(villager, pit, arrive, VillagerStates::ArrivesAtStoragePitForDropOff);
		return 1;
	}
	// Holding neither food nor wood -> 163; 0
	ResourceType type = ResourceType::None;
	const auto held = GetResourceHeld(villager, type);
	if (type != ResourceType::Food && type != ResourceType::Wood)
	{
		TraceIf(villager, "drop 31: nothing -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 0;
	}
	// Else walk to the drop-off point of that type, then 32; 1
	const auto pos = GetResourceDropoffPos(villager, type);
	villager::TraceFormatted(villager, "drop 31: {} {} -> drop-off point ({}, {}) -> 32", TypeName(type), held, pos.x, pos.y);
	SetupMoveToWithHug(villager, tq::ToMetres(pos), VillagerStates::ArrivesAtStoragePitForDropOff);
	return 1;
}

uint32_t GotoStoragePitForDropOffState(LivingAction& action)
{
	return GotoStoragePitForDropOff(Entities().ToEntity(action));
}

uint32_t ArrivesAtStoragePitForDropOff(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	// Only the larger of the two loads (the other stays in hand, literal); nothing -> 163; 1
	ResourceType type = ResourceType::None;
	const uint32_t held = GetResourceHeld(villager, type);
	if (held == 0)
	{
		TraceIf(villager, "drop 32: none -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto pit = GetStoragePit(villager);
	if (pit != entt::null && abode_queries::IsFunctional(pit))
	{
		uint32_t amount = held;
		const auto r = AtStructureAddResource(villager, pit, type, amount);
		villager::TraceFormatted(villager, "drop 32: held {} {} -> pit {} +{} (r={:#x})", TypeName(type), held,
		                         static_cast<uint32_t>(pit), amount, r);
		// 0x24 (the GO_HOME state id reused as "on the way") -> 1
		if (r == 0x24)
		{
			return 1;
		}
		// Added or refused: walk to the pit's arrive position, then 163; 1
		SetupMoveToOnFootpath(villager, pit, abode_queries::GetArrivePos(pit), VillagerStates::DecideWhatToDo);
		return 1;
	}
	// No town -> 163; 1
	const auto* v = VillagerOf(villager);
	const auto town = v != nullptr ? TownEntityOf(*v) : entt::null;
	if (town == entt::null)
	{
		TraceIf(villager, "drop 32: no pit, no town -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// The town's temporary pot of that type; none -> 163; 1
	const auto store = TemporaryStore(town, MapCoordsOfVillager(villager), type);
	if (store.pot == entt::null)
	{
		TraceIf(villager, "drop 32: no temporary pot -> 163");
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto pos = tq::ToMetres(glm::ivec2(store.pos.x, store.pos.z));
	if (AreWeThere(villager, pos, 0.0f))
	{
		// Into the pot; the result is ignored
		static_cast<void>(object_resources::AddResource(store.pot, type, held));
		// Drop the food or the wood, then 163; 1
		if (type == ResourceType::Food)
		{
			DropFood(villager, static_cast<uint16_t>(held));
		}
		else
		{
			DropWood(villager, static_cast<uint16_t>(held));
		}
		villager::TraceFormatted(villager, "drop 32: held {} {} -> pot {} +{} -> 163", TypeName(type), held,
		                         static_cast<uint32_t>(store.pot), held);
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// Not there yet: walk to it; 1
	villager::TraceFormatted(villager, "drop 32: walking to the pot {}", static_cast<uint32_t>(store.pot));
	SetupMoveToWithHug(villager, pos, GetFinalState(villager));
	return 1;
}

// ---- dropped resources -------------------------------------------------------------------------------------------

void CreateDroppedResource(entt::entity villager, std::optional<glm::vec3> velocity, std::optional<glm::vec3> angular,
                           std::optional<glm::vec3> extra)
{
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return;
	}
	// The carried object (SkeletalAnimation::carriedObject; none without one)
	const auto* animation = Entities().TryGet<const SkeletalAnimation>(villager);
	const int32_t carried = animation != nullptr ? animation->carriedObject : k_CarriedNone;
	// The new log is a pine with life 1 and scale 1, so its wood value (life x woodValue x scale^3) is the pine's
	const auto& pine = Locator::infoConstants::value().tree.at(static_cast<size_t>(TreeInfo::Pine));
	const auto logWoodValue = static_cast<float>(pine.woodValue);
	const auto& info = InfoOf(villager);
	const auto log = DroppedLogFor(carried, v->resourceHeld.at(1), info.minWoodToShowGraphic, logWoodValue);
	if (!log)
	{
		// Nothing dropped (and no DropWood)
		TraceIf(villager, "dropped log: none");
		return;
	}
	// The log has the carried object's mesh and the multiplier, lies at my position at an angle of pi/2 and goes into
	// physics with the velocity a and the angular b (b only with a) and the extra vector c; it is then set on the
	// ground and raised until it does not intersect (the villagerStores service)
	const auto* transform = Entities().TryGet<const Transform>(villager);
	const auto pos = transform != nullptr ? transform->position : glm::vec3(0.0f);
	const auto mesh = CarriedObjectMesh(log->carriedObject);
	const glm::vec3 a = velocity.value_or(glm::vec3(0.0f));
	const glm::vec3 b = velocity && angular ? *angular : glm::vec3(0.0f);
	villager::TraceFormatted(villager, "dropped log: wood {} carried {} mesh {} multiplier {:.9f} value {}",
	                         v->resourceHeld.at(1), log->carriedObject, mesh, log->multiplier,
	                         DroppedLogValue(pine.woodValue, log->multiplier, 1.0f));
	Stores().MakeDroppedLog(pos, mesh, log->multiplier, a, b, extra);
	// DropWood(0): all the wood
	DropWood(villager, 0);
}
} // namespace openblack::ecs::villager
