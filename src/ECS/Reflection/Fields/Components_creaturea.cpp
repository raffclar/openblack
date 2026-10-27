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

#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureAudio.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturea(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creaturea(entt::meta_ctx& context)
{
	Reflect<components::CreatureArena>(context)
	    .Field<&components::CreatureArena::place>("place")
	    .Field<&components::CreatureArena::radius>("radius")
	    .Field<&components::CreatureArena::temporary>("temporary")
	    .Field<&components::CreatureArena::fightOn>("fightOn")
	    .Field<&components::CreatureArena::first>("first")
	    .Field<&components::CreatureArena::second>("second")
	    .RunField<&components::CreatureArena::ring>("ring");
	Reflect<components::CreatureAudio>(context)
	    .Field<&components::CreatureAudio::last>("last")
	    .Field<&components::CreatureAudio::recent>("recent")
	    .Field<&components::CreatureAudio::clockMs>("clockMs");
}
