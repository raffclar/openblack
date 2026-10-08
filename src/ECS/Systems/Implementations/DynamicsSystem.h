/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include <glm/vec3.hpp>

#include "ECS/Components/Transform.h"
#include "ECS/Systems/DynamicsSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

class btCollisionDispatcher;
class btDefaultCollisionConfiguration;
class btDiscreteDynamicsWorld;
struct btDbvtBroadphase;
class btSequentialImpulseConstraintSolver;

namespace openblack
{
class LandIslandInterface;
namespace ecs::components
{
struct Transform;
}
} // namespace openblack

namespace openblack::ecs::systems
{

class DynamicsSystem final: public DynamicsSystemInterface
{
public:
	DynamicsSystem();
	~DynamicsSystem();

	void Reset() override;
	void Update(std::chrono::microseconds& dt) override;
	void AddRigidBody(btRigidBody* object) override;
	void RemoveRigidBody(btRigidBody* object) override;
	void RegisterRigidBodies() override;
	void RegisterIslandRigidBodies(LandIslandInterface& island) override;
	void UpdatePhysicsTransforms() override;
	[[nodiscard]] std::optional<std::pair<ecs::components::Transform, RigidBodyDetails>>
	RayCastClosestHit(const glm::vec3& origin, const glm::vec3& direction, float tMax) const override;

private:
	/// A ray asked again with the same bits against the same world gives the same answer: the last few are kept (the hand's,
	/// the camera's two and the gestures' rays each frame), keyed by the ray's exact floats and the world's generation
	struct CachedRay
	{
		glm::vec3 origin;
		glm::vec3 direction;
		float tMax;
		uint64_t generation;
		std::optional<std::pair<ecs::components::Transform, RigidBodyDetails>> result;
	};
	static constexpr size_t k_CachedRays = 4;

	/// Every change of the collision world: the cached rays are no longer valid
	void WorldChanged() { ++_generation; }
	/// Bullet's closest hit along the ray, uncached
	[[nodiscard]] std::optional<std::pair<ecs::components::Transform, RigidBodyDetails>>
	CastRay(const glm::vec3& origin, const glm::vec3& direction, float tMax) const;

	/// collision configuration contains default setup for memory, collision setup
	std::unique_ptr<btDefaultCollisionConfiguration> _configuration;
	/// use the default collision dispatcher. For parallel processing you can use
	/// a different dispatcher (see Extras/BulletMultiThreaded)
	std::unique_ptr<btCollisionDispatcher> _dispatcher;
	std::unique_ptr<btDbvtBroadphase> _broadphase;
	/// the default constraint solver. For parallel processing you can use a
	/// different solver (see Extras/BulletMultiThreaded)
	std::unique_ptr<btSequentialImpulseConstraintSolver> _solver;
	std::unique_ptr<btDiscreteDynamicsWorld> _world;

	/// Bumped on every body added or removed, every step and every reset
	uint64_t _generation {0};
	mutable std::array<std::optional<CachedRay>, k_CachedRays> _cachedRays;
	/// The slot the next new ray replaces (oldest first)
	mutable size_t _nextCachedRay {0};
};
} // namespace openblack::ecs::systems
