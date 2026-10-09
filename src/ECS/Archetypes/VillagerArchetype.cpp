/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerArchetype.h"

#include <glm/gtx/euler_angles.hpp>
#include <glm/vec3.hpp>

#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerClips.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

entt::entity VillagerArchetype::Create([[maybe_unused]] const glm::vec3& abodePosition, const glm::vec3& position,
                                       VillagerInfo type, uint32_t age)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(type));

	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	auto& random = Locator::gameRandom::value();
	const auto born = villager_age::MakeNewborn(
	    age, turn,
	    {.grownUp = info.grownUpAge,
	     .hungryForFood = info.hungryForFood,
	     .processChecksEvery = info.processChecksEvery,
	     .ageToScale = info.ageToScale.values},
	    [&random](float limit) { return random.GameFloatRand(limit); },
	    [&random](uint32_t limit) { return random.GameRand(limit); });

	registry.Assign<Transform>(entity, position, glm::eulerAngleY(glm::radians(180.0f)), glm::vec3(born.scale));
	registry.Assign<Mobile>(entity);
	const uint32_t health = 100;

	const auto lifeStage = born.child ? Villager::LifeStage::Child : Villager::LifeStage::Adult;
	const auto sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	const auto task = Villager::Task::IDLE;

	// TODO(bwrsandman): Might be better to make a FindClosestAbode
	const entt::entity town = Locator::townSystem::value().FindClosestTown(abodePosition);
	entt::entity abode = entt::null;
	if (town != entt::null)
	{
		abode = Locator::townSystem::value().FindAbodeWithSpace(town);
	}
	if (abode != entt::null)
	{
		registry.Get<Abode>(abode).inhabitants.insert(entity);
	}

	registry.Assign<Villager>(entity, health, villager_age::BirthTurnFor(turn, born.age), born.food, lifeStage, sex,
	                          info.tribeType, info.villagerNumber, task, town, abode, born.lastCheckTurn);
	registry.Assign<WallHug>(entity, glm::vec2(), glm::vec2(), 0.0f, GetSpeedStateSpeed(info.speedGroup.speedDefault));
	// A child made as a child wears its kind's child model; the rest their job's
	const auto resourceId = resources::HashIdentifier(born.child ? info.childMeshHigh : info.highDetail);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	// One made over water starts drowning
	const auto state = villager_clips::IsOnWater(position) ? VillagerStates::Drowning : VillagerStates::Created;
	registry.Assign<LivingAction>(entity, state, born.turnsUntilFirstDecision);
	registry.Assign<VillagerPose>(entity);
	// One joining a town with no home in it to go to is one of its homeless
	if (town != entt::null && abode == entt::null)
	{
		registry.Get<Villager>(entity).town = entt::null;
		Locator::townSystem::value().AddHomelessVillagerToTown(town, entity);
	}

	return entity;
}
