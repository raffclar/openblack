/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::physics
{
struct State;
} // namespace openblack::ecs::physics

namespace openblack::ecs::systems
{
/// The physics objects' state: the constants, the bodies, the release prediction, the class handlers and hooks, the
/// dust puffs and the collision sound pairs. physics::PhysicsObjects, Dust, CollisionSounds and from_hand work on it
/// (Locator::physicsObjectsSystem)
class PhysicsObjectsSystemInterface
{
public:
	virtual ~PhysicsObjectsSystemInterface() = default;

	[[nodiscard]] virtual physics::State& GetState() noexcept = 0;
};
} // namespace openblack::ecs::systems
