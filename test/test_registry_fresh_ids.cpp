/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A reset of the registry with fresh ids gives the entities the ids of a new program, as a network game's start needs

#include <optional>
#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Registry.h"

using openblack::ecs::Registry;

namespace
{

struct Mark
{
	int value {0};
};

struct Other
{
	int value {0};
};

std::vector<entt::entity> MakeSome(Registry& registry, int count)
{
	std::vector<entt::entity> made;
	for (int i = 0; i < count; ++i)
	{
		const auto entity = registry.Create();
		registry.Assign<Mark>(entity, i);
		made.push_back(entity);
	}
	return made;
}

} // namespace

TEST(RegistryFreshIds, APlainResetUsesTheOldIdsAgain)
{
	Registry fresh;
	const auto first = MakeSome(fresh, 5);
	Registry used;
	MakeSome(used, 9);
	used.Reset();
	EXPECT_NE(MakeSome(used, 5), first);
}

TEST(RegistryFreshIds, AResetWithFreshIdsGivesTheIdsOfANewRegistry)
{
	Registry fresh;
	const auto first = MakeSome(fresh, 5);
	Registry used;
	MakeSome(used, 9);
	used.SetFreshIdsAtNextReset();
	used.Reset();
	EXPECT_EQ(MakeSome(used, 5), first);
	EXPECT_EQ(used.Underlying().storage<Mark>().size(), 5u);
	// Only the next reset is fresh
	used.Reset();
	EXPECT_NE(MakeSome(used, 5), first);
}

TEST(RegistryFindFirst, TheFirstEntityIsTheFirstThatEachFinds)
{
	Registry registry;
	const auto made = MakeSome(registry, 12);
	// Two components of other sizes, so that the view goes through the smaller one, with gaps from removed entities
	for (size_t i = 0; i < made.size(); i += 2)
	{
		registry.Assign<Other>(made[i], static_cast<int>(i % 3));
	}
	registry.Destroy(made[4]);
	registry.Assign<Other>(made[5], 2);
	for (const int wanted : {0, 1, 2, 3})
	{
		std::optional<entt::entity> each;
		registry.Each<const Mark, const Other>([&each, wanted](entt::entity entity, const Mark&, const Other& other) {
			if (other.value == wanted && !each.has_value())
			{
				each = entity;
			}
		});
		const auto first = registry.FindFirst<const Mark, const Other>(
		    [wanted](entt::entity, const Mark&, const Other& other) { return other.value == wanted; });
		EXPECT_EQ(first, each) << wanted;
	}
	EXPECT_FALSE(registry.FindFirst<const Mark>([](entt::entity, const Mark& mark) { return mark.value > 100; }).has_value());
}

TEST(RegistryComponentLookup, ALookupGivesTheAnswersOfTheRegistry)
{
	Registry registry;
	// No store of Other exists yet: the lookup finds nothing
	const auto made = MakeSome(registry, 6);
	const openblack::ecs::ComponentLookup<Other> none(registry);
	EXPECT_FALSE(none.Has(made[0]));
	EXPECT_EQ(none.TryGet(made[0]), nullptr);
	registry.Assign<Other>(made[1], 7);
	registry.Assign<Other>(made[4], 9);
	registry.Destroy(made[2]);
	const openblack::ecs::ComponentLookup<Mark> marks(registry);
	const openblack::ecs::ComponentLookup<Other> others(registry);
	// The lookup stays right when the walk adds a component to a store that exists
	registry.Assign<Other>(made[5], 11);
	for (const auto entity : made)
	{
		if (entity == made[2])
		{
			EXPECT_FALSE(marks.Has(entity));
			continue;
		}
		EXPECT_EQ(others.Has(entity), registry.AllOf<Other>(entity));
		EXPECT_EQ(others.TryGet(entity), registry.TryGet<const Other>(entity));
		EXPECT_EQ(marks.TryGet(entity), registry.TryGet<const Mark>(entity));
	}
	EXPECT_EQ(others.TryGet(made[5])->value, 11);
}

TEST(RegistryStores, TheKeptStoresGiveTheAnswersOfTheRegistryBeneath)
{
	struct Late
	{
		int value {0};
	};
	struct Tag
	{
	};
	Registry registry;
	Registry second;
	const auto& view = registry;
	const auto made = MakeSome(registry, 4);
	const auto& beneath = registry.Underlying();
	// No store of Late exists yet: the registry finds nothing, and the search makes no store
	EXPECT_FALSE(view.AllOf<Late>(made[0]));
	EXPECT_FALSE((view.AnyOf<Late, Tag>(made[0])));
	EXPECT_EQ(view.TryGet<Late>(made[0]), nullptr);
	EXPECT_EQ(beneath.storage<Late>(), nullptr);
	// A store made after the search is found by the next search
	registry.Assign<Late>(made[1], 3);
	registry.Assign<Tag>(made[2]);
	for (const auto entity : made)
	{
		EXPECT_EQ(view.AllOf<Late>(entity), beneath.all_of<Late>(entity));
		EXPECT_EQ((view.AnyOf<Late, Tag>(entity)), (beneath.any_of<Late, Tag>(entity)));
		EXPECT_EQ((view.AllOf<Late, Tag>(entity)), (beneath.all_of<Late, Tag>(entity)));
		EXPECT_EQ(view.TryGet<Late>(entity), beneath.try_get<Late>(entity));
		EXPECT_EQ(registry.TryGet<Late>(entity), beneath.try_get<Late>(entity));
		EXPECT_EQ(registry.TryGet<const Late>(entity), beneath.try_get<Late>(entity));
	}
	EXPECT_EQ(&view.Get<Late>(made[1]), &beneath.get<Late>(made[1]));
	registry.Get<Late>(made[1]).value = 8;
	EXPECT_EQ(view.Get<const Late>(made[1]).value, 8);
	// Another registry has its own stores
	const auto other = second.Create();
	EXPECT_FALSE(second.AllOf<Late>(other));
	second.Assign<Late>(other, 5);
	EXPECT_EQ(second.Get<Late>(other).value, 5);
	EXPECT_EQ(view.TryGet<Late>(other), beneath.try_get<Late>(other));
	// An empty registry keeps its stores, so the kept store stays right
	registry.Reset();
	EXPECT_FALSE(view.AllOf<Late>(made[1]));
	const auto again = registry.Create();
	registry.Assign<Late>(again, 6);
	EXPECT_EQ(view.TryGet<Late>(again), beneath.try_get<Late>(again));
	EXPECT_EQ(view.Get<Late>(again).value, 6);
}
