/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapProduction.h"
#include "ECS/Registry.h"
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/WorldObjects.h"
#include "Fire/FireModel.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

constexpr glm::vec3 k_Here {105.0f, 0.0f, 105.0f};
constexpr float k_Combustion = 100.0f;

/// The fire's test of what takes it: a burn that reaches an object, and the heat of the fires near a held thing.
class FireReceivers: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::entitiesMap::emplace<ecs::MapProduction>();
		_info = std::make_unique<InfoConstants>();
		for (GObjectInfo* row : {static_cast<GObjectInfo*>(&_info->fieldType.at(0)),
		                         static_cast<GObjectInfo*>(&_info->pot.at(static_cast<size_t>(PotInfo::FoodPot))),
		                         static_cast<GObjectInfo*>(&_info->fishFarm)})
		{
			row->combustionTemperature = k_Combustion;
			row->heatCapacity = 1.0f;
			row->defenceMultiplierBurn = 1.0f;
		}
		Locator::infoConstants::emplace(*_info);
		Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	}
	void TearDown() override
	{
		Locator::fireSystem::reset();
		Locator::infoConstants::reset();
		Locator::entitiesMap::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity Made()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, k_Here, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	/// The object burns at this temperature, a blaze of its own
	static void Burn(entt::entity object, float temperature)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto& fire = registry.Assign<Fire>(object);
		fire.root = object;
		fire.state.temperature = temperature;
		fire.state.previous = temperature;
		registry.Assign<FireGroup>(object, FireGroup {.members = {object}, .firemen = {}});
	}

	static float TemperatureOf(entt::entity object) { return Locator::fireSystem::value().GetTemperature(object); }

	std::unique_ptr<InfoConstants> _info;
};

TEST_F(FireReceivers, ABurnHeatsABurningFieldThatHasNoFood)
{
	// A field with no food does not take the burn of a fire, but a burn reaches its fire all the same
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = Made();
	registry.Assign<Field>(field);
	Burn(field, 150.0f);
	ASSERT_FALSE(ecs::world_objects::TakesEffect(field, 1000.0f, 0.0f));
	Locator::fireSystem::value().ApplyBurn(field, 1000.0f, std::nullopt);
	EXPECT_GT(TemperatureOf(field), 150.0f);
}

TEST_F(FireReceivers, WaterCoolsABurningPotThatHoldsNothing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pot = Made();
	registry.Assign<Pot>(pot, Pot {.amount = 0, .maxAmount = 100});
	Burn(pot, 300.0f);
	ASSERT_FALSE(ecs::world_objects::TakesEffect(pot, -50.0f, 0.0f));
	Locator::fireSystem::value().ApplyBurn(pot, -50.0f, std::nullopt);
	EXPECT_LT(TemperatureOf(pot), 300.0f);
}

TEST_F(FireReceivers, ABurnSetsAlightOnlyWhatTakesTheBurnOfAFire)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto field = Made();
	auto& crop = registry.Assign<Field>(field).crop;
	auto& fires = Locator::fireSystem::value();
	// No food: no fire, however hot the burn
	fires.ApplyBurn(field, 1000.0f, std::nullopt);
	EXPECT_FALSE(registry.AllOf<Fire>(field));
	// A sown field with food catches
	crop.age = 1.0f;
	crop.food = 5.0f;
	fires.ApplyBurn(field, 1000.0f, std::nullopt);
	EXPECT_TRUE(registry.AllOf<Fire>(field));
	EXPECT_GT(TemperatureOf(field), fire::k_AmbientTemperature);
}

TEST_F(FireReceivers, AFireHeatsAHeldThingOnlyWhenItTakesTheBurnOfAFire)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A fish farm burns at its hottest beside the hand
	const auto farm = Made();
	registry.Assign<FishFarm>(farm);
	Burn(farm, 2.0f * k_Combustion);
	Locator::entitiesMap::value().Sync();
	// The hand holds a field that burns a little: a field with no food does not take the burn of a fire
	const auto held = Made();
	auto& crop = registry.Assign<Field>(held).crop;
	registry.Assign<InHand>(held);
	Burn(held, fire::k_AmbientTemperature);
	auto& fires = Locator::fireSystem::value();
	fires.HeatHeldObject(held);
	EXPECT_FLOAT_EQ(TemperatureOf(held), fire::k_AmbientTemperature);
	EXPECT_FLOAT_EQ(TemperatureOf(farm), 2.0f * k_Combustion);
	// With food, the fire beside it heats it
	crop.age = 1.0f;
	crop.food = 5.0f;
	fires.HeatHeldObject(held);
	EXPECT_GT(TemperatureOf(held), fire::k_AmbientTemperature);
}

} // namespace
