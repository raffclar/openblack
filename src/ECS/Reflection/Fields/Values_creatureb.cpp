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

#include "Creature/CreatureBornWith.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creatureb(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creatureb(entt::meta_ctx& context)
{
	Reflect<openblack::creature_born::BornWith>(context, ValueOnly {})
	    .Field<&openblack::creature_born::BornWith::facing>("facing")
	    .Field<&openblack::creature_born::BornWith::slowSpeed>("slowSpeed")
	    .Field<&openblack::creature_born::BornWith::walkSpeed>("walkSpeed")
	    .Field<&openblack::creature_born::BornWith::runSpeed>("runSpeed");
}
