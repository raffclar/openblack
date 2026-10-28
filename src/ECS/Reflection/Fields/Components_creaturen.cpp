/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Written by tools/inspector/generate_component_fields.py: run it again rather than editing this file.

#include <array>
#include <span>
#include <string_view>

#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturen(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creaturen(entt::meta_ctx& context)
{
	Reflect<components::CreatureNeeds>(context)
	    .Field<&components::CreatureNeeds::started>("started")
	    .Field<&components::CreatureNeeds::needs>("needs")
	    .Field<&components::CreatureNeeds::kept>("kept")
	    .Field<&components::CreatureNeeds::rest>("rest")
	    .Field<&components::CreatureNeeds::restTurns>("restTurns")
	    .Field<&components::CreatureNeeds::rested>("rested")
	    .Field<&components::CreatureNeeds::faint>("faint")
	    .Field<&components::CreatureNeeds::carriedWeight>("carriedWeight")
	    .Field<&components::CreatureNeeds::moving>("moving");
}
