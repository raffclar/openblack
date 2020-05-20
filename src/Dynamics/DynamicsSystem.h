/* openblack - A reimplementation of Lionhead's Black & White.
 *
 * openblack is the legal property of its developers, whose names
 * can be found in the AUTHORS.md file distributed with this source
 * distribution.
 *
 * openblack is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * openblack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openblack. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <tuple>

#include <glm/fwd.hpp>

class btCollisionDispatcher;
class btDefaultCollisionConfiguration;
class btDiscreteDynamicsWorld;
struct btDbvtBroadphase;
class btRigidBody;
class btSequentialImpulseConstraintSolver;

namespace openblack
{
struct Transform;
}

namespace openblack::dynamics
{
enum class RigidBodyType
{
	Terrain,
	Entity,
};

struct RigidBodyDetails
{
	RigidBodyType type;
	int id;
	const void* userData;
};

class DynamicsSystem
{
public:
	DynamicsSystem();
	virtual ~DynamicsSystem();

	void Reset();
	void Update(std::chrono::microseconds& dt);
	void AddRigidBody(btRigidBody* object);
	const std::optional<std::pair<Transform, RigidBodyDetails>>
	RayCastClosestHit(const glm::vec3& origin, const glm::vec3& direction, float t_max) const;

private:
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
};
} // namespace openblack::dynamics
