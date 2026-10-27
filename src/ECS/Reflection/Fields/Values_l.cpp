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

#include "3D/LandLightFrame.h"
#include "3D/Light.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_l(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_l(entt::meta_ctx& context)
{
	Reflect<openblack::LandLightInputs>(context, ValueOnly {})
	    .Field<&openblack::LandLightInputs::skyType>("skyType")
	    .Field<&openblack::LandLightInputs::alignment>("alignment")
	    .Field<&openblack::LandLightInputs::overcast>("overcast")
	    .Field<&openblack::LandLightInputs::flash>("flash");
	Reflect<openblack::LightCone>(context, ValueOnly {})
	    .Field<&openblack::LightCone::transform>("transform")
	    .Field<&openblack::LightCone::colour>("colour")
	    .Field<&openblack::LightCone::nearRadius>("nearRadius")
	    .Field<&openblack::LightCone::length>("length")
	    .Field<&openblack::LightCone::angle>("angle");
}
