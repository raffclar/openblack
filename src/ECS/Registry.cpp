/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Registry.h"

#include <utility>

#include "Components/Animal.h"
#include "Components/Flock.h"
#include "Components/ScriptControl.h"
#include "Locator.h"
#include "ScriptFlocks.h"
#include "Systems/RenderingSystemInterface.h"

namespace openblack::ecs
{

Registry::Registry()
{
	_registry.ctx().emplace<RegistryContext>();
	// Each entity made is counted as it is made, however it is made
	_registry.on_construct<entt::entity>().connect<&Registry::EntityMade>(*this);
	// The registry also counts each entity that goes. A reader can then know if entities went since it last looked.
	_registry.on_destroy<entt::entity>().connect<&Registry::EntityGone>(*this);
	// Every living that goes leaves its flock first, as each living does when the game deletes it. Thus a flock never
	// keeps a member that is gone, and its leader is always a living that is there. The stores of the flock components
	// exist from the start, so that the removal of a living never adds a store while the registry removes components. The
	// store of the script hold exists from the start for the same reason.
	_registry.storage<components::Flock>();
	_registry.storage<components::InScript>();
	_registry.on_destroy<components::Animal>().connect<&Registry::LivingGone>(*this);
	_registry.on_destroy<components::FlockMember>().connect<&Registry::LivingGone>(*this);
}

void Registry::LivingGone(entt::registry& /*registry*/, entt::entity living)
{
	script_flocks::RemoveAtDeath(*this, living);
}

void Registry::Release(entt::entity entity)
{
	ENTT_ASSERT(_registry.orphan(entity), "Non-orphan entity");
	_registry.storage<entt::entity>().erase(entity);
}

void Registry::Destroy(entt::entity entity)
{
	_destroying.publish(entity);
	SetLayoutDirty();
	_registry.destroy(entity);
}

RegistryContext& Registry::Context()
{
	return _registry.ctx().get<RegistryContext>();
}

const RegistryContext& Registry::Context() const
{
	return _registry.ctx().get<const RegistryContext>();
}

void Registry::Reset()
{
	SetLayoutDirty();
	_registry.clear();
	// The store of the ids forgets the ids of the entities that went, so that the next ids are those of a new registry.
	// The components' stores, their signals and the context stay.
	if (std::exchange(_freshIdsAtNextReset, false))
	{
		_registry.storage<entt::entity>().clear();
	}
	_registry.ctx().erase<RegistryContext>();
	_registry.ctx().emplace<RegistryContext>();
	_entitiesMade = 0;
	++_resets;
	++_entitiesGone;
}

void Registry::SetDirty()
{
	if (Locator::rendereringSystem::has_value())
	{
		Locator::rendereringSystem::value().SetDirty();
	}
}

void Registry::SetLayoutDirty()
{
	if (Locator::rendereringSystem::has_value())
	{
		Locator::rendereringSystem::value().SetLayoutDirty();
	}
}
} // namespace openblack::ecs
