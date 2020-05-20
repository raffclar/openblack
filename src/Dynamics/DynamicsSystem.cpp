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

#include "DynamicsSystem.h"

#include <vector>

#include <BulletCollision/BroadphaseCollision/btDbvtBroadphase.h>
#include <BulletCollision/CollisionDispatch/btCollisionDispatcher.h>
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletCollision/CollisionDispatch/btDefaultCollisionConfiguration.h>
#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolver.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <Entities/Components/Transform.h>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

using namespace openblack;
using namespace openblack::dynamics;

DynamicsSystem::DynamicsSystem()
    : _configuration(std::make_unique<btDefaultCollisionConfiguration>())
    , _dispatcher(std::make_unique<btCollisionDispatcher>(_configuration.get()))
    , _broadphase(std::make_unique<btDbvtBroadphase>())
    , _solver(std::make_unique<btSequentialImpulseConstraintSolver>())
    , _world(
          std::make_unique<btDiscreteDynamicsWorld>(_dispatcher.get(), _broadphase.get(), _solver.get(), _configuration.get()))
{
	_world->setGravity(btVector3(0, -10, 0));
}

void DynamicsSystem::Reset()
{
	std::vector<btRigidBody*> to_remove;
	for (int i = 0; i < _world->getNumCollisionObjects(); ++i)
	{
		auto obj = _world->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body)
		{
			to_remove.push_back(body);
		}
	}
	for (auto& obj : to_remove)
	{
		_world->removeRigidBody(obj);
	}
}

DynamicsSystem::~DynamicsSystem() = default;

void DynamicsSystem::Update(std::chrono::microseconds& dt)
{
	std::chrono::duration<float> seconds = dt;
	_world->stepSimulation(seconds.count());
}

void DynamicsSystem::AddRigidBody(btRigidBody* object)
{
	_world->addRigidBody(object);
}

const std::optional<std::pair<Transform, RigidBodyDetails>>
DynamicsSystem::RayCastClosestHit(const glm::vec3& origin, const glm::vec3& direction, float t_max) const
{
	auto from = btVector3(origin.x, origin.y, origin.z);
	auto to = from + t_max * btVector3(direction.x, direction.y, direction.z);

	btCollisionWorld::ClosestRayResultCallback callback(from, to);

	_world->rayTest(from, to, callback);

	if (!callback.hasHit())
		return std::nullopt;

	auto translation = glm::vec3(callback.m_hitPointWorld.x(), callback.m_hitPointWorld.y(), callback.m_hitPointWorld.z());
	auto normal = glm::vec3(callback.m_hitNormalWorld.x(), callback.m_hitNormalWorld.y(), callback.m_hitNormalWorld.z());
	const auto up = glm::vec3(0, 1, 0);
	auto rotation = glm::orientation(normal, up);

	return std::make_optional(std::make_pair(
	    Transform {translation, rotation, glm::vec3(1.0f)},
	    RigidBodyDetails {static_cast<RigidBodyType>(callback.m_collisionObject->getUserIndex()),
	                      callback.m_collisionObject->getUserIndex2(), callback.m_collisionObject->getUserPointer()}));
}
