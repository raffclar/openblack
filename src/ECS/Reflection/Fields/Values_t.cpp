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

#include "Creature/TempleLeashes.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_t(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_t(entt::meta_ctx& context)
{
	Reflect<openblack::temple_leashes::Look>(context, ValueOnly {})
	    .Field<&openblack::temple_leashes::Look::scroll>("scroll")
	    .Field<&openblack::temple_leashes::Look::pitch>("pitch")
	    .Field<&openblack::temple_leashes::Look::roll>("roll")
	    .Field<&openblack::temple_leashes::Look::glow>("glow");
}
