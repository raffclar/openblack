/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerArchetype.h"

#include <cstdlib>

#include <algorithm>

#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

entt::entity VillagerArchetype::Create([[maybe_unused]] const glm::vec3& abodePosition, const glm::vec3& position,
                                       VillagerInfo type, uint32_t age, [[maybe_unused]] bool joinTown)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(type));

	// the first draw (GameRand(10) <= 1 tries a special villager; TODO: special villagers)
	ecs::villager::RollSpecialVillager();

	// (inferred) the first angle the original is made with is not read: kept at glm's +pi
	registry.Assign<Transform>(entity, position, affine::AngleY(-glm::radians(180.0f)), glm::vec3(1.0));
	registry.Assign<Mobile>(entity);
	// the living part sets the life (info.life); the rest of Villager comes from the constructor below
	auto& villager = registry.Assign<Villager>(entity);
	villager.life = info.life;
	// whether it is a woman comes from the villager info's sex: the component mirrors it
	villager.sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	villager.tribe = info.tribeType;
	villager.number = info.villagerNumber;
	villager.task = Villager::Task::IDLE;
	villager.lifeStage = Villager::LifeStage::Adult;
	// the states start at 0 INVALID; the constructor sets the counter and the state
	registry.Assign<LivingAction>(entity, VillagerStates::InvalidState, static_cast<uint16_t>(0));
	// WallHug::speed is the distance moved per game turn (the u16 speed in MapCoords units, in metres), and
	// the speed groups are in m/s: a turn is 0.1 s
	registry.Assign<WallHug>(entity, glm::vec2(), glm::vec2(), 0.0f, GetSpeedStateSpeed(info.speedGroup.speedDefault) * 0.1f);

	// the villager constructor (ECS/Villager/VillagerCore.cpp): everything zeroed, the age (its scale draws here), food,
	// lastCheckTurn, the state counter and the water rule (in the water 16 DROWNING, else 85 CREATED)
	const uint32_t turn = ecs::villager::CurrentTurn();
	ecs::villager::Construct(entity, info, age, turn, ecs::sea_cells::IsWater(position) /* in the water */,
	                         [&](uint32_t setAge) {
		                         registry.Get<Transform>(entity).scale = glm::vec3(ecs::VillagerScaleForAge(info, setAge));
		                         age = setAge;
	                         });
	const bool child = ecs::villager::IsChild(entity);

	// children have their own meshes (childMeshHigh..Low); LOD 1 like the original
	const auto resourceId = resources::HashIdentifier(child ? info.childMeshMedium : info.stdDetail);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));

	// making a villager houses no one: the map script's handlers do (FeatureScriptCommands.cpp: CREATE_VILLAGER / _POS
	// the abode found at the position, else an abode with space in the town / the town, else the vagrants;
	// CREATE_TOWN_VILLAGER the town). The constructor left town and abode at 0

	if (std::getenv("OPENBLACK_OBJECT_INDEX_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Object index: villager {} at ({:.2f}, {:.2f}) type {} age {} meshes {} {}",
		                   ecs::object_index::Of(entity), position.x, position.z, static_cast<int>(type), age,
		                   static_cast<int>(info.highDetail), static_cast<int>(info.childMeshHigh));
	}
	// (approximate) the original's first speed comes with the first top state set (CREATED -> 163); openblack sets the
	// CREATED one now, as before (CREATED does not walk, so it is not seen)
	ecs::SetVillagerStateSpeed(entity);
	// on creation: InsertMapObject, the head of its cell's mobile list. (inferred) after the town and the house
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
