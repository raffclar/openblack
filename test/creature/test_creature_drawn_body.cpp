/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's drawn matrix (ecs::DrawnModel) on a registry of its own: between its turns the body and its eyes take
// the same pose, in the hand they take the hand's, standing it is the Transform's matrix bit for bit, and in its pen the
// eyes shrink with the body

#define LOCATOR_IMPLEMENTATIONS

#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/sparse_set.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureAnimationSystem;

namespace
{
const glm::vec3 k_Scale {2.0f};

glm::mat3 Heading(float radians)
{
	return glm::mat3(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 1.0f, 0.0f)));
}

/// A creature whose last turn put its Transform at `at` turned by `rotation`, its body drawn at `drawnAt` turned by
/// `drawnRotation`
entt::entity MakeCreature(ecs::Registry& registry, glm::vec3 at, const glm::mat3& rotation, glm::vec3 drawnAt,
                          const glm::mat3& drawnRotation)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, at, rotation, k_Scale);
	auto& locomotion = registry.Assign<CreatureLocomotion>(entity);
	locomotion.started = true;
	locomotion.fromPosition = drawnAt;
	locomotion.toPosition = at;
	registry.Assign<CreatureDrawPose>(entity, CreatureDrawPose {.position = drawnAt, .rotation = drawnRotation});
	return entity;
}

/// One eye on a triangle of the first bone, sunk `depth` into the body, the other eye and both lids' points left out
creature::CreatureRig::Eyes OneEye(float depth = 0.0f)
{
	creature::CreatureRig::EyePoint point {
	    .enabled = true, .depth = depth, .vertices = {}, .bones = {0, 0, 0}, .u = 0.25f, .v = 0.25f, .skinColour = {}};
	for (auto& mesh : point.vertices)
	{
		mesh = {glm::vec3(0.0f, 5.0f, 1.0f), glm::vec3(1.0f, 5.0f, 1.0f), glm::vec3(0.0f, 6.0f, 1.0f)};
	}
	auto none = point;
	none.enabled = false;
	return {.scale = 1.0f, .points = {point, none, none, none}, .lidAngles = {.open = 0.0f, .closed = 0.0f, .calm = 0.0f}};
}

/// The first eyeball's matrix of a creature's eyes placed for one frame
glm::mat4 EyeballOf(const ecs::Registry& registry, entt::entity creature, float depth = 0.0f)
{
	CreatureEyes eyes;
	const std::vector<glm::mat4> bones(1, glm::mat4(1.0f));
	CreatureAnimationSystem::PlaceEyes(registry, creature, eyes, OneEye(depth), creature_morph::Morph {}, bones, 1.0f, 0.016f);
	EXPECT_TRUE(eyes.drawn.front().eyeball.has_value());
	return eyes.drawn.front().eyeball.value_or(glm::mat4(0.0f));
}

std::vector<std::pair<entt::id_type, size_t>> StorageSizesOf(const ecs::Registry& registry)
{
	std::vector<std::pair<entt::id_type, size_t>> sizes;
	registry.EachStorage(
	    [&sizes](entt::id_type id, const entt::sparse_set& storage) { sizes.emplace_back(id, storage.size()); });
	return sizes;
}
} // namespace

TEST(CreatureDrawnBody, BetweenTurnsTheBodyAndItsEyesTakeTheSamePose)
{
	ecs::Registry registry;
	const glm::vec3 at {100.0f, 2.0f, 50.0f};
	const glm::vec3 drawnAt {95.0f, 2.0f, 51.0f};
	const auto walking = MakeCreature(registry, at, Heading(0.0f), drawnAt, Heading(0.25f));
	// standing where and as the walking one is drawn, and standing where the walking one's turn put it
	const auto there = MakeCreature(registry, drawnAt, Heading(0.25f), drawnAt, Heading(0.25f));
	const auto turnEnd = MakeCreature(registry, at, Heading(0.0f), at, Heading(0.0f));
	const ecs::Registry& lookup = registry;

	const auto model = ecs::DrawnModel(lookup, walking);
	EXPECT_EQ(model, affine::Model(drawnAt, Heading(0.25f), k_Scale));
	EXPECT_EQ(model, ecs::DrawnModel(lookup, there));
	EXPECT_EQ(ecs::DrawnBodyModel(lookup, walking), model);
	EXPECT_EQ(ecs::DrawnPosition(lookup, walking), drawnAt);
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(lookup, walking).rotation, Heading(0.25f));

	// the eyes on that same pose: as on the one standing there, not where the turn put the body
	const auto eyeball = EyeballOf(lookup, walking);
	EXPECT_EQ(eyeball, EyeballOf(lookup, there));
	EXPECT_NE(eyeball, EyeballOf(lookup, turnEnd));
	// and its Transform stays where the turn put it
	EXPECT_EQ(lookup.Get<Transform>(walking).position, at);
}

TEST(CreatureDrawnBody, HeldItIsDrawnAtTheHandsPose)
{
	ecs::Registry registry;
	const auto creature =
	    MakeCreature(registry, glm::vec3(100.0f, 2.0f, 50.0f), Heading(0.0f), glm::vec3(95.0f, 2.0f, 51.0f), Heading(0.25f));
	const glm::vec3 inHand {10.0f, 40.0f, 12.0f};
	registry.Assign<HandDrawPose>(creature, HandDrawPose {.position = inHand, .rotation = Heading(-0.7f), .upStretch = 1.0f});
	const auto there = MakeCreature(registry, inHand, Heading(-0.7f), inHand, Heading(-0.7f));
	const ecs::Registry& lookup = registry;

	EXPECT_EQ(ecs::DrawnModel(lookup, creature), affine::Model(inHand, Heading(-0.7f), k_Scale));
	const auto placement = ecs::creature_pose::DrawnPlacementOf(lookup, creature);
	EXPECT_EQ(placement.position, inHand);
	EXPECT_EQ(placement.rotation, Heading(-0.7f));
	EXPECT_EQ(EyeballOf(lookup, creature), EyeballOf(lookup, there));
}

TEST(CreatureDrawnBody, StandingItIsTheTransformsMatrixBitForBit)
{
	ecs::Registry registry;
	const glm::vec3 at {100.0f, 2.0f, 50.0f};
	const auto turn = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.4f, glm::vec3(0.3f, 1.0f, 0.2f)));
	const auto standing = MakeCreature(registry, at, turn, at, turn);
	// before its first turn, its pose is not read
	const auto unmoved = MakeCreature(registry, at, turn, glm::vec3(0.0f), glm::mat3(1.0f));
	registry.Get<CreatureLocomotion>(unmoved).started = false;
	const ecs::Registry& lookup = registry;

	EXPECT_EQ(ecs::DrawnModel(lookup, standing), affine::Model(lookup.Get<Transform>(standing)));
	EXPECT_EQ(ecs::DrawnBodyModel(lookup, standing), affine::Model(lookup.Get<Transform>(standing)));
	EXPECT_EQ(ecs::DrawnModel(lookup, unmoved), affine::Model(lookup.Get<Transform>(unmoved)));
	// in its pen it keeps its drawn scale
	registry.Get<CreatureDrawPose>(standing).scale = glm::vec3(0.5f);
	EXPECT_EQ(ecs::DrawnModel(lookup, standing), affine::Model(at, turn, glm::vec3(0.5f)));
}

TEST(CreatureDrawnBody, InItsPenTheEyesShrinkWithTheBody)
{
	ecs::Registry registry;
	const glm::vec3 at {100.0f, 2.0f, 50.0f};
	const auto full = MakeCreature(registry, at, Heading(0.25f), at, Heading(0.25f));
	const auto shrunk = MakeCreature(registry, at, Heading(0.25f), at, Heading(0.25f));
	// drawn at 0.22 of its own size, as in its temple's pen
	constexpr float k_Share = 0.22f;
	registry.Get<CreatureDrawPose>(shrunk).scale = k_Scale * k_Share;
	const ecs::Registry& lookup = registry;

	EXPECT_EQ(ecs::DrawnModel(lookup, shrunk), affine::Model(at, Heading(0.25f), k_Scale * k_Share));
	EXPECT_FLOAT_EQ(ecs::creature_pose::DrawnSizeShare(lookup, shrunk), k_Share);
	EXPECT_EQ(ecs::creature_pose::DrawnSizeShare(lookup, full), 1.0f);
	// the eye's offset from the body's origin (how deep it sits too) and its size shrink by the body's share
	constexpr float k_Depth = 0.5f;
	const auto big = EyeballOf(lookup, full, k_Depth);
	const auto small = EyeballOf(lookup, shrunk, k_Depth);
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		EXPECT_NEAR(small[3][axis] - at[axis], (big[3][axis] - at[axis]) * k_Share, 1e-4f);
	}
	EXPECT_NEAR(glm::length(glm::vec3(small[0])), glm::length(glm::vec3(big[0])) * k_Share, 1e-5f);
	// its own size stays
	EXPECT_EQ(lookup.Get<Transform>(shrunk).scale, k_Scale);
}

TEST(CreatureDrawnBody, TheDrawnMatrixMakesNoStorage)
{
	ecs::Registry registry;
	const auto creature =
	    MakeCreature(registry, glm::vec3(100.0f, 2.0f, 50.0f), Heading(0.0f), glm::vec3(95.0f, 2.0f, 51.0f), Heading(0.25f));
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const ecs::Registry& lookup = registry;
	const auto before = StorageSizesOf(lookup);
	static_cast<void>(ecs::DrawnBodyModel(lookup, creature));
	static_cast<void>(ecs::DrawnBodyModel(lookup, thing));
	static_cast<void>(EyeballOf(lookup, creature));
	EXPECT_EQ(StorageSizesOf(lookup), before);
}
