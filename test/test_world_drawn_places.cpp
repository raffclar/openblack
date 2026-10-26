/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The frames draw the things that the hand holds or pulls, and the things that a tornado carries, at places that only
// the draw reads. The world keeps each thing at its Transform.

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawnPlace.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

const Transform k_TurnPlace {.position = glm::vec3(10.0f, 2.0f, 30.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(2.0f)};

entt::entity MakeThing(ecs::Registry& registry)
{
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, k_TurnPlace);
	return thing;
}

} // namespace

TEST(WorldDrawnPlaces, AThingWithNoDrawnPlaceIsDrawnAtItsTransform)
{
	ecs::Registry registry;
	const auto thing = MakeThing(registry);
	registry.Assign<InHand>(thing);
	const auto drawn = ecs::drawn_place::DrawnTransformOf(registry, thing, k_TurnPlace);
	EXPECT_EQ(drawn.position, k_TurnPlace.position);
	EXPECT_EQ(drawn.scale, k_TurnPlace.scale);
}

TEST(WorldDrawnPlaces, AThingInTheHandIsDrawnAtTheDrawnHand)
{
	ecs::Registry registry;
	const auto thing = MakeThing(registry);
	const glm::mat3 turned = glm::mat3(glm::rotate(glm::mat4(1.0f), 1.0f, glm::vec3(0.0f, 1.0f, 0.0f)));
	registry.Assign<InHand>(thing,
	                        InHand {.drawn = InHand::Drawn {.position = glm::vec3(1.0f, 5.0f, 2.0f), .rotation = turned}});
	const auto drawn = ecs::drawn_place::DrawnTransformOf(registry, thing, k_TurnPlace);
	EXPECT_EQ(drawn.position, glm::vec3(1.0f, 5.0f, 2.0f));
	EXPECT_EQ(drawn.rotation, turned);
	EXPECT_EQ(drawn.scale, k_TurnPlace.scale);
	// The world keeps the thing where the turn put it
	EXPECT_EQ(registry.Get<const Transform>(thing).position, k_TurnPlace.position);
}

TEST(WorldDrawnPlaces, AThingThatTheHandPullsIsDrawnLeaningAndStretched)
{
	ecs::Registry registry;
	const auto thing = MakeThing(registry);
	registry.Assign<HandPulled>(thing, HandPulled {.position = glm::vec3(10.0f, 2.0f, 30.0f),
	                                               .rotation = glm::mat3(1.0f),
	                                               .scale = glm::vec3(2.0f, 3.0f, 2.0f),
	                                               .standsAt = k_TurnPlace.position,
	                                               .standsRotation = k_TurnPlace.rotation,
	                                               .standsScale = k_TurnPlace.scale});
	const auto drawn = ecs::drawn_place::DrawnTransformOf(registry, thing, k_TurnPlace);
	EXPECT_EQ(drawn.scale, glm::vec3(2.0f, 3.0f, 2.0f));
	EXPECT_EQ(registry.Get<const Transform>(thing).scale, k_TurnPlace.scale);
}

// A lean that the hand let go of stays drawn until the world moves, turns or sizes the thing again, as a tree that grows
TEST(WorldDrawnPlaces, AThingLeftLeaningIsDrawnAtItsOwnPlaceOnceTheWorldChangesIt)
{
	ecs::Registry registry;
	const auto thing = MakeThing(registry);
	registry.Assign<HandPulled>(thing, HandPulled {.position = glm::vec3(10.0f, 2.0f, 30.0f),
	                                               .rotation = glm::mat3(1.0f),
	                                               .scale = glm::vec3(2.0f, 3.0f, 2.0f),
	                                               .standsAt = k_TurnPlace.position,
	                                               .standsRotation = k_TurnPlace.rotation,
	                                               .standsScale = k_TurnPlace.scale});
	auto grown = k_TurnPlace;
	grown.scale *= 1.01f;
	const auto drawn = ecs::drawn_place::DrawnTransformOf(registry, thing, grown);
	EXPECT_EQ(drawn.position, grown.position);
	EXPECT_EQ(drawn.scale, grown.scale);
	auto moved = k_TurnPlace;
	moved.position.x += 1.0f;
	EXPECT_EQ(ecs::drawn_place::DrawnTransformOf(registry, thing, moved).position, moved.position);
}

TEST(WorldDrawnPlaces, AThingThatATornadoCarriesIsDrawnBetweenItsSteps)
{
	ecs::Registry registry;
	const auto thing = MakeThing(registry);
	registry.Assign<CarriedByTornado>(
	    thing, CarriedByTornado {
	               .carried = nullptr,
	               .drawn = CarriedByTornado::Drawn {.position = glm::vec3(4.0f, 20.0f, 6.0f), .rotation = glm::mat3(1.0f)}});
	const auto drawn = ecs::drawn_place::DrawnTransformOf(registry, thing, k_TurnPlace);
	EXPECT_EQ(drawn.position, glm::vec3(4.0f, 20.0f, 6.0f));
	EXPECT_EQ(drawn.scale, k_TurnPlace.scale);
}
