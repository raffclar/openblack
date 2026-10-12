/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownDesireSystem.h"

#include <algorithm>
#include <ranges>
#include <vector>

#include <glm/gtx/euler_angles.hpp>

#include "3D/DayNightClock.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/TownDesire.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace town_desire = openblack::ecs::town_desire;
namespace construction = openblack::ecs::construction;
namespace town_desire_flags = openblack::ecs::town_desire_flags;
namespace villager_home = openblack::ecs::villager_home;

namespace
{
constexpr size_t Index(TownDesireInfo desire)
{
	return static_cast<size_t>(static_cast<int>(desire));
}

bool Valid(TownDesireInfo desire)
{
	return static_cast<int>(desire) >= 0 && Index(desire) < town_desire::k_Count;
}

/// The info of a tribe's kind of abode, if it has one
const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number)
{
	const auto& abodes = Locator::infoConstants::value().abode;
	const auto found = std::ranges::find_if(
	    abodes, [tribe, number](const GAbodeInfo& info) { return info.tribeType == tribe && info.abodeNumber == number; });
	return found != abodes.end() ? &*found : nullptr;
}

/// The info of a tribe's villager of a number
const GVillagerInfo* FindVillagerInfo(Tribe tribe, VillagerNumber number)
{
	const auto& villagers = Locator::infoConstants::value().villager;
	const auto found = std::ranges::find_if(villagers, [tribe, number](const GVillagerInfo& info) {
		return info.tribeType == tribe && info.villagerNumber == number;
	});
	return found != villagers.end() ? &*found : nullptr;
}

/// A town's people and buildings this turn, and what its desires read of them and the world
town_desire::DesireInputs GatherInputs(entt::entity townEntity, const Town& town, Tribe tribe)
{
	auto& registry = Locator::entitiesRegistry::value();
	town_desire::DesireInputs in;
	in.tribe = tribe;
	auto& stats = in.stats;

	registry.Each<const Villager>([&](const Villager& villager) {
		if (villager.town != townEntity)
		{
			return;
		}
		++(villager.sex == Villager::Sex::MALE ? stats.men : stats.women);
		if (villager.lifeStage == Villager::LifeStage::Child)
		{
			++stats.children;
		}
		else
		{
			++stats.adults;
		}
		if (const auto* disciple = registry.TryGet<const Disciple>(registry.ToEntity(villager)); disciple != nullptr)
		{
			++stats.disciples.at(static_cast<size_t>(disciple->type));
		}
		if (const auto* info = FindVillagerInfo(villager.tribe, villager.number); info != nullptr)
		{
			stats.foodForDinner += static_cast<float>(info->foodReqiredForDinner);
		}
	});

	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId != town.id)
		{
			return;
		}
		const auto* info = FindAbodeInfo(tribe, abode.type);
		// A building counts among the town's buildings and their places once it is built
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		if (progress == nullptr || progress->built >= 1.0f)
		{
			++in.abodeCount;
			const auto number = static_cast<size_t>(abode.type);
			if (number < stats.abodesByNumber.size())
			{
				++stats.abodesByNumber.at(number);
			}
			if (info != nullptr && info->maxVillagersInAbode + info->maxChildrenInAbode != 0)
			{
				++stats.abodesWithPlaces;
				stats.adultPlaces += info->maxVillagersInAbode;
				stats.childPlaces += info->maxChildrenInAbode;
				stats.totalPlaces += info->maxVillagersInAbode + info->maxChildrenInAbode;
				const auto household = openblack::ecs::town_homes::HouseholdOf(registry, entity);
				stats.adultRoom += static_cast<int32_t>(info->maxVillagersInAbode) - static_cast<int32_t>(household.adults);
				stats.childRoom += static_cast<int32_t>(info->maxChildrenInAbode) - static_cast<int32_t>(household.children);
			}
		}
		// Each abode as worn as it is, by how much its kind wants mending
		in.abodes.push_back({
		    .life = ecs::world_objects::LifeOf(entity),
		    .livingQuarters = info != nullptr &&
		                      (static_cast<uint32_t>(info->abodeType) & static_cast<uint32_t>(AbodeType::LivingQuarters)) != 0,
		    .inhabitants = static_cast<uint32_t>(abode.inhabitants.size()),
		    .desireToBeRepaired = info != nullptr ? info->desireToBeRepaired : 0.0f,
		});
		if (abode.type == AbodeNumber::StoragePit)
		{
			in.storageFood = abode.foodAmount;
			in.storageWood = abode.woodAmount;
		}
		if (abode.type == AbodeNumber::Creche)
		{
			in.crecheFunctional = true;
		}
	});

	for (size_t i = 0; i < in.populationWhenNeeded.size(); ++i)
	{
		if (const auto* info = FindAbodeInfo(tribe, static_cast<AbodeNumber>(i)); info != nullptr)
		{
			in.populationWhenNeeded.at(i) = info->populationWhenNeeded;
		}
	}
	in.homeless = static_cast<uint32_t>(town.homelessVillagers.size());
	in.worshipping = static_cast<int32_t>(town.worshippers.size());
	in.onWayToWorship = static_cast<int32_t>(town.onWayToWorship.size());

	// Its building sites: how much each wants villagers, the builders at it and the most it takes
	for (const auto building : construction::SitesOfTown(townEntity))
	{
		in.siteDesires.push_back(construction::DesireForVillagers(building));
		in.siteBuilders.push_back(static_cast<uint32_t>(registry.Get<const BuildingSite>(building).workerCount));
		in.sitePlaces.push_back(construction::MaxBuilders(building));
	}
	// What its people carry and the wood at its sites, as it has kept count
	if (const auto* tally = registry.TryGet<const TownResourceTally>(townEntity))
	{
		stats.foodCarried = tally->foodCarried;
		stats.woodCarried = tally->woodCarried;
		stats.woodAtSites = tally->woodAtSites;
	}

	// The game counts the sky from 2 at night to 0 by day
	if (Locator::skySystem::has_value())
	{
		const auto& clock = Locator::skySystem::value().GetClock();
		in.visualHour = clock.GetVisualTime();
		in.skyType = 2.0f - clock.SkyType(in.visualHour);
		in.dayFull = clock.GetVisualTimes()[3];
	}
	if (Locator::time::has_value())
	{
		in.turn = Locator::time::value().GetTurn();
	}
	// What it wants of protection and mercy is what it summed of the attacks on it last turn
	if (const auto* aggression = registry.TryGet<const TownAggression>(townEntity))
	{
		in.protection = aggression->record.protection;
		in.mercy = aggression->record.mercy;
	}
	return in;
}
} // namespace

void TownDesireSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	// The game takes a villager's load from its African farmer
	const auto& farmer = info.villager.at(10);
	registry.Each<const Town, const Tribe, TownDesire, TownStats>(
	    [&](entt::entity entity, const Town& town, const Tribe tribe, TownDesire& desire, TownStats& stats) {
		    const auto inputs = GatherInputs(entity, town, tribe);
		    stats = inputs.stats;
		    const town_desire::DesireContext context {
		        .desire = desire,
		        .in = inputs,
		        .town = info.town,
		        .info = info.townDesire,
		        .farmerMaxFood = farmer.maxFoodCarried,
		        .farmerMaxWood = farmer.maxWoodCarried,
		    };
		    town_desire::Process(desire, context);
		    // Then the attacks on it fade and are summed for its next turn's desires
		    if (auto* aggression = registry.TryGet<TownAggression>(entity))
		    {
			    town_aggression::ProcessTurn(aggression->record, town.owner);
		    }
	    });
}

uint32_t TownDesireSystem::OfferVillager(entt::entity town, bool child, float trigger,
                                         const std::function<uint32_t(TownDesireInfo)>& satisfy)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<TownDesire, TownStats>(town))
	{
		return 0;
	}
	const auto& desire = registry.Get<const TownDesire>(town);
	const auto& stats = registry.Get<const TownStats>(town);
	return town_desire::CheckVillagerNeeded(desire, Locator::infoConstants::value().townDesire, stats.adults + stats.children,
	                                        child, trigger,
	                                        [&satisfy](size_t d) { return satisfy(static_cast<TownDesireInfo>(d)); });
}

float TownDesireSystem::GetDesire(entt::entity town, TownDesireInfo desire) const
{
	const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const TownDesire>(town);
	return townDesire != nullptr && Valid(desire) ? town_desire::GetDesire(*townDesire, Index(desire)) : 0.0f;
}

float TownDesireSystem::GetRawDesire(entt::entity town, TownDesireInfo desire) const
{
	const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const TownDesire>(town);
	return townDesire != nullptr && Valid(desire) ? town_desire::GetRawDesire(*townDesire, Index(desire)) : 0.0f;
}

float TownDesireSystem::GetSignificanceToVillager(entt::entity town, TownDesireInfo desire) const
{
	const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const TownDesire>(town);
	if (townDesire == nullptr || !Valid(desire) || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value().townDesire.at(Index(desire));
	return town_desire::GetDesireSignificanceToVillager(*townDesire, info, Index(desire));
}

float TownDesireSystem::RecomputeDesire(entt::entity town, TownDesireInfo desire)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* townDesire = registry.TryGet<TownDesire>(town);
	const auto* data = registry.TryGet<const Town>(town);
	const auto* tribe = registry.TryGet<const Tribe>(town);
	if (townDesire == nullptr || data == nullptr || tribe == nullptr || !Valid(desire))
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& farmer = info.villager.at(10);
	const auto inputs = GatherInputs(town, *data, *tribe);
	const town_desire::DesireContext context {
	    .desire = *townDesire,
	    .in = inputs,
	    .town = info.town,
	    .info = info.townDesire,
	    .farmerMaxFood = farmer.maxFoodCarried,
	    .farmerMaxWood = farmer.maxWoodCarried,
	};
	return town_desire::CallDesireFunction(*townDesire, context, Index(desire));
}

float TownDesireSystem::FoodShortage(entt::entity town) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* townDesire = registry.TryGet<const TownDesire>(town);
	const auto* data = registry.TryGet<const Town>(town);
	const auto* tribe = registry.TryGet<const Tribe>(town);
	if (townDesire == nullptr || data == nullptr || tribe == nullptr)
	{
		return 0.0f;
	}
	// TODO(villager-hunger-health): a town of the player's running this low (0.95 or more) shows the low-on-food help
	const auto& info = Locator::infoConstants::value();
	const auto& farmer = info.villager.at(10);
	const auto inputs = GatherInputs(town, *data, *tribe);
	return town_desire::DesireForFood({
	    .desire = *townDesire,
	    .in = inputs,
	    .town = info.town,
	    .info = info.townDesire,
	    .farmerMaxFood = farmer.maxFoodCarried,
	    .farmerMaxWood = farmer.maxWoodCarried,
	});
}

TownDesireInfo TownDesireSystem::GetMostWanted(entt::entity town) const
{
	const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const TownDesire>(town);
	return townDesire != nullptr ? static_cast<TownDesireInfo>(townDesire->sorted.at(0).index) : TownDesireInfo::None;
}

void TownDesireSystem::SetBoost(entt::entity town, TownDesireInfo desire, float boost, bool resort)
{
	auto* townDesire = Locator::entitiesRegistry::value().TryGet<TownDesire>(town);
	if (townDesire == nullptr || !Valid(desire))
	{
		return;
	}
	townDesire->boost.at(Index(desire)) = boost;
	if (resort)
	{
		town_desire::SortDesires(*townDesire);
	}
}

namespace
{

/// A town's flags are made with it, unseen until it has a store standing built
void MakeFlags(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value().townDesire;
	TownDesireFlags made;
	made.flags.fill(entt::null);
	for (size_t i = 0; i < info.size(); ++i)
	{
		const auto point = town_desire_flags::PointOf(info.at(i).worshipSiteSlot);
		if (!point.has_value())
		{
			continue;
		}
		const auto flag = registry.Create();
		registry.Assign<TownDesireFlag>(
		    flag, TownDesireFlag {.town = town, .desire = static_cast<TownDesireInfo>(i), .point = *point});
		registry.Assign<Mesh>(flag, resources::HashIdentifier(info.at(i).worshipSiteMesh), static_cast<int8_t>(0),
		                      static_cast<int8_t>(0));
		registry.Assign<Transform>(flag, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		made.flags.at(i) = flag;
	}
	registry.Assign<TownDesireFlags>(town, made);
}

/// Each new store of its town takes the flag to its own place for it, from the store's model, over the ground
void PlaceOnStore(TownDesireFlag& flag, entt::entity store)
{
	const auto& registry = Locator::entitiesRegistry::value();
	flag.store = store;
	const auto& transform = registry.Get<const Transform>(store);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = registry.Get<const Mesh>(store).id;
	glm::vec3 local {0.0f};
	if (meshes.Contains(meshId))
	{
		if (const auto& points = meshes.Handle(meshId)->GetExtraMetrics(); flag.point < points.size())
		{
			local = glm::vec3(points.at(flag.point)[3]);
		}
	}
	flag.place = transform.position + (transform.rotation * (local * transform.scale));
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({flag.place.x, flag.place.z}) : 0.0f;
	flag.overGround = flag.place.y - ground;
}

} // namespace

void TownDesireSystem::UpdateFlags(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value().townDesire;

	std::vector<entt::entity> towns;
	registry.Each<const Town, const TownDesire>([&](entt::entity town, const Town&, const TownDesire&) {
		if (!registry.AllOf<TownDesireFlags>(town))
		{
			towns.push_back(town);
		}
	});
	for (const auto town : towns)
	{
		MakeFlags(town);
	}

	// A flag holds still while it is out of view, as the game moves it only as it draws it
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto viewProjection = camera != nullptr ? camera->GetViewProjectionMatrix() : glm::mat4(1.0f);
	const auto eye = camera != nullptr ? camera->GetOrigin() : glm::vec3(0.0f);

	std::vector<entt::entity> orphans;
	registry.Each<TownDesireFlag, Transform>([&](entt::entity entity, TownDesireFlag& flag, Transform& transform) {
		if (!registry.Valid(flag.town) || !registry.AllOf<Town>(flag.town))
		{
			orphans.push_back(entity);
			return;
		}
		const auto store = construction::StoragePitOf(flag.town);
		if (store.has_value() && *store != flag.store)
		{
			PlaceOnStore(flag, *store);
		}
		// A store works while built and not hurt down to the share at which it stops working
		const bool working = store.has_value() && villager_home::IsFunctional(*store);
		const float desire = GetRawDesire(flag.town, flag.desire);
		const float showsAfter = info.at(static_cast<size_t>(flag.desire)).showsAfterPercent;
		const bool shows = town_desire_flags::Shows(working, flag.level, showsAfter);
		if (shows && flag.shown && camera != nullptr &&
		    !town_desire_flags::InView(viewProjection * glm::vec4(transform.position, 1.0f)))
		{
			return;
		}
		const auto next = town_desire_flags::Step({.position = flag.level, .speed = flag.speed}, shows, desire, seconds);
		flag.level = next.position;
		flag.speed = next.speed;
		flag.shown = shows;
		if (!shows)
		{
			return;
		}
		auto position = flag.place;
		const float ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({position.x, position.z}) : 0.0f;
		position.y = ground + flag.overGround + town_desire_flags::Rise(flag.level);
		transform.position = position;
		transform.rotation = glm::mat3(glm::eulerAngleY(-town_desire_flags::FacingAngle(position, eye)));
		transform.scale = glm::vec3(town_desire_flags::DrawnScale(flag.level, showsAfter));
	});
	for (const auto orphan : orphans)
	{
		registry.Destroy(orphan);
	}
}

std::optional<TownDesireSystemInterface::FlagToolTip> TownDesireSystem::GetFlagToolTip(entt::entity flag) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(flag) ? registry.TryGet<const TownDesireFlag>(flag) : nullptr;
	if (component == nullptr || !component->shown || !registry.Valid(component->town))
	{
		return std::nullopt;
	}
	const auto words = town_desire_flags::ToolTipWords(component->desire);
	if (!words.has_value())
	{
		return std::nullopt;
	}
	return FlagToolTip {.words = *words,
	                    .percent = town_desire_flags::NeedPercent(GetRawDesire(component->town, component->desire))};
}
