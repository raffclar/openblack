/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Registry.h"

#include "Locator.h"
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
}

void Registry::Release(entt::entity entity)
{
	ENTT_ASSERT(_registry.orphan(entity), "Non-orphan entity");
	_registry.storage<entt::entity>().erase(entity);
}

void Registry::Destroy(entt::entity entity)
{
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
	_registry.ctx().erase<RegistryContext>();
	_registry.ctx().emplace<RegistryContext>();
	_entitiesMade = 0;
	++_entitiesGone;
};

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
