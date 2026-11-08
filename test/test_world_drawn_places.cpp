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

#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/DrawnHeight.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/DrawnPlace.h"
#include "ECS/Registry.h"
#include "ECS/SoundPoint.h"

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

TEST(WorldDrawnPlaces, ATownTotemIsDrawnAtItsDrawnHeightAndSoundsFromThere)
{
	ecs::Registry registry;
	const auto totem = MakeThing(registry);
	registry.Assign<DrawnHeight>(totem, 7.5f);
	// Only the height comes from the frames: the turns keep its place on the land
	const auto drawn = ecs::drawn_place::DrawnTransformOf(registry, totem, k_TurnPlace);
	EXPECT_EQ(drawn.position, glm::vec3(10.0f, 7.5f, 30.0f));
	EXPECT_EQ(drawn.scale, k_TurnPlace.scale);
	EXPECT_EQ(ecs::SoundPointOf(registry, totem), std::optional(glm::vec3(10.0f, 7.5f, 30.0f)));
	EXPECT_EQ(registry.Get<Transform>(totem).position, k_TurnPlace.position);
}

// The frame draws a mark over a body, for example the glow of a disciple, at the point where it draws the body's model.
// A walking villager glides between its last two turns, thus the mark is not at the villager's Transform.

TEST(WorldDrawnPlaces, TheModelOfAThingWithNoDrawnPoseIsAtItsDrawnPlace)
{
	const glm::vec3 place {1.0f, 2.0f, 3.0f};
	EXPECT_EQ(ecs::drawn_place::ModelOrigin(place, nullptr, nullptr, nullptr), place);
}

TEST(WorldDrawnPlaces, TheModelOfAGlidingVillagerIsWhereItsDrawingPutIt)
{
	VillagerPose pose;
	pose.drawnAt = glm::vec3(4.0f, 0.0f, 6.0f);
	EXPECT_EQ(ecs::drawn_place::ModelOrigin(k_TurnPlace.position, &pose, nullptr, nullptr), *pose.drawnAt);
	pose.drawnAt.reset();
	EXPECT_EQ(ecs::drawn_place::ModelOrigin(k_TurnPlace.position, &pose, nullptr, nullptr), k_TurnPlace.position);
}

TEST(WorldDrawnPlaces, TheModelOfAVillagerInTheOpeningHandIsInTheGrip)
{
	VillagerPose pose;
	pose.drawnAt = glm::vec3(4.0f, 0.0f, 6.0f);
	HighDetail detail;
	detail.heldAt = glm::vec3(7.0f, 8.0f, 9.0f);
	EXPECT_EQ(ecs::drawn_place::ModelOrigin(k_TurnPlace.position, &pose, &detail, nullptr), *detail.heldAt);
	detail.heldAt.reset();
	EXPECT_EQ(ecs::drawn_place::ModelOrigin(k_TurnPlace.position, &pose, &detail, nullptr), *pose.drawnAt);
}

TEST(WorldDrawnPlaces, TheModelOfABodyInThePhysicsIsAtItsPhysicsPose)
{
	VillagerPose pose;
	pose.drawnAt = glm::vec3(4.0f, 0.0f, 6.0f);
	HighDetail detail;
	detail.heldAt = glm::vec3(7.0f, 8.0f, 9.0f);
	const PhysicsDrawPose physics {.origin = glm::vec3(-1.0f, -2.0f, -3.0f)};
	EXPECT_EQ(ecs::drawn_place::ModelOrigin(k_TurnPlace.position, &pose, &detail, &physics), physics.origin);
}

TEST(WorldDrawnPlaces, AGlidingVillagerInTheRegistryHasItsModelWhereItsDrawingPutIt)
{
	ecs::Registry registry;
	const auto villager = MakeThing(registry);
	auto& pose = registry.Assign<VillagerPose>(villager);
	pose.drawnAt = glm::vec3(9.5f, 2.0f, 29.5f);
	EXPECT_EQ(ecs::drawn_place::ModelOriginOf(registry, villager, k_TurnPlace), *pose.drawnAt);
	EXPECT_EQ(ecs::drawn_place::DrawnPlaces(registry).ModelOriginOf(villager, k_TurnPlace), *pose.drawnAt);
	EXPECT_FALSE(ecs::drawn_place::SunkFromView(registry, villager));
	registry.Assign<PhysicsDrawPose>(villager, PhysicsDrawPose {.origin = glm::vec3(0.0f), .underSea = true});
	EXPECT_TRUE(ecs::drawn_place::SunkFromView(registry, villager));
}

TEST(WorldDrawnPlaces, TheModelMatrixIsThePhysicsPoseOrTheDrawnOriginTurnedAndSized)
{
	ecs::Registry registry;
	const auto thing = MakeThing(registry);
	auto& pose = registry.Assign<VillagerPose>(thing);
	pose.drawnAt = glm::vec3(9.5f, 2.0f, 29.5f);
	const auto matrix = ecs::drawn_place::DrawnPlaces(registry).ModelMatrixOf(thing, k_TurnPlace);
	EXPECT_EQ(glm::vec3(matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), *pose.drawnAt);
	EXPECT_EQ(glm::vec3(matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)), glm::vec3(2.0f, 0.0f, 0.0f));
	const PhysicsDrawPose physics {.axes = glm::mat3(3.0f), .origin = glm::vec3(-1.0f, -2.0f, -3.0f)};
	registry.Assign<PhysicsDrawPose>(thing, physics);
	const auto flying = ecs::drawn_place::DrawnPlaces(registry).ModelMatrixOf(thing, k_TurnPlace);
	EXPECT_EQ(glm::vec3(flying * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), physics.origin);
	EXPECT_EQ(glm::vec3(flying * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)), glm::vec3(0.0f, 3.0f, 0.0f));
}
