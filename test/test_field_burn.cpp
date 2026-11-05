/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Field.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr float k_FullFood = 200.0f;

/// A wheat field that needs one sowing, is ripe at age 10 and holds 200 food when full
class FieldBurn: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		auto& wheat = _info->fieldType.at(static_cast<size_t>(FieldTypeInfo::Wheat));
		wheat.timesToSow = 1.0f;
		wheat.ageGrowth = 5.0f;
		wheat.ageRecolt = 10.0f;
		wheat.totalFoodInField = k_FullFood;
		wheat.ratioBeforeRipe = 0.5f;
		Locator::infoConstants::emplace(*_info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	static entt::entity MadeField(field_crop::Crop crop)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& field = registry.Assign<Field>(entity);
		field.town = -1;
		field.type = FieldTypeInfo::Wheat;
		field.crop = crop;
		return entity;
	}

	static const field_crop::Crop& CropOf(entt::entity field)
	{
		return Locator::entitiesRegistry::value().Get<const Field>(field).crop;
	}

	std::unique_ptr<InfoConstants> _info;
};

TEST_F(FieldBurn, HarmThatIsNotFireTakesNoFoodAndNoLifeFromAField)
{
	const auto field = MadeField({.timesSown = 1, .age = 20.0f, .food = 100.0f});
	EXPECT_FLOAT_EQ(ecs::world_objects::ReduceLife(field, 0.5f), 1.0f);
	EXPECT_FLOAT_EQ(CropOf(field).food, 100.0f);
}

TEST_F(FieldBurn, AFireTakesTheDamageTimesTheFullFoodInWholeUnits)
{
	const auto field = MadeField({.timesSown = 1, .age = 20.0f, .food = 100.0f});
	ecs::systems::FieldSystem fields;
	// 0.1 x 200 is 20
	fields.BurnFood(field, 0.1f);
	EXPECT_FLOAT_EQ(CropOf(field).food, 80.0f);
	// 0.0123 x 200 is 2.46, of which the fire takes 2
	fields.BurnFood(field, 0.0123f);
	EXPECT_FLOAT_EQ(CropOf(field).food, 78.0f);
}

TEST_F(FieldBurn, AFireTakesNoFoodFromAFieldThatIsNotSown)
{
	const auto field = MadeField({.timesSown = 0, .age = 20.0f, .food = 100.0f});
	ecs::systems::FieldSystem fields;
	fields.BurnFood(field, 0.1f);
	EXPECT_FLOAT_EQ(CropOf(field).food, 100.0f);
}

TEST_F(FieldBurn, AFireTakesMoreFromAnUnripeCrop)
{
	const auto field = MadeField({.timesSown = 1, .age = 2.0f, .food = 100.0f});
	ecs::systems::FieldSystem fields;
	// 20 taken, and 20 x 0.5 more lost before the crop is ripe
	fields.BurnFood(field, 0.1f);
	EXPECT_FLOAT_EQ(CropOf(field).food, 70.0f);
	EXPECT_EQ(CropOf(field).timesSown, 1);
}

TEST_F(FieldBurn, ARipeFieldThatAFireEmptiesMustBeSownAgain)
{
	const auto field = MadeField({.timesSown = 1, .age = 20.0f, .food = 10.0f});
	ecs::systems::FieldSystem fields;
	fields.BurnFood(field, 0.1f);
	EXPECT_FLOAT_EQ(CropOf(field).food, 0.0f);
	EXPECT_EQ(CropOf(field).timesSown, 0);
	EXPECT_FLOAT_EQ(CropOf(field).age, 0.0f);
}
} // namespace
