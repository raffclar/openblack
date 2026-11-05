/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

namespace openblack::ecs::components
{
struct Animal;
struct CarriedByTornado;
struct CreatureAnimation;
struct DrawnHeight;
struct Firefly;
struct HandPulled;
struct HeldByCreature;
struct HighDetail;
struct InHand;
struct PhysicsDrawPose;
struct Reward;
struct Shark;
struct TownDesireFlag;
struct VillagerPose;
} // namespace openblack::ecs::components

/// Where the frame draws a thing that moves between the game turns. The turns go by the thing's Transform; the frames
/// keep the drawn place in fields that only the draw reads.
namespace openblack::ecs::drawn_place
{

/// Whether a thing still stands in the world as it stood when the frame drew its pulled pose. When the world moved,
/// turned or sized the thing after that, the frames no longer draw the pulled pose.
[[nodiscard]] bool StandsAsPulled(const components::HandPulled& pulled, const components::Transform& transform);

/// The Transform that the frame draws a thing with: a walking creature, an animal or a shark between its last two turns,
/// a thing in a creature's hand or in a god's hand where the drawn hand is, a reward chest as it falls, a firefly where it
/// drifts, a town's totem and its icon at their drawn height, else the thing's own Transform
[[nodiscard]] components::Transform DrawnTransformOf(const Registry& registry, entt::entity entity,
                                                     const components::Transform& transform);

/// The drawn Transform of many things in one walk, the same as DrawnTransformOf. It finds the stores of the components
/// one time. Make it again when the walk can make the first component of one of these kinds.
class DrawnPlaces
{
public:
	explicit DrawnPlaces(const Registry& registry);
	[[nodiscard]] components::Transform Of(entt::entity entity, const components::Transform& transform) const;
	/// The point where the frame draws the origin of the thing's model, the same as ModelOrigin
	[[nodiscard]] glm::vec3 ModelOriginOf(entt::entity entity, const components::Transform& transform) const;
	/// The matrix that the frame draws the thing's model with: its physics pose, or its model origin turned as the frame
	/// draws it and sized by its Transform
	[[nodiscard]] glm::mat4 ModelMatrixOf(entt::entity entity, const components::Transform& transform) const;

private:
	ComponentLookup<components::CreatureAnimation> _animation;
	ComponentLookup<components::HeldByCreature> _holder;
	ComponentLookup<components::CarriedByTornado> _carried;
	ComponentLookup<components::HandPulled> _pulled;
	ComponentLookup<components::InHand> _inHand;
	ComponentLookup<components::Animal> _animal;
	ComponentLookup<components::Shark> _shark;
	ComponentLookup<components::Reward> _chest;
	ComponentLookup<components::TownDesireFlag> _flag;
	ComponentLookup<components::Firefly> _firefly;
	ComponentLookup<components::DrawnHeight> _height;
	ComponentLookup<components::VillagerPose> _pose;
	ComponentLookup<components::HighDetail> _highDetail;
	ComponentLookup<components::PhysicsDrawPose> _physics;
};

/// The point where the frame draws the origin of a thing's model, from its drawn place. The order is:
/// - A body that moves in the physics is at the place of its physics pose.
/// - A high-detail villager in the grip of the opening's hand is in that grip.
/// - A villager that glides between its last two turns is where its drawing put it.
/// - Another thing is at its drawn place.
/// Things that the frame draws over a body, for example the glow of a disciple and the label of a villager, use this
/// point. Thus they stay on the body while it moves between the turns.
[[nodiscard]] glm::vec3 ModelOrigin(const glm::vec3& drawnPlace, const components::VillagerPose* pose,
                                    const components::HighDetail* highDetail, const components::PhysicsDrawPose* physics);

/// The point where the frame draws the origin of a thing's model, the same as ModelOrigin with the thing's drawn place
[[nodiscard]] glm::vec3 ModelOriginOf(const Registry& registry, entt::entity entity, const components::Transform& transform);

/// Whether the frame does not draw the model of a thing, because the thing sank under the sea in the physics
[[nodiscard]] bool SunkFromView(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs::drawn_place
