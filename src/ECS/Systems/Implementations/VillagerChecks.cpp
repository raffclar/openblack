/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerChecks.h"

#include "Common/GameRandom.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerCheckRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerBreeding.h"
#include "VillagerFire.h"
#include "VillagerFood.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::villager_check_rules;
namespace villager_fire = openblack::ecs::villager_fire;
namespace villager_age = openblack::ecs::villager_age;
namespace world_objects = openblack::ecs::world_objects;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

/// It may die of old age; whether it did
bool CheckDeathFromOldAge(entt::entity villager)
{
	const auto& person = Entities().Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	auto& random = Locator::gameRandom::value();
	if (!villager_age::DiesOfOldAge(
	        villager_age::AgeNow(person), {.grownUp = info.grownUpAge, .old = info.oldAge, .oldest = info.retirementAge},
	        [&random](float limit) { return random.GameFloatRand(limit); },
	        [&random](uint32_t limit) { return random.GameRand(limit); }))
	{
		return false;
	}
	villager_fire::DieByEffect(villager, villager_fire::DeathCause {.reason = DeathReason::OldAge, .weight = info.life});
	return true;
}

float GrownScaleFor(const Villager& person, uint32_t age, float scale)
{
	const auto& info = InfoOf(person);
	auto& random = Locator::gameRandom::value();
	return villager_age::GrownScale(age, info.grownUpAge, info.ageToScale.values, scale,
	                                [&random](float limit) { return random.GameFloatRand(limit); });
}

void SetScale(entt::entity villager, float scale)
{
	Entities().Get<Transform>(villager).scale = glm::vec3(scale);
	Entities().SetDirty();
}

/// A child grows on the growth turns its checks fall on, and once old enough becomes an adult: the youngest adult's
/// age and an adult's size, counted among the adults of its home and town, and off home to change into its adult self
void CheckChildGrownUp(LivingAction& action, uint32_t turn)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	auto& person = registry.Get<Villager>(villager);
	const auto& info = InfoOf(person);
	const auto age = villager_age::AgeNow(person);
	switch (villager_age::CheckChild(age, info.grownUpAge, turn))
	{
	case villager_age::ChildGrowth::None:
		return;
	case villager_age::ChildGrowth::Grows:
		ecs::villager_checks::GrowForAge(villager);
		return;
	case villager_age::ChildGrowth::GrowsUp:
		break;
	}
	// Its home's and town's counts of children and adults follow from its life stage. Its model stays the child's
	// until it changes at home.
	person.lifeStage = Villager::LifeStage::Adult;
	const auto grownUpAge = villager_age::GrownUpAge(info.grownUpAge);
	villager_age::SetBirthTurnForAge(person, grownUpAge);
	SetScale(villager,
	         GrownScaleFor(person, grownUpAge, villager_age::StartScale(grownUpAge, info.grownUpAge, info.ageToScale.values)));
	// It stops following its mother
	person.mother = entt::null;
	ecs::villager_home::LookForBetterHome(action);
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::GoHomeAndChange,
	                                                      false);
}
} // namespace

void ecs::villager_checks::GrowForAge(entt::entity villager)
{
	const auto& person = Entities().Get<const Villager>(villager);
	SetScale(villager, GrownScaleFor(person, villager_age::AgeNow(person), Entities().Get<const Transform>(villager).scale.x));
}

void ecs::villager_checks::CheckEveryTime(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	if (registry.AllOf<ScriptControlled>(villager) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& living = Locator::livingActionSystem::value();
	const auto top = living.VillagerGetState(action, LivingAction::Index::Top);
	const auto states = rules::DrainAndCheckStates(top, living.VillagerGetState(action, LivingAction::Index::Final),
	                                               living.VillagerGetFinalState(action), StateInfo(top).movesWithGround != 0);
	world_objects::ReduceLife(villager, StateInfo(states.drain).lifeDrain);
	const auto& row = StateInfo(states.checks);
	if (row.doesChecks == 0)
	{
		// TODO(villager-hunger-health): a working disciple waiting for work decides afresh when its town has some for it
		return;
	}
	if (world_objects::LifeOf(villager) == 0.0f)
	{
		// TODO(villager-hunger-health): a worshipper at its worship site dies of chanting too, once villagers worship
		const auto reason = rules::ExhaustionDeath(rules::ChantedLast(living.VillagerGetFinalState(action)));
		villager_fire::DieByEffect(villager,
		                           villager_fire::DeathCause {.reason = reason, .killer = world_objects::PlayerOf(villager)});
		return;
	}
	const auto& person = registry.Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	const auto since = turn - person.lastCheckTurn;
	if (!rules::ChecksDue(since, info.processChecksEvery))
	{
		return;
	}
	if (rules::OldAgeDue(turn, entt::to_integral(entt::to_entity(villager)), since) && CheckDeathFromOldAge(villager))
	{
		return;
	}
	// TODO(villager-hunger-health): a villager playing football is not sent home hurt; openblack has no football yet
	if (rules::WoundedGoesHome({
	        .life = world_objects::LifeOf(villager),
	        .goHomeLife = info.damageThresholdToGoHome,
	        .atHome = registry.AllOf<AtHome>(villager),
	        .stateSendsHome = row.woundedGoesHome != 0,
	        .stateKeepsWounded = row.woundedStays != 0,
	        .goingForFood =
	            states.checks == VillagerStates::GotoFoodReaction || states.checks == VillagerStates::ArrivesAtFoodReaction,
	        .food = person.food,
	        .hungryForFood = info.hungryForFood,
	    }))
	{
		living.VillagerSetState(action, LivingAction::Index::Top, VillagerStates::GoHome, false);
	}
	if (person.lifeStage == Villager::LifeStage::Child)
	{
		CheckChildGrownUp(action, turn);
	}
	// A woman with child comes nearer her time; a girl grown up in this check is a woman already
	if (person.sex == Villager::Sex::FEMALE && person.lifeStage != Villager::LifeStage::Child)
	{
		ecs::villager_breeding::WomanSpecial(action);
	}
	ecs::villager_food::CheckHungry(action);
}
