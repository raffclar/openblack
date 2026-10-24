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

#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureEyes.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturee(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturee(entt::meta_ctx& context)
{
	Reflect<openblack::creature_emote::RunLookBack>(context, ValueOnly {})
	    .Field<&openblack::creature_emote::RunLookBack::lookingBack>("lookingBack")
	    .Field<&openblack::creature_emote::RunLookBack::turns>("turns");
	Reflect<openblack::creature_eyes::Blink>(context, ValueOnly {})
	    .Field<&openblack::creature_eyes::Blink::state>("state")
	    .Field<&openblack::creature_eyes::Blink::timerMs>("timerMs")
	    .Field<&openblack::creature_eyes::Blink::intervalMs>("intervalMs");
}
