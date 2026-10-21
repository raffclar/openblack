/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureMorph.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;
using ecs::archetypes::CreatureArchetype;

namespace
{
entt::entity MakeCreature(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Creature>(entity,
	                          Creature {.species = CreatureType::Tiger, .alignment = -0.3f, .fatness = 0.8f, .strength = 0.2f});
	// The body still shows a strong, good creature, as it did before the strength faded
	registry.Assign<CreatureMorph>(entity, CreatureMorph {.shownFatness = 0.5f, .strengthLook = 1.0f, .alignmentLook = 0.9f});
	return entity;
}
} // namespace

TEST(CreatureLooks, TheBodyTakesTheLooksOfTheCreature)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	CreatureArchetype::TakeLooks(registry, creature);
	const auto& morph = registry.Get<const CreatureMorph>(creature);
	EXPECT_EQ(morph.shownFatness, creature_morph::EaseFatness(0.5f, 0.8f));
	EXPECT_EQ(morph.alignmentLook, -0.3f);
	EXPECT_EQ(morph.strengthLook, creature_morph::StrengthLook(0.2f, CreatureArchetype::SpeciesStrength(CreatureType::Tiger)));
}

TEST(CreatureLooks, EachTimeTheShownFatnessMovesOneStep)
{
	ecs::Registry registry;
	const auto creature = MakeCreature(registry);
	CreatureArchetype::TakeLooks(registry, creature);
	CreatureArchetype::TakeLooks(registry, creature);
	EXPECT_EQ(registry.Get<const CreatureMorph>(creature).shownFatness,
	          creature_morph::EaseFatness(creature_morph::EaseFatness(0.5f, 0.8f), 0.8f));
}

TEST(CreatureLooks, AThingWithNoBodyIsLeftAlone)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Creature>(thing);
	CreatureArchetype::TakeLooks(registry, thing);
	EXPECT_FALSE(registry.AllOf<CreatureMorph>(thing));
}
