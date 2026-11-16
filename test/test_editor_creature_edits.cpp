/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "Common/CreatureEditValues.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "Editor/EditorCreatureEdits.h"

using namespace openblack;
using namespace openblack::editor;
using creature_edit_values::Value;

namespace
{

constexpr uint32_t k_Stages = 14;

/// The parts of a creature that the edits change
struct FakeCreature
{
	ecs::components::Creature creature {};
	ecs::components::CreatureNeeds needs {};
	ecs::components::CreatureMindState mind {};

	void Set(Value value, float amount)
	{
		const auto inLimits = AmountInLimits(value, amount, ecs::components::ShownSize(creature), k_Stages);
		ASSERT_TRUE(inLimits.has_value());
		SetCreatureValue(value, *inLimits, creature, needs.needs, mind);
	}

	[[nodiscard]] float Of(Value value) const { return CreatureValueOf(value, creature, needs.needs, mind); }
};

} // namespace

TEST(EditorCreatureEdits, EachValueHasANameAndANumber)
{
	for (size_t index = 0; index < creature_edit_values::k_ValueCount; ++index)
	{
		const auto value = static_cast<Value>(index);
		EXPECT_EQ(creature_edit_values::ValueFrom(static_cast<uint8_t>(index)), value);
		EXPECT_EQ(creature_edit_values::ValueNamed(creature_edit_values::NameOf(value)), value);
	}
	EXPECT_FALSE(creature_edit_values::ValueFrom(static_cast<uint8_t>(creature_edit_values::k_ValueCount)).has_value());
	EXPECT_FALSE(creature_edit_values::ValueNamed("species").has_value());
}

TEST(EditorCreatureEdits, EachValueThatIsSetReadsBack)
{
	for (size_t index = 0; index < creature_edit_values::k_ValueCount; ++index)
	{
		FakeCreature fake;
		const auto value = static_cast<Value>(index);
		fake.Set(value, 0.75f);
		// A stage of growing up is a whole number
		EXPECT_FLOAT_EQ(fake.Of(value), value == Value::GrownUp ? 0.0f : 0.75f) << creature_edit_values::NameOf(value);
	}
}

TEST(EditorCreatureEdits, AnAmountStaysInTheLimitsOfItsValue)
{
	EXPECT_EQ(AmountInLimits(Value::Alignment, -3.0f, 1.0f, k_Stages), -1.0f);
	EXPECT_EQ(AmountInLimits(Value::Fatness, -0.5f, 1.0f, k_Stages), 0.0f);
	EXPECT_EQ(AmountInLimits(Value::Warmth, 2.0f, 1.0f, k_Stages), 1.0f);
	EXPECT_EQ(AmountInLimits(Value::Size, 99.0f, 1.0f, k_Stages), creature_morph::k_MaxScale);
	EXPECT_EQ(AmountInLimits(Value::Size, 0.0f, 1.0f, k_Stages), creature_morph::k_MinScale);
	// A big creature takes more energy than 1
	EXPECT_EQ(AmountInLimits(Value::Energy, 5.0f, 1.5f, k_Stages), 1.5f);
	EXPECT_EQ(AmountInLimits(Value::Energy, 5.0f, 0.5f, k_Stages), 1.0f);
	EXPECT_EQ(AmountInLimits(Value::GrownUp, 20.0f, 1.0f, k_Stages), 13.0f);
	EXPECT_EQ(AmountInLimits(Value::GrownUp, 3.7f, 1.0f, k_Stages), 3.0f);
}

TEST(EditorCreatureEdits, AnAmountThatIsNotAFiniteNumberIsRefused)
{
	EXPECT_FALSE(AmountInLimits(Value::Alignment, std::numeric_limits<float>::quiet_NaN(), 1.0f, k_Stages).has_value());
	EXPECT_FALSE(AmountInLimits(Value::Size, std::numeric_limits<float>::infinity(), 1.0f, k_Stages).has_value());
}

TEST(EditorCreatureEdits, AnEditChangesOnlyItsOwnValue)
{
	FakeCreature fake;
	const FakeCreature before;
	fake.Set(Value::Thirst, 0.9f);
	EXPECT_FLOAT_EQ(fake.needs.needs.dehydration, 0.9f);
	for (size_t index = 0; index < creature_edit_values::k_ValueCount; ++index)
	{
		const auto value = static_cast<Value>(index);
		if (value != Value::Thirst)
		{
			EXPECT_EQ(fake.Of(value), before.Of(value)) << creature_edit_values::NameOf(value);
		}
	}
}

TEST(EditorCreatureEdits, TheRuleTakesANewCreatureOnlyWithTheValuesOfTheCreatureTools)
{
	using player_commands::EditorCreatureSpawn;
	const EditorCreatureSpawn good {.species = static_cast<uint8_t>(CreatureType::Tiger),
	                                .owner = static_cast<uint8_t>(PlayerNames::PLAYER_TWO),
	                                .scale = creature_morph::k_MaxScale,
	                                .alignment = -1.0f,
	                                .fatness = 1.0f,
	                                .strength = 0.0f};
	EXPECT_FALSE(CreatureSpawnRefusal(good).has_value());
	const auto refused = [&good](auto change) {
		auto spawn = good;
		change(spawn);
		return CreatureSpawnRefusal(spawn).has_value();
	};
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.species = 0; }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.species = static_cast<uint8_t>(CreatureType::_COUNT); }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.owner = static_cast<uint8_t>(PlayerNames::_COUNT); }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.scale = creature_morph::k_MinScale / 2.0f; }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.scale = creature_morph::k_MaxScale * 2.0f; }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.alignment = 1.5f; }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.fatness = -0.1f; }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.strength = 1.1f; }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.position.x = std::numeric_limits<float>::quiet_NaN(); }));
	EXPECT_TRUE(refused([](EditorCreatureSpawn& spawn) { spawn.radians = std::numeric_limits<float>::infinity(); }));
}
