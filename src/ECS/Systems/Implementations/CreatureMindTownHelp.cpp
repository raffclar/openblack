/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world's side of the actions with which a creature helps a town or plays with its people

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindTownHelp.h"

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureTownHelp.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/WorshipSites.h"
#include "Locator.h"
#include "VillagerDisciples.h"

using namespace openblack;
using namespace openblack::ecs::components;
using creature_plan_actions::Target;

namespace
{
/// Whether a thing stands fully built: it has no build progress left, or its progress is full
bool Built(const ecs::Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return false;
	}
	const auto* progress = registry.TryGet<const BuildProgress>(thing);
	return progress == nullptr || !(progress->built < 1.0f);
}

/// The town of a totem: the town of the town centre that it stands on
entt::entity TownOfTotem(const ecs::Registry& registry, entt::entity totem)
{
	const auto* standing = registry.TryGet<const VillageTotem>(totem);
	const auto* centre = standing != nullptr && registry.Valid(standing->townCentre)
	                         ? registry.TryGet<const Abode>(standing->townCentre)
	                         : nullptr;
	if (centre == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(centre->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

/// Whether a creature may move a totem: one of the towns of the player who watches, its town centre built, its worship
/// site built, and that player's temple there. With one player at this machine, the player who watches is that player.
bool MayMoveTotem(const ecs::Registry& registry, entt::entity totem)
{
	const auto town = TownOfTotem(registry, totem);
	const auto* townComponent = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	if (townComponent == nullptr || !Locator::playerSystem::has_value())
	{
		return false;
	}
	const auto player = Locator::playerSystem::value().GetLocalPlayer();
	bool temple = false;
	registry.Each<const Temple>(
	    [player, &temple](entt::entity /*unused*/, const Temple& standing) { temple = temple || standing.owner == player; });
	return townComponent->owner == player && Built(registry, registry.Get<const VillageTotem>(totem).townCentre) && temple &&
	       Built(registry, townComponent->worshipSite);
}

/// The breeders that a town has now
uint32_t BreedersOf(const ecs::Registry& registry, entt::entity town)
{
	uint32_t breeders = 0;
	registry.Each<const Disciple, const Villager>(
	    [town, &breeders](entt::entity /*unused*/, const Disciple& disciple, const Villager& villager) {
		    if (disciple.type == VillagerDisciple::Breeder && villager.town == town)
		    {
			    ++breeders;
		    }
	    });
	return breeders;
}

bool CanPickUp(entt::entity self, entt::entity thing)
{
	return Locator::creatureObjectActionSystem::has_value() &&
	       Locator::creatureObjectActionSystem::value().CanPickUp(self, thing);
}
} // namespace

bool ecs::systems::mind_town_help::Accepts(const ecs::Registry& registry, Target target, entt::entity thing, entt::entity self,
                                           bool heldByOther)
{
	switch (target)
	{
	case Target::MagicWoodPile:
	{
		// A pile of the wood miracle counts wherever it is
		const auto* pot = registry.TryGet<const Pot>(thing);
		return !heldByOther && pot != nullptr && pot->type == PotInfo::MagicWood;
	}
	case Target::Cow:
	{
		const auto* animal = registry.TryGet<const Animal>(thing);
		return !heldByOther && animal != nullptr && creature_town_help::IsCowKind(animal->type) && CanPickUp(self, thing);
	}
	case Target::VillagerNotWorshipping:
		return registry.AllOf<Villager>(thing) && !registry.AllOf<Dancer>(thing);
	case Target::BreederVillager:
	{
		const auto* villager = registry.TryGet<const Villager>(thing);
		if (villager == nullptr)
		{
			return false;
		}
		const bool hasTown = registry.Valid(villager->town);
		return creature_town_help::CanMakeBreeder({
		    .available = villager->lifeStage == Villager::LifeStage::Adult && !registry.AllOf<VillagerDeath>(thing) &&
		                 !registry.AllOf<ScriptControlled>(thing),
		    .hasTown = hasTown,
		    .disciple = registry.AllOf<Disciple>(thing),
		    .follower = registry.AllOf<DiscipleFollower>(thing),
		    .townBreeders = hasTown ? BreedersOf(registry, villager->town) : 0,
		});
	}
	case Target::VillagerFarFromHome:
	{
		const auto* villager = registry.TryGet<const Villager>(thing);
		const auto* at = registry.TryGet<const Transform>(thing);
		const auto* home = villager != nullptr && registry.Valid(villager->abode)
		                       ? registry.TryGet<const Transform>(villager->abode)
		                       : nullptr;
		return at != nullptr && home != nullptr &&
		       creature_town_help::FarFromHome(gutils::GetDistanceInMetres(home->position, at->position));
	}
	case Target::PlayerTotem:
		return registry.AllOf<VillageTotem>(thing) && MayMoveTotem(registry, thing);
	default:
		return false;
	}
}

void ecs::systems::mind_town_help::Situate(const ecs::Registry& registry, entt::entity creature,
                                           const creature_plan_actions::Executor& executor, std::optional<uint32_t> object,
                                           creature_plan_actions::Situation& situation)
{
	using creature_plan_actions::Build;
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto held = Locator::creatureObjectActionSystem::value().GetHeld(creature))
		{
			const auto* animal = registry.TryGet<const Animal>(*held);
			situation.holdingCow = animal != nullptr && creature_town_help::IsCowKind(animal->type);
			situation.holdingVillager = registry.AllOf<Villager>(*held);
		}
	}
	const auto thing = object.has_value() ? static_cast<entt::entity>(*object) : entt::null;
	if (!registry.Valid(thing))
	{
		return;
	}
	if (executor.build == Build::PutFieldFoodBySite)
	{
		// The place where the site keeps its food
		const auto* site = registry.TryGet<const WorshipSite>(thing);
		const auto* at = registry.TryGet<const Transform>(thing);
		if (site != nullptr && at != nullptr)
		{
			situation.siteFood =
			    worship_site::TurnedPoint({at->position.x, at->position.z}, site->facing, worship_site::k_FoodPotPoint);
		}
	}
	if (executor.build == Build::TakeVillagerHome)
	{
		if (const auto* villager = registry.TryGet<const Villager>(thing);
		    villager != nullptr && registry.Valid(villager->abode))
		{
			situation.villagerHome = entt::to_integral(villager->abode);
		}
	}
}

std::optional<float> ecs::systems::mind_town_help::TotemShareOf(const ecs::Registry& registry,
                                                                const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size())
	{
		return std::nullopt;
	}
	const auto& step = idle.agenda[idle.step];
	if ((step.kind != creature_mind::Step::Kind::RaiseTotem && step.kind != creature_mind::Step::Kind::LowerTotem) ||
	    !step.object.has_value())
	{
		return std::nullopt;
	}
	const auto totem = static_cast<entt::entity>(*step.object);
	const auto town = registry.Valid(totem) ? TownOfTotem(registry, totem) : entt::null;
	const auto* townComponent = town != entt::null ? registry.TryGet<const Town>(town) : nullptr;
	if (townComponent == nullptr || !registry.Valid(townComponent->worshipSite))
	{
		return std::nullopt;
	}
	return townComponent->worshipShare;
}

void ecs::systems::mind_town_help::Apply(ecs::Registry& registry, const creature_mind::Commands& commands)
{
	if (commands.totem.has_value() && Locator::villageTotemSystem::has_value())
	{
		const auto totem = static_cast<entt::entity>(commands.totem->totem);
		if (const auto town = registry.Valid(totem) ? TownOfTotem(registry, totem) : entt::null; town != entt::null)
		{
			Locator::villageTotemSystem::value().SetTownShare(town, commands.totem->share);
		}
	}
	if (commands.makeDisciple.has_value())
	{
		const auto villager = static_cast<entt::entity>(*commands.makeDisciple);
		if (registry.Valid(villager) && registry.AllOf<Villager>(villager))
		{
			villager_disciple::SetType(villager, VillagerDisciple::Breeder);
		}
	}
}
