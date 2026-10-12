/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFood.h"

#include <algorithm>
#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DiscipleRules.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerCheckRules.h"
#include "ECS/VillagerFoodRules.h"
#include "ECS/VillagerNeeds.h"
#include "ECS/VillagerRoutine.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerBuild.h"
#include "VillagerFire.h"
#include "VillagerFootpath.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_food = openblack::ecs::villager_food;
namespace rules = openblack::ecs::villager_food_rules;
namespace needs = openblack::ecs::villager_needs;
namespace villager_home = openblack::ecs::villager_home;
namespace villager_build = openblack::ecs::villager_build;
namespace villager_footpath = openblack::ecs::villager_footpath;
namespace villager_fire = openblack::ecs::villager_fire;
namespace world_objects = openblack::ecs::world_objects;
namespace construction = openblack::ecs::construction;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

VillagerStates FinalState(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetFinalState(action);
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

entt::entity TownOf(const Villager& villager)
{
	const auto& registry = Entities();
	return villager.town != entt::null && registry.Valid(villager.town) && registry.AllOf<Town>(villager.town) ? villager.town
	                                                                                                           : entt::null;
}

entt::entity AbodeOf(const Villager& villager)
{
	const auto& registry = Entities();
	return villager.abode != entt::null && registry.Valid(villager.abode) && registry.AllOf<Abode>(villager.abode)
	           ? villager.abode
	           : entt::null;
}

bool IsAtHome(entt::entity villager)
{
	return Entities().AllOf<AtHome>(villager);
}

bool IsPoisoned(entt::entity villager)
{
	return Entities().AllOf<Poisoned>(villager);
}

/// Its town's storage pit when that works
std::optional<entt::entity> WorkingStoragePit(const Villager& villager)
{
	const auto pit = construction::StoragePitOf(TownOf(villager));
	return pit.has_value() && villager_home::IsFunctional(*pit) ? pit : std::nullopt;
}

/// A disciple at work: the farmer, forester, fisherman, builder, breeder, protector, craftsman and trader. Hunger alone
/// doesn't take them from their work, though starving does.
bool IsWorkingDisciple(entt::entity villager)
{
	const auto* role = Entities().TryGet<const Disciple>(villager);
	return role != nullptr && openblack::disciple::KindOf(role->type).waitsWhenIdle;
}

/// How much food a meal is for the villager
uint32_t AmountOfFoodToEat(const Villager& person)
{
	const auto town = TownOf(person);
	std::optional<float> townDesire;
	if (town != entt::null && Locator::townDesireSystem::has_value())
	{
		townDesire = Locator::townDesireSystem::value().GetDesire(town, TownDesireInfo::ForFood);
	}
	return rules::AmountToEat(needs::DesireForFood(person.food), InfoOf(person).foodReqiredForDinner, townDesire);
}

uint32_t AmountOfFoodRequiredForMeal(const Villager& person)
{
	return rules::RequiredForMeal(AmountOfFoodToEat(person), person.foodHeld);
}

/// Whether its walk moved it this turn
bool IsMoving(entt::entity villager)
{
	const auto& registry = Entities();
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	return wallHug != nullptr && wallHug->step != glm::ivec2(0) &&
	       registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                      MoveStateFinalStepTag>(villager);
}

/// The power of its player's Japanese tribe, which makes food last; none without a player
std::optional<float> FoodTribalPower(entt::entity villager)
{
	const auto player = world_objects::PlayerOf(villager);
	if (!player.has_value())
	{
		return std::nullopt;
	}
	return Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetTribalPower(*player, Tribe::JAPANESE) : 1.0f;
}

/// Takes the food it needs beyond what it carries from its home's store
void GetFoodFromHome(entt::entity villager, uint32_t amount)
{
	const auto& person = Entities().Get<const Villager>(villager);
	const auto abode = AbodeOf(person);
	if (abode == entt::null)
	{
		return;
	}
	const auto held = Locator::resourceStoreSystem::value().GetResource(abode, ResourceType::Food);
	// What it takes comes into its hands once (the game took it into its hands twice: deviations register)
	villager_food::GetResourceFrom(villager, abode, ResourceType::Food,
	                               static_cast<uint16_t>(rules::TakenFromHome(amount, held)));
}

/// Whether what it takes from is sped-up food: a pile or pot of it. A store never passes its speed-up on.
bool IsSpeedUp(entt::entity source)
{
	const auto* pile = Entities().TryGet<const ResourcePile>(source);
	return Entities().AllOf<Pot>(source) && pile != nullptr && pile->speedUp;
}

/// Whether a pile is there and poisoned
bool IsPoisonedPile(entt::entity pile)
{
	auto& registry = Entities();
	if (pile == entt::null || !registry.Valid(pile))
	{
		return false;
	}
	const auto* pot = registry.TryGet<const Pot>(pile);
	return pot != nullptr && pot->poisoned;
}

/// Whether what it takes from is poisoned: a poisoned pile or pot, or a storage pit with poison in its food pile or any of
/// its wood piles (so poisoned wood poisons the food taken from it too). A home's food is never poisoned.
bool IsPoisonedSource(entt::entity source)
{
	auto& registry = Entities();
	if (const auto* pit = registry.TryGet<const StoragePit>(source))
	{
		return IsPoisonedPile(pit->foodPile) || std::ranges::any_of(pit->woodPiles, IsPoisonedPile);
	}
	if (registry.AllOf<Abode>(source))
	{
		return false;
	}
	return IsPoisonedPile(source);
}
} // namespace

bool villager_food::IsHungry(entt::entity villager)
{
	const auto& person = Entities().Get<const Villager>(villager);
	return needs::IsHungry(person.food, InfoOf(person).hungryForFood);
}

uint16_t villager_food::GetResourceFrom(entt::entity villager, entt::entity source, ResourceType type, uint16_t amount)
{
	auto& registry = Entities();
	auto& stores = Locator::resourceStoreSystem::value();
	const auto taken = static_cast<uint16_t>(stores.RemoveResource(source, type, amount));
	if (taken == 0)
	{
		return 0;
	}
	stores.AddResource(villager, type, taken, std::nullopt);
	if (IsSpeedUp(source))
	{
		registry.Get<Villager>(villager).foodSpeedUp = rules::k_FoodSpeedUpTicks;
	}
	if (IsPoisonedSource(source))
	{
		registry.AssignOrReplace<Poisoned>(villager);
	}
	return taken;
}

uint32_t villager_food::FoodRequiredForMeal(entt::entity villager)
{
	return AmountOfFoodRequiredForMeal(Entities().Get<const Villager>(villager));
}

float villager_food::EatFoodHeld(entt::entity villager)
{
	auto& registry = Entities();
	auto& person = registry.Get<Villager>(villager);
	const auto meal =
	    rules::EatHeld(AmountOfFoodToEat(person), person.foodHeld, person.food, InfoOf(person).foodNurishmentMultiplier);
	const auto dropped = ecs::villager_carry::PutDown(person, ResourceType::Food, meal.eaten);
	const auto town = TownOf(person);
	if (auto* tally = town != entt::null ? registry.TryGet<TownResourceTally>(town) : nullptr)
	{
		tally->foodCarried -= static_cast<float>(dropped);
	}
	person.food = meal.food;
	if (town != entt::null)
	{
		// The town and its player count the food eaten
		if (auto* tally = registry.TryGet<TownResourceTally>(town))
		{
			tally->foodUsed += static_cast<float>(meal.eaten);
		}
		player_statistics::CountFoodUsed(registry, registry.Get<const Town>(town).owner, meal.eaten);
	}
	registry.SetDirty();
	return person.food;
}

void villager_food::ProcessFoodSpeedUp(entt::entity villager)
{
	auto& person = Entities().Get<Villager>(villager);
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	person.foodSpeedUp = rules::TickFoodSpeedUp(person.foodSpeedUp, turn);
}

bool villager_food::ChangeStateToFindFoodToEat(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& person = Entities().Get<const Villager>(villager);
	rules::MealInputs in {
	    .required = AmountOfFoodRequiredForMeal(person), .carried = person.foodHeld, .atHome = IsAtHome(villager)};
	auto& stores = Locator::resourceStoreSystem::value();
	if (const auto abode = AbodeOf(person); abode != entt::null && villager_home::IsFunctional(abode))
	{
		in.homeFood = stores.GetResource(abode, ResourceType::Food);
	}
	if (const auto pit = WorkingStoragePit(person); pit.has_value())
	{
		in.pitFood = stores.GetResource(*pit, ResourceType::Food);
	}
	// TODO(villager-hunger-health): without a working pit the town's own place for food
	switch (rules::PlanMeal(in))
	{
	case rules::MealPlan::None:
		return false;
	case rules::MealPlan::EatHeld:
		SetTopState(action, VillagerStates::EatFood);
		return true;
	case rules::MealPlan::EatAtHome:
		SetTopState(action, VillagerStates::EatFoodAtHome);
		return true;
	case rules::MealPlan::GoHome:
		SetTopState(action, VillagerStates::GoHome);
		return true;
	case rules::MealPlan::GoToStoragePit:
		SetTopState(action, VillagerStates::GotoStoragePitForFood);
		return true;
	case rules::MealPlan::GoToDropOff:
		return false;
	}
	return false;
}

bool villager_food::CheckSatisfyOwnFoodDesire(LivingAction& action)
{
	return IsHungry(EntityOf(action)) && ChangeStateToFindFoodToEat(action);
}

bool villager_food::CheckHungry(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	auto& person = registry.Get<Villager>(villager);
	const auto& info = InfoOf(person);
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	const auto digestion = rules::Digest({
	    .turnsSinceChecked = turn - person.lastCheckTurn,
	    .food = person.food,
	    .foodPerTurn = info.gameTurnReducesFoodInBellyBy,
	    .hungryForFood = info.hungryForFood,
	    .hungerToLife = info.hungerToLifeMultiplier,
	    .speed = static_cast<float>(ecs::wall_hug::WholeSpeed(wallHug != nullptr ? wallHug->speed : 0.0f)),
	    .walkingSpeed = static_cast<float>(info.speedGroup.speedDefault),
	    .moving = IsMoving(villager),
	    .tribalPower = FoodTribalPower(villager),
	    .poisoned = IsPoisoned(villager),
	});
	if (!digestion.has_value())
	{
		return false;
	}
	person.food = digestion->food;
	bool busy = false;
	if (digestion->lifeLost > 0.0f)
	{
		world_objects::ReduceLife(villager, digestion->lifeLost);
	}
	if (digestion->hungry)
	{
		const auto& row = StateInfo(FinalState(action));
		busy = rules::SeeksFood(row.hungerInterrupts != 0, row.starvingInterrupts != 0, IsWorkingDisciple(villager),
		                        person.food, info.starvingForFood, [&action] { return ChangeStateToFindFoodToEat(action); });
		if (world_objects::LifeOf(villager) <= 0.0f)
		{
			// A worshipper at its site dies of its chanting there, in its worship turn
			const auto reason = rules::HungerDeath(ecs::villager_check_rules::ChantedLast(FinalState(action)));
			villager_fire::DieByEffect(villager, villager_fire::DeathCause {.reason = reason,
			                                                                .killer = world_objects::PlayerOf(villager),
			                                                                .weight = world_objects::LifeOf(villager)});
			busy = true;
		}
	}
	// The villager may have gone with its death; its component is looked up afresh
	if (auto* still = registry.TryGet<Villager>(villager))
	{
		still->lastCheckTurn = turn;
	}
	registry.SetDirty();
	return busy;
}

uint32_t villager_food::GotoStoragePitForFood(LivingAction& action)
{
	const auto& person = Entities().Get<const Villager>(EntityOf(action));
	if (const auto pit = WorkingStoragePit(person); pit.has_value())
	{
		villager_footpath::SetupMoveToOnFootpath(action, *pit, villager_home::ArrivePosition(*pit),
		                                         VillagerStates::ArrivesAtStoragePitForFood);
		return 1;
	}
	// TODO(villager-hunger-health): without a working pit it walks to its town's own place for food
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_food::ArrivesAtStoragePitForFood(LivingAction& action)
{
	const auto& person = Entities().Get<const Villager>(EntityOf(action));
	return villager_build::ArrivesAtStoragePitForResource(action, ResourceType::Food, AmountOfFoodRequiredForMeal(person),
	                                                      VillagerStates::DecideWhatToDo, VillagerStates::DecideWhatToDo);
}

uint32_t villager_food::EatFood(LivingAction& action)
{
	const auto villager = EntityOf(action);
	EatFoodHeld(villager);
	Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(
	    action, IsPoisoned(villager) ? VillagerStates::ShowPoisoned : VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_food::EatFoodAtHome(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& person = Entities().Get<const Villager>(villager);
	const int32_t required = static_cast<int32_t>(AmountOfFoodToEat(person)) - person.foodHeld;
	if (required > 0)
	{
		GetFoodFromHome(villager, static_cast<uint32_t>(required));
	}
	EatFoodHeld(villager);
	SetTopState(action, IsPoisoned(villager) ? VillagerStates::ShowPoisoned : VillagerStates::AtHome);
	return 1;
}

uint32_t villager_food::ShowPoisoned(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (IsAtHome(villager))
	{
		// It steps outside first; showing its poison then takes over at once, as in the game
		if (const auto spot = villager_home::PosOutsideAbode(villager); spot.has_value())
		{
			villager_home::SetupMoveTo(action, *spot, VillagerStates::ShowPoisoned);
		}
	}
	Locator::livingActionSystem::value().VillagerPlayAnimThenSetState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_food::DoesNothing(LivingAction& /*action*/)
{
	return 1;
}

namespace
{
/// The food reaction's table row: how far it is noticed, and how far a villager will go for it
const ::openblack::ReactionInfo& FoodReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToFood));
}

glm::vec3 Position(entt::entity entity)
{
	return Entities().Get<const Transform>(entity).position;
}

/// A pile of food a villager can take from: a pile on the ground or a store's food pile, not a pot
bool IsFoodPile(entt::entity thing)
{
	const auto* pot = Entities().TryGet<const Pot>(thing);
	return pot != nullptr &&
	       (pot->type == PotInfo::FoodPile || pot->type == PotInfo::MagicFood || pot->type == PotInfo::StoragePitFoodPile);
}

/// Whether food lying about catches the villager's eye
bool IsInterestedInFoodObject(entt::entity villager, entt::entity food)
{
	// TODO(villager-hunger-health): the game passes over things with two flags of its own whose meaning is unknown
	auto& registry = Entities();
	const auto& person = registry.Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	const auto& action = registry.Get<const LivingAction>(villager);
	const auto town = TownOf(person);
	const float reach = FoodReactionInfo().maxReactionDistance;
	std::optional<float> townDesire;
	if (town != entt::null && Locator::townDesireSystem::has_value())
	{
		townDesire = Locator::townDesireSystem::value().GetDesire(town, TownDesireInfo::ForFood);
	}
	const auto& townInfo = Locator::infoConstants::value().townDesire.at(static_cast<size_t>(TownDesireInfo::ForFood));
	return rules::IsInterestedInFood({
	    .desireForFood = needs::DesireForFood(person.food),
	    .carried = person.foodHeld,
	    .maxCarried = info.maxFoodCarried,
	    .stateInterest = StateInfo(FinalState(action)).foodInterest,
	    .distanceModifier = gutils::GetDistanceModifier(glm::distance(Position(villager), Position(food)), reach),
	    .speedUp = IsSpeedUp(food),
	    .townFoodDesire = townDesire,
	    .townTrigger = townInfo.desireTriggersVillagerAction,
	    // TODO(villager-hunger-health): a land's script may raise every villager's interest; no shipped land does
	    .interactDesire = 0.0f,
	});
}
} // namespace

uint32_t villager_food::ReactToFoodPriority(entt::entity villager, entt::entity food, uint32_t priority)
{
	auto& registry = Entities();
	if (food == entt::null || !registry.Valid(food) || !registry.AllOf<Transform>(food) ||
	    Locator::resourceStoreSystem::value().GetResource(food, ResourceType::Food) == 0 ||
	    glm::distance(Position(villager), Position(food)) > FoodReactionInfo().maxDistanceToRunAwayFromObject)
	{
		return 0;
	}
	return IsInterestedInFoodObject(villager, food) ? priority : 0;
}

uint32_t villager_food::GotoFoodReaction(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	const auto food = ecs::systems::villager_reactions::ReactingTo(villager);
	if (!food.has_value() || !registry.AllOf<Transform>(*food))
	{
		ecs::systems::villager_reactions::StopReacting(action);
		return 1;
	}
	// It makes for the edge of the food on its own side
	const auto at = glm::xz(Position(*food));
	const float angle = gutils::Get3DAngleFromXZ(at, glm::xz(Position(villager)));
	const float gap = ecs::systems::object_measures::TwoDRadius(registry, *food) +
	                  ecs::systems::object_measures::TwoDRadius(registry, villager);
	villager_home::SetupMoveTo(action, ecs::villager_routine::Polar(at, angle, gap), VillagerStates::ArrivesAtFoodReaction);
	return 1;
}

uint32_t villager_food::ArrivesAtFoodReaction(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	const auto food = ecs::systems::villager_reactions::ReactingTo(villager);
	if (!food.has_value())
	{
		ecs::systems::villager_reactions::StopReacting(action);
		return 1;
	}
	const auto& info = InfoOf(registry.Get<const Villager>(villager));
	const auto room = static_cast<int16_t>(info.maxFoodCarried - registry.Get<const Villager>(villager).foodHeld);
	// Only a pile gives up its food; a pot is passed by
	if (IsFoodPile(*food) && room > 0)
	{
		GetResourceFrom(villager, *food, ResourceType::Food, static_cast<uint16_t>(room));
	}
	const auto& person = registry.Get<const Villager>(villager);
	if (person.foodHeld > 0)
	{
		if (IsHungry(villager))
		{
			SetTopState(action, VillagerStates::EatFood);
			return 1;
		}
		const auto town = TownOf(person);
		const auto pit = WorkingStoragePit(person);
		// TODO(villager-hunger-health): with no working pit, the town's own place for food is where it would carry it
		if (!IsWorkingDisciple(villager) && town != entt::null && pit.has_value() && Locator::townDesireSystem::has_value())
		{
			const float distance =
			    gutils::GetDistanceInMetres(glm::xz(Position(villager)), villager_home::ArrivePosition(*pit));
			if (rules::CarriesFoodToStore(Locator::townDesireSystem::value().FoodShortage(town), person.foodHeld,
			                              info.maxFoodCarried, gutils::GetDistanceModifier(distance, info.maxDistCarryFoodPit)))
			{
				return villager_build::GotoStoragePitForDropOff(action);
			}
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_food::ExitFoodReaction(LivingAction& action, VillagerStates next)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	if (StateInfo(next).isReactionState == 0)
	{
		if (auto* state = registry.TryGet<LivingReaction>(villager); state != nullptr && state->reaction != 0)
		{
			ecs::systems::villager_reactions::Stop(villager, *state, false);
			state->reaction = 0;
			state->type = Reaction::None;
		}
	}
	// Leaving is never held up
	return false;
}
