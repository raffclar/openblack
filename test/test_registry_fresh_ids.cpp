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
