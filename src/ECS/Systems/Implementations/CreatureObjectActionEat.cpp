/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <utility>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreatureObjectActions.h"
#include "CreatureObjectActionSystem.h"
#include "CreatureObjectActionSystemDetail.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/CreatureCarried.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/Graveyards.h"
#include "ECS/Map.h"
#include "ECS/MobileObjectRecord.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/Implementations/MagicLiving.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Sound/SoundRequestInterface.h"
#include "VillagerFire.h"
#include "VillagerPhysics.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature_object_actions::Kind;
using namespace openblack::ecs::systems::object_action_detail;

namespace
{
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();

/// Whether a creature about to swallow a thing brings it back up instead: one made indestructible (as Sable is for her
/// lessons), or one the scripts hold while they own the widescreen bars
bool BringsBackUp(const ecs::Registry& registry, entt::entity food)
{
	bool heldDuringCutscene = false;
	if (registry.AllOf<InScript>(food) && Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		heldDuringCutscene = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	}
	return creature_object_actions::BringsBackUp(registry.AllOf<Indestructible>(food), heldDuringCutscene);
}

/// The info of a thing that a creature may eat: an animal's own, or that of any other thing
const GObjectInfo* FoodInfoOf(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto kind = static_cast<size_t>(animal->type);
		if (!Locator::infoConstants::has_value() || kind >= Locator::infoConstants::value().animal.size())
		{
			return nullptr;
		}
		return &Locator::infoConstants::value().animal.at(kind);
	}
	return ecs::world_objects::InfoOf(object);
}

/// A magic mushroom growing on the land, or one of the mushrooms lying about
bool IsMagicMushroom(const ecs::Registry& registry, entt::entity object)
{
	const auto* feature = registry.TryGet<const Feature>(object);
	const auto* mobile = registry.TryGet<const MobileObject>(object);
	return (feature != nullptr && feature->type == FeatureInfo::MagicMushroom) ||
	       (mobile != nullptr &&
	        (mobile->type == MobileObjectInfo::MagicMushroom || mobile->type == MobileObjectInfo::Toadstool));
}

/// What the creature notes about a thing that it eats, before the thing goes
struct Meal
{
	std::optional<uint32_t> sort;
	bool magicMushroom {false};
	bool foodPile {false};
	bool poisoned {false};
};

Meal MealOf(const ecs::Registry& registry, entt::entity food)
{
	Meal meal;
	if (const auto* info = FoodInfoOf(registry, food))
	{
		meal.sort = static_cast<uint32_t>(info->type);
	}
	meal.magicMushroom = IsMagicMushroom(registry, food);
	if (const auto* pot = registry.TryGet<const Pot>(food))
	{
		const auto kind = static_cast<size_t>(pot->type);
		meal.foodPile = Locator::infoConstants::has_value() && kind < Locator::infoConstants::value().pot.size() &&
		                Locator::infoConstants::value().pot.at(kind).potType == PotType::PileFood;
		meal.poisoned = pot->poisoned;
	}
	else
	{
		// A toadstool is always poisoned food. Because it is also a mushroom, the creature does not bring it back up.
		const auto* mobile = registry.TryGet<const MobileObject>(food);
		meal.poisoned = registry.AllOf<Poisoned>(food) || (mobile != nullptr && mobile->type == MobileObjectInfo::Toadstool);
	}
	return meal;
}

/// Decrease the hunger of the creature by the energy of a meal that it did not bring up
void KeepMealDown(entt::entity creature, float energy)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ChangeDesires(creature, [energy](creature_desires::Desires& desires) {
			auto& hunger = desires[creature_desires::Desire::Hunger];
			hunger.value = creature_meals::HungerAfterMeal(hunger.value, energy);
		});
	}
}

/// The creature brings up a meal with too little food in it, or a poisoned meal. It is sick and is not hungry for a
/// time. Unless the meal was poisoned, it does not eat that sort of thing again.
void RefuseMeal(entt::entity creature, const Meal& meal)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* mind = registry.TryGet<CreatureMindState>(creature);
	    mind != nullptr && meal.sort.has_value() && creature_object_actions::RefusesSortAfter(meal.poisoned))
	{
		mind->refusedFoodSorts.insert(*meal.sort);
	}
	if (Locator::creatureMindSystem::has_value())
	{
		auto& minds = Locator::creatureMindSystem::value();
		minds.ChangeDesires(creature, [](creature_desires::Desires& desires) {
			creature_desires::Suppress(desires, creature_desires::Desire::Hunger,
			                           creature_object_actions::k_NotHungryAfterBringingUpSeconds, 1.0f / k_TurnSeconds);
		});
		minds.Puke(creature);
	}
}

/// The creature brings up what it was about to swallow: it falls from where it is, uneaten, the creature isn't hungry
/// for a while and is sick
void BringBackUp(entt::entity creature, entt::entity food)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<CreatureHeldObject>(creature);
	registry.Remove<HeldByCreature>(food);
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().InitialisePhysics(food, {.thrower = creature});
	}
	if (Locator::creatureMindSystem::has_value())
	{
		auto& minds = Locator::creatureMindSystem::value();
		minds.ChangeDesires(creature, [](creature_desires::Desires& desires) {
			creature_desires::Suppress(desires, creature_desires::Desire::Hunger,
			                           creature_object_actions::k_NotHungryAfterBringingUpSeconds, 1.0f / k_TurnSeconds);
		});
		minds.Puke(creature);
	}
}
} // namespace

namespace openblack::ecs::systems::object_action_detail
{
void Consume(ecs::Registry& registry, entt::entity creature, entt::entity food)
{
	// A living villager eaten dies of no cause, put down to the creature's player, as any death is heard of and counted.
	// One already dead was counted when it died.
	if (registry.AllOf<Villager>(food) && !registry.AllOf<VillagerDeath>(food))
	{
		const auto& body = registry.Get<const Creature>(creature);
		ecs::villager_fire::DieByEffect(food, ecs::villager_fire::EatenDeath(body.owner, ecs::world_objects::LifeOf(food)));
		// A villager that can't die as others do still counts among its town's burials, though no body is left
		if (!registry.AllOf<VillagerDeath>(food))
		{
			graveyards::TownPersonDied(registry.Get<const Villager>(food).town);
		}
	}
	// In a network game each villager that the creature eats, alive or dead, counts for the winning condition of its
	// player
	if (registry.AllOf<Villager>(food) && Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().AddToCondition(registry.Get<const Creature>(creature).owner,
		                                               game_end::Condition::VillagersKilledByCreature, 1);
	}
	if (const auto* villager = registry.TryGet<const Villager>(food))
	{
		if (auto* abode = registry.TryGet<Abode>(villager->abode))
		{
			std::erase(abode->inhabitants, food);
		}
		if (auto* town = registry.TryGet<Town>(villager->town))
		{
			std::erase(town->homelessVillagers, food);
		}
	}
	// An animal eaten leaves its flock, which goes once it has no one left
	if (registry.AllOf<Animal>(food) && Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().LeaveFlock(food);
	}
	registry.Destroy(food);
}

float TakeInFood(entt::entity creature, std::optional<float> value)
{
	// A thing with no food in it is taken in too, as a meal of no food. Such a meal can make a creature fat when its
	// energy is more than full.
	const auto energy = Locator::creaturePhysiologySystem::has_value()
	                        ? Locator::creaturePhysiologySystem::value().Eat(creature, value.value_or(0.0f))
	                        : 0.0f;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} takes in food {}, energy {}", entt::to_integral(creature),
	                    value.value_or(0.0f), energy);
	return energy;
}

void Swallow(entt::entity creature, CreatureObjectAction& action, std::optional<float> value)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto food = std::exchange(action.eaten, std::nullopt);
	if (!food.has_value() || !registry.Valid(*food))
	{
		return;
	}
	registry.Remove<HiddenByState>(*food);
	if (BringsBackUp(registry, *food))
	{
		BringBackUp(creature, *food);
		return;
	}
	// A magic mushroom makes it high once it is done with what it is doing
	if (IsMagicMushroom(registry, *food) && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().AteMagicMushroom(creature);
	}
	const auto meal = MealOf(registry, *food);
	Consume(registry, creature, *food);
	const bool bringsUp =
	    creature_object_actions::BringsMealBackUp(value.value_or(0.0f), meal.magicMushroom, meal.foodPile, meal.poisoned);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} swallows a thing of sort {}: food {}, brought up {}",
	                    entt::to_integral(creature), meal.sort.value_or(0), value.value_or(0.0f), bringsUp);
	auto* tallies = registry.TryGet<CreatureTallies>(creature);
	creature_meals::CountMeal(tallies != nullptr ? tallies->meals : registry.Assign<CreatureTallies>(creature).meals, meal.sort,
	                          !bringsUp);
	if (bringsUp)
	{
		RefuseMeal(creature, meal);
	}
	else
	{
		KeepMealDown(creature, action.mealEnergy);
	}
}
} // namespace openblack::ecs::systems::object_action_detail

bool CreatureObjectActionSystem::EatHeld(entt::entity creature)
{
	const auto held = GetHeld(creature);
	// It eats whatever it holds. A meal with no food in it comes back up after.
	if (!held.has_value())
	{
		auto action = CreatureObjectAction {.kind = Kind::Eat};
		Fail(action, "it holds nothing to eat");
		Locator::entitiesRegistry::value().AssignOrReplace<CreatureObjectAction>(creature, std::move(action));
		return false;
	}
	return Start(creature, {.kind = Kind::Eat, .target = held});
}

bool CreatureObjectActionSystem::EatHeldOnly(entt::entity creature)
{
	const auto held = GetHeld(creature);
	if (!held.has_value())
	{
		auto action = CreatureObjectAction {.kind = Kind::Eat, .swallows = false};
		Fail(action, "it holds nothing to eat");
		Locator::entitiesRegistry::value().AssignOrReplace<CreatureObjectAction>(creature, std::move(action));
		return false;
	}
	return Start(creature, {.kind = Kind::Eat, .target = held, .swallows = false});
}

void CreatureObjectActionSystem::SwallowStoppedEat(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::Eat || !action->eaten.has_value())
	{
		return;
	}
	Swallow(creature, *action, FoodValueOf(*action->eaten));
}

std::optional<float> CreatureObjectActionSystem::FoodValueOf(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	float value = 0.0f;
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// A pot or pile of food is worth as much as the food in it. A storage pit's pile is eaten from where it lies,
		// which isn't done here.
		const auto kind = static_cast<size_t>(pot->type);
		const bool food = kind < info.pot.size() && info.pot.at(kind).resourceType == ResourceType::Food &&
		                  pot->type != PotInfo::StoragePitFoodPile;
		value = food ? static_cast<float>(pot->amount) : 0.0f;
	}
	else if (const auto* objectInfo = FoodInfoOf(registry, object))
	{
		// Anything else has the food value of its kind, when its kind is meat or vegetable
		const auto eaten = static_cast<uint32_t>(FoodType::Meat) | static_cast<uint32_t>(FoodType::Vegetable);
		value = (static_cast<uint32_t>(objectInfo->foodType) & eaten) != 0 ? objectInfo->foodValue : 0.0f;
	}
	return value > 0.0f ? std::optional(value) : std::nullopt;
}

bool CreatureObjectActionSystem::CanEat(entt::entity creature, entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.Valid(object))
	{
		return false;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	const auto* info = FoodInfoOf(registry, object);
	const bool refused =
	    mind != nullptr && info != nullptr && mind->refusedFoodSorts.contains(static_cast<uint32_t>(info->type));
	const auto* body = registry.TryGet<const Creature>(creature);
	const bool ownVillager = registry.AllOf<Villager>(object) && body != nullptr &&
	                         ecs::world_objects::PlayerOf(object) == std::optional(body->owner);
	const auto kind = KindFactsOf(object);
	return creature_object_actions::MayEat(kind,
	                                       {.mayPickUp = CanPickUpKind(creature, object, kind),
	                                        .refusedSort = refused,
	                                        .computersOwnVillager = ownVillager && ComputerCreatureNotScripted(creature)});
}

bool CreatureObjectActionSystem::CanEatHeld(entt::entity creature) const
{
	const auto held = GetCarried(creature);
	return held.has_value() && creature_object_actions::MayEatHeld(KindFactsOf(*held), CanPickUp(creature, *held));
}
