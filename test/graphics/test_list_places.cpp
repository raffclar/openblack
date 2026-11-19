/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Graphics/ListPlaces.h"

using openblack::graphics::list_places::Places;

namespace
{

entt::entity Entity(uint32_t index, uint32_t version = 0)
{
	return entt::entt_traits<entt::entity>::construct(index, version);
}

/// The places as a hash map finds them: the first place of each thing
std::unordered_map<entt::entity, uint32_t> MapOf(const std::vector<entt::entity>& list)
{
	std::unordered_map<entt::entity, uint32_t> map;
	for (uint32_t place = 0; place < static_cast<uint32_t>(list.size()); ++place)
	{
		map.try_emplace(list[place], place);
	}
	return map;
}

std::optional<uint32_t> PlaceIn(const std::unordered_map<entt::entity, uint32_t>& map, entt::entity entity)
{
	const auto found = map.find(entity);
	return found != map.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace

TEST(ListPlaces, AThingHasItsPlaceInTheList)
{
	Places places;
	places.Make(std::vector {Entity(7), Entity(2), Entity(40)});
	EXPECT_EQ(places.Of(Entity(7)), 0u);
	EXPECT_EQ(places.Of(Entity(2)), 1u);
	EXPECT_EQ(places.Of(Entity(40)), 2u);
	EXPECT_EQ(places.Of(Entity(3)), std::nullopt);
	EXPECT_EQ(places.Of(Entity(1000)), std::nullopt);
	EXPECT_EQ(places.Of(entt::null), std::nullopt);
}

TEST(ListPlaces, AThingInTheListTwoTimesKeepsItsFirstPlace)
{
	Places places;
	places.Make(std::vector {Entity(5), Entity(6), Entity(5)});
	EXPECT_EQ(places.Of(Entity(5)), 0u);
	EXPECT_EQ(places.Of(Entity(6)), 1u);
}

TEST(ListPlaces, AnotherVersionOfAThingIsNotThatThing)
{
	Places places;
	places.Make(std::vector {Entity(5, 1)});
	EXPECT_EQ(places.Of(Entity(5, 1)), 0u);
	EXPECT_EQ(places.Of(Entity(5, 0)), std::nullopt);
	EXPECT_EQ(places.Of(Entity(5, 2)), std::nullopt);

	// Two versions of one number in one list each keep their own place
	places.Make(std::vector {Entity(5, 1), Entity(9), Entity(5, 2), Entity(5, 2), Entity(5, 1)});
	EXPECT_EQ(places.Of(Entity(5, 1)), 0u);
	EXPECT_EQ(places.Of(Entity(5, 2)), 2u);
	EXPECT_EQ(places.Of(Entity(9)), 1u);
}

TEST(ListPlaces, ANewListForgetsTheThingsOfTheLastList)
{
	Places places;
	places.Make(std::vector {Entity(1), Entity(2), Entity(3)});
	places.Make(std::vector {Entity(3), Entity(4)});
	EXPECT_EQ(places.Of(Entity(1)), std::nullopt);
	EXPECT_EQ(places.Of(Entity(2)), std::nullopt);
	EXPECT_EQ(places.Of(Entity(3)), 0u);
	EXPECT_EQ(places.Of(Entity(4)), 1u);
	places.Make({});
	EXPECT_EQ(places.Of(Entity(3)), std::nullopt);
	EXPECT_EQ(places.Of(Entity(4)), std::nullopt);
}

TEST(ListPlaces, TheNullEntityHasAPlaceOnlyWhenTheListHasIt)
{
	Places places;
	places.Make(std::vector {Entity(1), entt::entity {entt::null}, Entity(2)});
	EXPECT_EQ(places.Of(entt::null), 1u);
	EXPECT_EQ(places.Of(Entity(2)), 2u);
	places.Make(std::vector {Entity(1)});
	EXPECT_EQ(places.Of(entt::null), std::nullopt);
}

TEST(ListPlaces, ThePlacesAreThoseOfAHashMapOverManyLists)
{
	std::mt19937 random(11);
	std::uniform_int_distribution<uint32_t> index(0, 300);
	std::uniform_int_distribution<uint32_t> version(0, 2);
	std::uniform_int_distribution<size_t> size(0, 400);
	Places places;
	for (int round = 0; round < 50; ++round)
	{
		std::vector<entt::entity> list(size(random));
		for (auto& entity : list)
		{
			entity = Entity(index(random), version(random));
		}
		places.Make(list);
		const auto map = MapOf(list);
		for (uint32_t i = 0; i <= 310; ++i)
		{
			for (uint32_t v = 0; v <= 3; ++v)
			{
				ASSERT_EQ(places.Of(Entity(i, v)), PlaceIn(map, Entity(i, v))) << round << " " << i << " " << v;
			}
		}
	}
}
