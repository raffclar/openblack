/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/ThingMapPlace.h"

namespace
{
using namespace openblack;
using namespace openblack::ecs::components;
using map_coords::MapCoords;

/// A map position far from the corner of the map, which metres cannot keep: through metres and back it is one unit more
constexpr int32_t k_FarX = 0x01000001;
constexpr int32_t k_FarZ = 0x00FC131F;

void Stand(ecs::Registry& registry, entt::entity thing, glm::vec3 position)
{
	registry.Assign<Transform>(thing, Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
}
} // namespace

TEST(ThingMapPlace, MetresCannotKeepAPlaceFarFromTheCorner)
{
	EXPECT_NE(map_coords::ToFixed(map_coords::ToMetres(k_FarX)), k_FarX);
}

TEST(ThingMapPlace, AThingThatStandsIsWhereItsMetresAre)
{
	ecs::Registry registry;
	const auto rock = registry.Create();
	Stand(registry, rock, {2520.0f, 7.0f, 2490.0f});
	EXPECT_EQ(ecs::thing_map_place::Of(registry, rock),
	          (MapCoords {.x = map_coords::ToFixed(2520.0f), .z = map_coords::ToFixed(2490.0f)}));
}

TEST(ThingMapPlace, AThingWithNoPlaceIsAtTheCorner)
{
	ecs::Registry registry;
	EXPECT_EQ(ecs::thing_map_place::Of(registry, registry.Create()), MapCoords {});
}

TEST(ThingMapPlace, AWalkerIsWhereItsWalkHoldsItUnlessItWasMovedSince)
{
	ecs::Registry registry;
	const auto villager = registry.Create();
	const glm::vec3 metres {map_coords::ToMetres(k_FarX), 0.0f, map_coords::ToMetres(k_FarZ)};
	Stand(registry, villager, metres);
	auto& walk = registry.Assign<WallHug>(villager);
	walk.position = {k_FarX, k_FarZ};
	walk.placedAt = {metres.x, metres.z};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, villager), (MapCoords {.x = k_FarX, .z = k_FarZ}));
	// Something else moved it: it is where its metres are
	registry.Get<Transform>(villager).position.x += 1.0f;
	EXPECT_EQ(ecs::thing_map_place::Of(registry, villager).x, map_coords::ToFixed(metres.x + 1.0f));
}

TEST(ThingMapPlace, AnAnimalIsWhereItsTurnPutItNotWhereItIsDrawn)
{
	ecs::Registry registry;
	const auto sheep = registry.Create();
	Stand(registry, sheep, {100.0f, 0.0f, 200.0f});
	registry.Assign<Animal>(sheep).move.position = {k_FarX, k_FarZ};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, sheep), (MapCoords {.x = k_FarX, .z = k_FarZ}));
}

TEST(ThingMapPlace, AFlockAndAFireflyKeepTheirOwnPlaces)
{
	ecs::Registry registry;
	const auto flock = registry.Create();
	registry.Assign<Flock>(flock).place = {.x = k_FarX, .z = k_FarZ, .altitude = 3.0f};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, flock), (MapCoords {.x = k_FarX, .z = k_FarZ}));
	const auto firefly = registry.Create();
	registry.Assign<Firefly>(firefly).at = {.x = k_FarZ, .z = k_FarX, .altitude = 1.0f};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, firefly), (MapCoords {.x = k_FarZ, .z = k_FarX}));
}
