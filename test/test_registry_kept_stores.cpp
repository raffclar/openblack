/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The registry keeps the stores of the component kinds that it found. Its answers are the answers of EnTT's registry
// before and after a kind gets its first component, after a reset or a clear, in a registry made again, with two
// registries alive and from many threads at the same time.

#include <atomic>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "Common/WorkerPool.h"
#include "ECS/Registry.h"

using openblack::WorkerPool;
using openblack::ecs::Registry;

namespace
{

struct Size
{
	int value {0};
};

struct Colour
{
	int value {0};
};

struct Late
{
	int value {0};
};

/// The answers of the registry and of EnTT's registry under it are the same for each kind
void ExpectSameAnswers(const Registry& registry, entt::entity entity)
{
	const auto& underlying = registry.Underlying();
	EXPECT_EQ(registry.TryGet<Size>(entity), underlying.try_get<Size>(entity));
	EXPECT_EQ(registry.TryGet<const Colour>(entity), underlying.try_get<const Colour>(entity));
	EXPECT_EQ(registry.TryGet<Late>(entity), underlying.try_get<Late>(entity));
	EXPECT_EQ(registry.AllOf<Size>(entity), underlying.all_of<Size>(entity));
	EXPECT_EQ((registry.AllOf<Size, Colour>(entity)), (underlying.all_of<Size, Colour>(entity)));
	EXPECT_EQ((registry.AnyOf<Colour, Late>(entity)), (underlying.any_of<Colour, Late>(entity)));
}

} // namespace

TEST(RegistryKeptStores, TheAnswersAreThoseOfTheRegistryBeforeAndAfterAKindComes)
{
	Registry registry;
	const auto first = registry.Create();
	registry.Assign<Size>(first, 3);
	// No entity has the late kind yet, so the registry has no store of it
	ExpectSameAnswers(registry, first);
	EXPECT_EQ(registry.TryGet<Late>(first), nullptr);

	const auto second = registry.Create();
	registry.Assign<Late>(second, 7);
	registry.Assign<Colour>(second, 2);
	ExpectSameAnswers(registry, first);
	ExpectSameAnswers(registry, second);
	ASSERT_NE(registry.TryGet<Late>(second), nullptr);
	EXPECT_EQ(registry.TryGet<Late>(second)->value, 7);
	EXPECT_EQ(registry.Get<Size>(first).value, 3);
	EXPECT_EQ(std::as_const(registry).Get<const Colour>(second).value, 2);

	registry.Remove<Late>(second);
	ExpectSameAnswers(registry, second);
	EXPECT_EQ(registry.TryGet<Late>(second), nullptr);
}

TEST(RegistryKeptStores, AWriteThroughTheAnswerChangesTheComponent)
{
	Registry registry;
	const auto entity = registry.Create();
	registry.Assign<Size>(entity, 1);
	registry.Assign<Colour>(entity, 4);
	registry.TryGet<Size>(entity)->value = 5;
	registry.Get<Colour>(entity).value = 6;
	auto [size, colour] = registry.Get<Size, Colour>(entity);
	EXPECT_EQ(size.value, 5);
	EXPECT_EQ(colour.value, 6);
	const auto [maybeSize, maybeLate] = registry.TryGet<Size, Late>(entity);
	EXPECT_EQ(maybeSize, &size);
	EXPECT_EQ(maybeLate, nullptr);
}

TEST(RegistryKeptStores, TheAnswersStayTheSameAfterAReset)
{
	Registry registry;
	const auto before = registry.Create();
	registry.Assign<Size>(before, 1);
	ASSERT_NE(registry.TryGet<Size>(before), nullptr);
	registry.Reset();
	EXPECT_FALSE(registry.Valid(before));
	const auto after = registry.Create();
	EXPECT_EQ(registry.TryGet<Size>(after), nullptr);
	registry.Assign<Size>(after, 2);
	ExpectSameAnswers(registry, after);
	EXPECT_EQ(registry.TryGet<Size>(after)->value, 2);
}

TEST(RegistryKeptStores, ManyThreadsGetTheSameAnswers)
{
	Registry registry;
	std::vector<entt::entity> entities;
	for (int i = 0; i < 2000; ++i)
	{
		const auto entity = registry.Create();
		registry.Assign<Size>(entity, i);
		if (i % 3 == 0)
		{
			registry.Assign<Colour>(entity, i);
		}
		entities.push_back(entity);
	}
	const Registry& reader = registry;
	std::atomic<size_t> wrong {0};
	WorkerPool pool(4);
	pool.ForEach(entities.size(), [&](size_t index, size_t /*lane*/) {
		const auto entity = entities[index];
		const auto* size = reader.TryGet<const Size>(entity);
		const auto* colour = reader.TryGet<const Colour>(entity);
		const bool right = size != nullptr && size->value == static_cast<int>(index) &&
		                   (colour != nullptr) == (index % 3 == 0) && !reader.AllOf<Late>(entity);
		if (!right)
		{
			++wrong;
		}
	});
	EXPECT_EQ(wrong.load(), 0u);
}

TEST(RegistryKeptStores, EachRegistryKeepsItsOwnStores)
{
	// Two registries are alive at the same time, as the server's world and a client's world are
	Registry server;
	Registry client;
	const auto onServer = server.Create();
	server.Assign<Size>(onServer, 1);
	const auto onClient = client.Create();
	client.Assign<Size>(onClient, 2);
	client.Assign<Late>(onClient, 3);
	ASSERT_NE(server.TryGet<Size>(onServer), nullptr);
	ASSERT_NE(client.TryGet<Size>(onClient), nullptr);
	EXPECT_EQ(server.TryGet<Size>(onServer)->value, 1);
	EXPECT_EQ(client.TryGet<Size>(onClient)->value, 2);
	EXPECT_EQ(server.TryGet<Late>(onServer), nullptr);
	EXPECT_EQ(client.TryGet<Late>(onClient)->value, 3);
	ExpectSameAnswers(server, onServer);
	ExpectSameAnswers(client, onClient);
}

TEST(RegistryKeptStores, ARegistryMadeAgainFindsItsNewStores)
{
	// A land or a saved world that loads can make the registry again at the same place in memory
	std::optional<Registry> registry;
	registry.emplace();
	const auto before = registry->Create();
	registry->Assign<Size>(before, 1);
	registry->Assign<Colour>(before, 2);
	ASSERT_NE(registry->TryGet<Colour>(before), nullptr);
	registry.reset();
	registry.emplace();
	const auto after = registry->Create();
	EXPECT_EQ(registry->TryGet<Colour>(after), nullptr);
	EXPECT_FALSE(registry->AllOf<Size>(after));
	registry->Assign<Colour>(after, 5);
	ExpectSameAnswers(*registry, after);
	EXPECT_EQ(registry->TryGet<Colour>(after)->value, 5);
}

TEST(RegistryKeptStores, AClearOfTheEntitiesKeepsTheAnswersRight)
{
	Registry registry;
	const auto first = registry.Create();
	registry.Assign<Size>(first, 1);
	ASSERT_TRUE(registry.AllOf<Size>(first));
	registry.Underlying().clear();
	EXPECT_FALSE(registry.Valid(first));
	const auto second = registry.Create();
	EXPECT_FALSE(registry.AllOf<Size>(second));
	registry.Assign<Size>(second, 4);
	ExpectSameAnswers(registry, second);
	EXPECT_EQ(registry.Get<Size>(second).value, 4);
}
