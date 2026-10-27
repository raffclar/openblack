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

#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/CreatureTarget.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturet(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creaturet(entt::meta_ctx& context)
{
	Reflect<components::CreatureTallies>(context)
	    .Field<&components::CreatureTallies::meals>("meals")
	    .Field<&components::CreatureTallies::poos>("poos")
	    .Field<&components::CreatureTallies::mushroomsEaten>("mushroomsEaten");
	Reflect<components::CreatureTarget>(context)
	    .Field<&components::CreatureTarget::point>("point")
	    .Field<&components::CreatureTarget::object>("object")
	    .Field<&components::CreatureTarget::has>("has");
}
