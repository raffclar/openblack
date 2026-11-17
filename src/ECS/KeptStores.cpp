/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "KeptStores.h"

namespace openblack::ecs
{
namespace
{
template <typename... Components>
void Make(entt::registry& registry, const ComponentLookups<Components...>* /*kinds*/)
{
	(static_cast<void>(registry.storage<Components>()), ...);
}
} // namespace

void KeptStores::MakeStores(entt::registry& registry)
{
	Make(registry, static_cast<const Life*>(nullptr));
	Make(registry, static_cast<const Walk*>(nullptr));
}

} // namespace openblack::ecs
