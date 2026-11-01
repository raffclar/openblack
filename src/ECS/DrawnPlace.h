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
struct InHand;
struct Reward;
struct Shark;
struct TownDesireFlag;
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
};

} // namespace openblack::ecs::drawn_place
