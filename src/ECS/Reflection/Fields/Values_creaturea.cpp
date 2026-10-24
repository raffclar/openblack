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

#include "Creature/CreatureAudio.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturea(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturea(entt::meta_ctx& context)
{
	Reflect<openblack::creature_audio::Layers>(context, ValueOnly {})
	    .Field<&openblack::creature_audio::Layers::body>("body")
	    .Field<&openblack::creature_audio::Layers::gesture>("gesture")
	    .Field<&openblack::creature_audio::Layers::face>("face")
	    .Field<&openblack::creature_audio::Layers::slots>("slots")
	    .Field<&openblack::creature_audio::Layers::soundingSlot>("soundingSlot")
	    .Field<&openblack::creature_audio::Layers::faceLooped>("faceLooped");
	Reflect<openblack::creature_audio::Played>(context, ValueOnly {})
	    .Field<&openblack::creature_audio::Played::animation>("animation")
	    .Field<&openblack::creature_audio::Played::timeMs>("timeMs");
}
