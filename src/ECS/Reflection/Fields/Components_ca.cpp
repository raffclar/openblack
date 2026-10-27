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

#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_ca(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_ca(entt::meta_ctx& context)
{
	Reflect<components::CameraBookmark>(context)
	    .Field<&components::CameraBookmark::number>("number")
	    .Field<&components::CameraBookmark::set>("set")
	    .Field<&components::CameraBookmark::point>("point")
	    .Field<&components::CameraBookmark::thing>("thing")
	    .Field<&components::CameraBookmark::cameraOffset>("cameraOffset");
	Reflect<components::CarriedByTornado>(context)
	    .Field<&components::CarriedByTornado::carried>("carried")
	    .Field<&components::CarriedByTornado::drawn>("drawn");
	Reflect<components::CaughtByTornado> {context};
}
