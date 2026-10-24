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

#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creatureh(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creatureh(entt::meta_ctx& context)
{
	Reflect<components::CreatureHair>(context)
	    .Field<&components::CreatureHair::groups>("groups")
	    .Field<&components::CreatureHair::started>("started");
	Reflect<components::CreatureHandGlows>(context)
	    .Field<&components::CreatureHandGlows::glows>("glows")
	    .Field<&components::CreatureHandGlows::slidePace>("slidePace")
	    .Field<&components::CreatureHandGlows::turnPace>("turnPace")
	    .Field<&components::CreatureHandGlows::placed>("placed");
	Reflect<components::CreatureHelpMemory>(context).Field<&components::CreatureHelpMemory::memory>("memory");
}
