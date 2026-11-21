/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Field.h"
#include "ECS/ListOrder.h"
#include "ECS/Registry.h"

/// The order of the fields. The game puts each new field at the front of the list of all fields and at the front of the
/// list of its town. A field never moves to another town. Thus the two lists keep the newest field first, also after a
/// field goes.
namespace openblack::ecs::field_order
{

/// Every field, the newest first
[[nodiscard]] inline std::vector<entt::entity> AllFields(const Registry& registry)
{
	return list_order::Walk<components::Field>(registry);
}

/// The fields of the town with this number, the newest first
[[nodiscard]] inline std::vector<entt::entity> FieldsOfTown(const Registry& registry, int town)
{
	auto fields = AllFields(registry);
	std::erase_if(fields,
	              [&registry, town](entt::entity field) { return registry.Get<const components::Field>(field).town != town; });
	return fields;
}

} // namespace openblack::ecs::field_order
