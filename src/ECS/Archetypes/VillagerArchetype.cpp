/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerArchetype.h"

#include <array>
#include <optional>
#include <tuple>

#include <glm/gtx/euler_angles.hpp>
#include <glm/vec3.hpp>

#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingBirth.h"
#include "ECS/Registry.h"
#include "ECS/SpecialVillagers.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerDrawRules.h"
#include "Graphics/MeshDetail.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

entt::entity VillagerArchetype::Create([[maybe_unused]] const glm::vec3& abodePosition, const glm::vec3& position,
                                       VillagerInfo type, uint32_t age, Joins joins)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(type));

	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	auto& random = Locator::gameRandom::value();
	// Two in ten villagers made are tried as named villagers first: a name of their sex not yet given on this land.
	// A network game makes no named villagers.
	std::optional<uint8_t> name;
	const auto& names = Locator::infoConstants::value().specialVillager;
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	if (special_villager::TriesNamed(random.GameRand(special_villager::k_Draw), networkGame))
	{
		std::array<special_villager::Row, special_villager::k_NameCount> rows {};
		for (size_t i = 0; i < rows.size(); ++i)
		{
			rows.at(i) = {.sex = names.at(i).sex, .hasName = names.at(i).name.at(0) != 0};
		}
		const auto start = random.GameRand(static_cast<uint32_t>(rows.size()));
		name = special_villager::PickName(rows, registry.Context().namedVillagersUsed, info.sex, start);
	}
	// The first number of each living thing that the game makes, before the numbers of the villager
	std::ignore = random.GameRand(living_birth::AgeDrawRange(info.startAge));
	const villager_age::NewbornKind kind {.grownUp = info.grownUpAge,
	                                      .hungryForFood = info.hungryForFood,
	                                      .processChecksEvery = info.processChecksEvery,
	                                      .ageToScale = info.ageToScale.values,
	                                      .beforeTable = villager_age::BeforeTable(info.dancingSpeed)};
	auto born = villager_age::MakeNewborn(
	    age, turn, kind, [&random](float limit) { return random.GameFloatRand(limit); },
	    [&random](uint32_t limit) { return random.GameRand(limit); });
	// A named villager is then given its name's age, unless it is being born, and sized again for it
	if (name.has_value())
	{
		registry.Context().namedVillagersUsed.at(*name) = true;
		const auto setting =
		    villager_age::SetAge(special_villager::NamedAge(age, names.at(*name).age), born.age, info.grownUpAge);
		born.age = setting.age;
		born.child = setting.child;
		born.scale =
		    villager_age::GrownScale(setting.age, info.grownUpAge, kind.ageToScale,
		                             villager_age::StartScale(setting.age, info.grownUpAge, kind.ageToScale, kind.beforeTable),
		                             [&random](float limit) { return random.GameFloatRand(limit); });
	}

	registry.Assign<Transform>(entity, position, portable_math::EulerAngleY(glm::radians(180.0f)), glm::vec3(born.scale));
	registry.Assign<Mobile>(entity);
	const float life = info.life;

	const auto lifeStage = born.child ? Villager::LifeStage::Child : Villager::LifeStage::Adult;
	const auto sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	const auto task = Villager::Task::IDLE;

	const entt::entity town =
	    joins == Joins::NearestTown ? Locator::townSystem::value().FindClosestTown(abodePosition) : entt::null;

	auto& person =
	    registry.Assign<Villager>(entity, life, villager_age::BirthTurnFor(turn, born.age), born.food, lifeStage, sex,
	                              info.tribeType, info.villagerNumber, task, entt::null, entt::null, born.lastCheckTurn);
	// The villager's place in the order of all the things made gives it its own small difference in speed
	person.creationIndex = TakeThingMadePlace();
	person.kind = type;
	// It goes to the front of the list of the living
	registry.Assign<LivingListPlace>(entity, LivingListPlace {.made = registry.Context().livingMade++});
	registry.Assign<WallHug>(
	    entity, WallHug {.goal = glm::vec2(), .yAngle = 0.0f, .speed = GetSpeedStateSpeed(info.speedGroup.speedDefault)});
	// A child made as a child wears its kind's child model; the rest their job's
	const auto resourceId = resources::HashIdentifier(born.child ? info.childMeshHigh : info.highDetail);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	// Drawn in less detail further off: a child made as a child has its high child model at every distance
	const auto detailMeshes = born.child ? std::array {resourceId, resourceId, resourceId}
	                                     : std::array {resourceId, resources::HashIdentifier(info.stdDetail),
	                                                   resources::HashIdentifier(info.lowDetail)};
	registry.Assign<DetailMeshes>(entity, detailMeshes, graphics::mesh_detail::VillagerImportance(born.age));
	// One made over water starts drowning
	const auto state = villager_clips::IsOnWater(position) ? VillagerStates::Drowning : VillagerStates::Created;
	registry.Assign<LivingAction>(entity, state, born.turnsUntilFirstDecision);
	registry.Assign<VillagerPose>(entity);
	// It joins the nearest town: into the building there that suits it best, or one of its homeless
	if (town != entt::null)
	{
		Locator::townSystem::value().AddVillagerToTown(town, entity);
	}
	// A town's first person may bring it its worship site
	if (town != entt::null && Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().PersonJoinedTown(town);
	}

	if (name.has_value())
	{
		registry.Assign<NamedVillager>(entity, *name);
	}

	return entity;
}

entt::entity VillagerArchetype::CreateLoaded(const Loaded& loaded)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(loaded.type));
	const uint32_t turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	// A villager younger than the age of growing up is a child, and it wears the models of a child
	const bool child = villager_age::IsChildAge(villager_age::AgeOf(turn, loaded.birthTurn), info.grownUpAge);

	registry.Assign<Transform>(entity, loaded.position, villager_draw::RotationOf(loaded.heading), glm::vec3(loaded.scale));
	registry.Assign<Mobile>(entity);
	const auto lifeStage = child ? Villager::LifeStage::Child : Villager::LifeStage::Adult;
	const auto sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	auto& person =
	    registry.Assign<Villager>(entity, loaded.life, loaded.birthTurn, loaded.food, lifeStage, sex, info.tribeType,
	                              info.villagerNumber, Villager::Task::IDLE, entt::null, entt::null, loaded.lastCheckTurn);
	person.creationIndex = loaded.made;
	person.kind = loaded.type;
	person.foodSpeedUp = loaded.foodSpeedUp;
	person.foodHeld = loaded.foodHeld;
	person.woodHeld = loaded.woodHeld;
	person.pregnancy = loaded.pregnancy;
	registry.Assign<LivingListPlace>(entity, LivingListPlace {.made = registry.Context().livingMade++});
	registry.Assign<WallHug>(
	    entity, WallHug {.goal = glm::vec2(), .yAngle = 0.0f, .speed = GetSpeedStateSpeed(info.speedGroup.speedDefault)});
	const auto resourceId = resources::HashIdentifier(child ? info.childMeshHigh : info.highDetail);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	const auto detailMeshes =
	    child ? std::array {resourceId, resourceId, resourceId}
	          : std::array {resourceId, resources::HashIdentifier(info.stdDetail), resources::HashIdentifier(info.lowDetail)};
	registry.Assign<DetailMeshes>(entity, detailMeshes,
	                              graphics::mesh_detail::VillagerImportance(villager_age::AgeOf(turn, loaded.birthTurn)));
	auto& action = registry.Assign<LivingAction>(entity);
	action.states = loaded.states;
	action.turnsUntilStateChange = loaded.turnsUntilStateChange;
	action.turnsSinceStateChange = loaded.turnsSinceStateChange;
	registry.Assign<VillagerPose>(entity);
	if (loaded.nameRow.has_value() && *loaded.nameRow < registry.Context().namedVillagersUsed.size())
	{
		registry.Context().namedVillagersUsed.at(*loaded.nameRow) = true;
		registry.Assign<NamedVillager>(entity, *loaded.nameRow);
	}
	return entity;
}
