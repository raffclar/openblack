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

#include "Creature/CreatureGesture.h"
#include "Creature/CreatureGiving.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creatureg(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creatureg(entt::meta_ctx& context)
{
	Reflect<openblack::creature_gesture::Drawing>(context, ValueOnly {})
	    .Field<&openblack::creature_gesture::Drawing::drawn>("drawn")
	    .Field<&openblack::creature_gesture::Drawing::turns>("turns");
	Reflect<openblack::creature_giving::Offer>(context, ValueOnly {}).Field<&openblack::creature_giving::Offer::ms>("ms");
}
