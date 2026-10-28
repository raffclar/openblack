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

#include "ECS/GameEndRules.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Graphics/Moon.h"
#include "Graphics/RenderModes.h"
#include "Graphics/Sun.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_g(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_g(entt::meta_ctx& context)
{
	Reflect<openblack::game_end::State>(context, ValueOnly {})
	    .Field<&openblack::game_end::State::decided>("decided")
	    .Field<&openblack::game_end::State::lastPlace>("lastPlace")
	    .Field<&openblack::game_end::State::conditionCount>("conditionCount");
	Reflect<openblack::graphics::moon::Placement>(context, ValueOnly {})
	    .Field<&openblack::graphics::moon::Placement::offset>("offset")
	    .Field<&openblack::graphics::moon::Placement::alpha>("alpha");
	Reflect<openblack::graphics::render_modes::Properties>(context, ValueOnly {})
	    .Field<&openblack::graphics::render_modes::Properties::additive>("additive")
	    .Field<&openblack::graphics::render_modes::Properties::writesDepth>("writesDepth")
	    .Field<&openblack::graphics::render_modes::Properties::bothSides>("bothSides")
	    .Field<&openblack::graphics::render_modes::Properties::textureAlpha>("textureAlpha");
	Reflect<openblack::graphics::sun::Placement>(context, ValueOnly {})
	    .Field<&openblack::graphics::sun::Placement::position>("position")
	    .Field<&openblack::graphics::sun::Placement::alpha>("alpha");
}
