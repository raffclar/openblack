/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Registry.h"

#include "ECS/Components/Unavailable.h"
#include "Locator.h"
#include "Profiler.h"
#include "Systems/RenderingSystemInterface.h"

namespace openblack::ecs
{

Registry::Registry()
{
	_registry.ctx().emplace<RegistryContext>();
	// the views' exclude<Unavailable> would create it lazily (the draw's or the logic's first): it exists from the
	// start, so Debug/StateHash's "pools" do not depend on which views ran
	_registry.storage<components::Unavailable>();
}

void Registry::Release(entt::entity entity)
{
	ENTT_ASSERT(_registry.orphan(entity), "Non-orphan entity");
	_registry.storage<entt::entity>().erase(entity);
}

void Registry::Destroy(entt::entity entity)
{
	// an entity gone changes the order of the components' storages: the draw lists are made again
	Dirty("Destroy", {}, 0, true);
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
	Dirty("Reset", {}, 0, true);
	_registry.clear();
	_registry.ctx().erase<RegistryContext>();
	_registry.ctx().emplace<RegistryContext>();
	_registry.storage<components::Unavailable>(); // as in the constructor (clear() keeps it; explicit anyway)
};

void Registry::SetDirty(std::source_location where)
{
	Dirty(where.file_name(), {}, where.line());
}

void Registry::Dirty(std::string_view where, std::string_view what, uint32_t line, bool layout)
{
	// (openblack engine) the profile counts them by caller
	if (Locator::profiler::has_value() && Locator::profiler::value().Counting())
	{
		Locator::profiler::value().CountDirty(where, what, line);
	}
	if (Locator::rendereringSystem::has_value())
	{
		if (layout)
		{
			Locator::rendereringSystem::value().SetLayoutDirty();
		}
		else
		{
			Locator::rendereringSystem::value().SetDirty();
		}
	}
}
} // namespace openblack::ecs
