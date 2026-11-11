/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/CreatureListOrder.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

entt::entity MakeCreature(ecs::Registry& registry, uint64_t made)
{
	const auto entity = registry.Create();
	registry.Assign<Creature>(entity);
	registry.Assign<LivingListPlace>(entity, LivingListPlace {.made = made});
	return entity;
}

} // namespace

TEST(CreatureListOrder, NewestFirst)
{
	ecs::Registry registry;
	const auto a = MakeCreature(registry, 1);
	const auto b = MakeCreature(registry, 2);
	const auto c = MakeCreature(registry, 3);
	EXPECT_EQ(ecs::creature_list::NewestFirst(registry), (std::vector {c, b, a}));
}

TEST(CreatureListOrder, KeepsOrderAfterTheFirstMadeGoes)
{
	// The game unlinks a creature that goes and keeps the others in their order: from A, B, C (C newest) it is C, B
	ecs::Registry registry;
	const auto a = MakeCreature(registry, 1);
	const auto b = MakeCreature(registry, 2);
	const auto c = MakeCreature(registry, 3);
	registry.Destroy(a);
	EXPECT_EQ(ecs::creature_list::NewestFirst(registry), (std::vector {c, b}));
}

TEST(CreatureListOrder, ANewCreatureAfterOneGoesIsFirst)
{
	ecs::Registry registry;
	const auto a = MakeCreature(registry, 1);
	const auto b = MakeCreature(registry, 2);
	registry.Destroy(a);
	const auto d = MakeCreature(registry, 4);
	EXPECT_EQ(ecs::creature_list::NewestFirst(registry), (std::vector {d, b}));
}

TEST(CreatureListOrder, ACreatureWithNoPlaceIsLast)
{
	ecs::Registry registry;
	const auto shown = registry.Create();
	registry.Assign<Creature>(shown);
	const auto a = MakeCreature(registry, 1);
	EXPECT_EQ(ecs::creature_list::NewestFirst(registry), (std::vector {a, shown}));
}

TEST(CreatureListOrder, OtherThingsAreNotInTheList)
{
	ecs::Registry registry;
	const auto other = registry.Create();
	registry.Assign<LivingListPlace>(other, LivingListPlace {.made = 5});
	const auto a = MakeCreature(registry, 1);
	EXPECT_EQ(ecs::creature_list::NewestFirst(registry), (std::vector {a}));
}
