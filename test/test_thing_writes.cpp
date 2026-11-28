/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The registry marks each thing whose place, model or map cells a request can change, so that a reader looks only at
// these things

#include <cstdint>

#include <algorithm>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/registry.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/DeadTree.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ThingWrites.h"

using openblack::ecs::Registry;
using openblack::ecs::ThingWrites;
using openblack::ecs::components::DeadTree;
using openblack::ecs::components::MapCellResident;
using openblack::ecs::components::Mesh;
using openblack::ecs::components::Transform;

namespace
{

constexpr auto k_Reader = ThingWrites::Reader::LandFootprints;
constexpr auto k_OtherReader = ThingWrites::Reader::LandShadowBake;

uint32_t Number(entt::entity entity)
{
	return static_cast<uint32_t>(entt::to_entity(entity));
}

std::vector<uint32_t> Numbers(std::initializer_list<entt::entity> entities)
{
	std::vector<uint32_t> numbers;
	for (const auto entity : entities)
	{
		numbers.push_back(Number(entity));
	}
	std::ranges::sort(numbers);
	return numbers;
}

/// The registry with its marks started, and a thing with a place and a model
struct Fixture
{
	Registry registry;
	entt::entity thing {registry.Create()};
	entt::entity other {registry.Create()};

	Fixture()
	{
		registry.Assign<Transform>(thing);
		registry.Assign<Mesh>(thing);
		registry.Assign<Transform>(other);
		EXPECT_FALSE(registry.ThingChanges().Take(k_Reader).has_value());
	}
};

} // namespace

TEST(ThingWrites, TheFirstTakeGivesNoMarks)
{
	ThingWrites writes;
	writes.Mark(entt::entity {3});
	EXPECT_FALSE(writes.Take(k_Reader).has_value());
	const auto second = writes.Take(k_Reader);
	ASSERT_TRUE(second.has_value());
	EXPECT_TRUE(second->empty());
}

TEST(ThingWrites, ATakeGivesEachMarkedNumberOneTimeInOrder)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	writes.Mark(entt::entity {700});
	writes.Mark(entt::entity {5});
	writes.Mark(entt::entity {700});
	writes.Mark(entt::entity {64});
	writes.Mark(entt::null);
	const auto marked = writes.Take(k_Reader);
	ASSERT_TRUE(marked.has_value());
	EXPECT_EQ(*marked, (std::vector<uint32_t> {5, 64, 700}));
	const auto again = writes.Take(k_Reader);
	ASSERT_TRUE(again.has_value());
	EXPECT_TRUE(again->empty());
}

TEST(ThingWrites, TheHighestNumberIsMarked)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	// The highest number of all is the number of the null entity
	const auto highest = static_cast<uint32_t>(ThingWrites::k_Numbers - 2);
	writes.Mark(entt::entt_traits<entt::entity>::construct(highest, 2));
	const auto marked = writes.Take(k_Reader);
	ASSERT_TRUE(marked.has_value());
	EXPECT_EQ(*marked, (std::vector<uint32_t> {highest}));
}

TEST(ThingWrites, AMarkOfAllGivesNoMarksOneTime)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	writes.Mark(entt::entity {9});
	writes.MarkAll();
	EXPECT_FALSE(writes.Take(k_Reader).has_value());
	const auto after = writes.Take(k_Reader);
	ASSERT_TRUE(after.has_value());
	EXPECT_TRUE(after->empty());
}

TEST(ThingWrites, EachReaderGetsEachMarkOneTime)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	writes.Mark(entt::entity {12});
	// The marks of a reader start at its first take
	EXPECT_FALSE(writes.Take(k_OtherReader).has_value());
	EXPECT_EQ(*writes.Take(k_Reader), (std::vector<uint32_t> {12}));
	writes.Mark(entt::entity {40});
	writes.Mark(entt::entity {7});
	EXPECT_EQ(*writes.Take(k_Reader), (std::vector<uint32_t> {7, 40}));
	writes.Mark(entt::entity {40});
	writes.Mark(entt::entity {3});
	EXPECT_EQ(*writes.Take(k_OtherReader), (std::vector<uint32_t> {3, 7, 40}));
	EXPECT_EQ(*writes.Take(k_Reader), (std::vector<uint32_t> {3, 40}));
	EXPECT_EQ(*writes.Take(k_OtherReader), std::vector<uint32_t> {});
	EXPECT_EQ(*writes.Take(k_Reader), std::vector<uint32_t> {});
}

TEST(ThingWrites, AMarkOfAllReachesEachReader)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	std::ignore = writes.Take(k_OtherReader);
	writes.MarkAll();
	EXPECT_FALSE(writes.Take(k_Reader).has_value());
	EXPECT_TRUE(writes.Take(k_Reader).has_value());
	EXPECT_FALSE(writes.Take(k_OtherReader).has_value());
	EXPECT_TRUE(writes.Take(k_OtherReader).has_value());
}

TEST(ThingWrites, AReaderThatDoesNotTakeForALongTimeGetsNoMarks)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	std::ignore = writes.Take(k_OtherReader);
	for (uint32_t i = 0; i < 70000; ++i)
	{
		writes.Mark(entt::entity {i});
	}
	EXPECT_EQ(writes.Take(k_Reader)->size(), 70000U);
	writes.Mark(entt::entity {5});
	EXPECT_EQ(*writes.Take(k_Reader), (std::vector<uint32_t> {5}));
	EXPECT_FALSE(writes.Take(k_OtherReader).has_value());
	EXPECT_EQ(*writes.Take(k_OtherReader), std::vector<uint32_t> {});
}

TEST(ThingWrites, ManyThreadsMarkAtTheSameTime)
{
	ThingWrites writes;
	std::ignore = writes.Take(k_Reader);
	constexpr uint32_t k_Threads = 4;
	constexpr uint32_t k_EachThread = 5000;
	std::vector<std::thread> threads;
	for (uint32_t t = 0; t < k_Threads; ++t)
	{
		threads.emplace_back([&writes, t] {
			for (uint32_t i = 0; i < k_EachThread; ++i)
			{
				writes.Mark(entt::entity {i * k_Threads + t});
			}
		});
	}
	for (auto& thread : threads)
	{
		thread.join();
	}
	const auto marked = writes.Take(k_Reader);
	ASSERT_TRUE(marked.has_value());
	ASSERT_EQ(marked->size(), k_Threads * k_EachThread);
	for (uint32_t i = 0; i < marked->size(); ++i)
	{
		EXPECT_EQ((*marked)[i], i);
	}
}

TEST(ThingWrites, ARequestThatCanWriteThePlaceMarksTheThing)
{
	Fixture f;
	std::ignore = f.registry.Get<const Transform>(f.thing);
	std::ignore = std::as_const(f.registry).Get<Transform>(f.thing);
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), std::vector<uint32_t> {});
	f.registry.Get<Transform>(f.thing).position.x = 1.0f;
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.thing}));
	f.registry.TryGet<Mesh>(f.thing)->id = 4;
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.thing}));
}

TEST(ThingWrites, AWalkThatCanWriteThePlaceMarksEachThing)
{
	Fixture f;
	f.registry.Each<const Transform>([](const Transform& /*transform*/) {});
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), std::vector<uint32_t> {});
	f.registry.Each<Transform>([](Transform& transform) { transform.scale.x = 2.0f; });
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.thing, f.other}));
}

TEST(ThingWrites, AGainAReplaceALossAndAWriteOfTheCellsMarkTheThing)
{
	Fixture f;
	f.registry.Assign<MapCellResident>(f.thing);
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.thing}));
	f.registry.Write<MapCellResident>(f.thing, [](MapCellResident& resident) { resident.cells.push_back(3); });
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.thing}));
	f.registry.AssignOrReplace<Mesh>(f.other);
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.other}));
	f.registry.Assign<DeadTree>(f.other);
	f.registry.Remove<DeadTree>(f.other);
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.other}));
	f.registry.Destroy(f.thing);
	EXPECT_EQ(*f.registry.ThingChanges().Take(k_Reader), Numbers({f.thing}));
}

TEST(ThingWrites, AnEmptyRegistryAndADirectWriteMarkAllThings)
{
	Fixture f;
	f.registry.StoresWritten();
	EXPECT_FALSE(f.registry.ThingChanges().Take(k_Reader).has_value());
	f.registry.Reset();
	EXPECT_FALSE(f.registry.ThingChanges().Take(k_Reader).has_value());
	EXPECT_TRUE(f.registry.ThingChanges().Take(k_Reader).has_value());
}
