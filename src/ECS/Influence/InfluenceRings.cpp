/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The influence rings: a list, newest first.

#include <algorithm>

#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Influence.h"
#include "InfluenceState.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace openblack::influence
{
entt::entity CreateRing(const glm::vec3& position, PlayerNames player, float radius, bool anti)
{
	// pushed at the head of the list
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<InfluenceRing>(entity, position, player, radius, anti);
	auto& rings = detail::Globals().rings;
	rings.insert(rings.begin(), entity);
	return entity;
}

entt::entity CreateRingOnObject(entt::entity object, PlayerNames player, float radius, bool anti)
{
	// the ring takes the object's position and keeps a weak link to it
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return entt::null;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return entt::null;
	}
	const auto ring = CreateRing(transform->position, player, radius, anti);
	registry.Get<InfluenceRing>(ring).attached = object;
	return ring;
}

void DeleteRing(entt::entity ring)
{
	// unlinked from the list, then deleted
	auto& registry = Locator::entitiesRegistry::value();
	auto& rings = detail::Globals().rings;
	rings.erase(std::remove(rings.begin(), rings.end(), ring), rings.end());
	if (registry.Valid(ring) && registry.AllOf<InfluenceRing>(ring))
	{
		registry.Destroy(ring);
	}
}

void ProcessRings()
{
	// each ring: a ring with an object copies its position while it exists, and is deleted with it
	auto& registry = Locator::entitiesRegistry::value();
	const auto rings = detail::GlobalsOrDefault().rings; // (a copy: rings may go)
	for (const auto entity : rings)
	{
		auto* ring = registry.TryGet<InfluenceRing>(entity);
		if (ring == nullptr || ring->attached == entt::null)
		{
			continue;
		}
		const auto* transform = ecs::IsAvailable(ring->attached) ? registry.TryGet<const Transform>(ring->attached) : nullptr;
		if (transform == nullptr)
		{
			DeleteRing(entity);
			continue;
		}
		ring->position = transform->position;
	}
}
} // namespace openblack::influence
