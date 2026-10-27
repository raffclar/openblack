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

#include "Creature/CreatureRoute.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturer(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturer(entt::meta_ctx& context)
{
	Reflect<openblack::creature_route::Circle>(context, ValueOnly {})
	    .Field<&openblack::creature_route::Circle::centre>("centre")
	    .Field<&openblack::creature_route::Circle::radius>("radius");
	Reflect<openblack::creature_route::Leg>(context, ValueOnly {})
	    .Field<&openblack::creature_route::Leg::from>("from")
	    .Field<&openblack::creature_route::Leg::to>("to")
	    .Field<&openblack::creature_route::Leg::arc>("arc")
	    .Field<&openblack::creature_route::Leg::heading>("heading")
	    .Field<&openblack::creature_route::Leg::way>("way")
	    .Field<&openblack::creature_route::Leg::onStartCircle>("onStartCircle")
	    .Field<&openblack::creature_route::Leg::cost>("cost")
	    .Field<&openblack::creature_route::Leg::round>("round")
	    .Field<&openblack::creature_route::Leg::roundWay>("roundWay");
	Reflect<openblack::creature_route::Route>(context, ValueOnly {})
	    .Field<&openblack::creature_route::Route::legs>("legs")
	    .Field<&openblack::creature_route::Route::segment>("segment")
	    .Field<&openblack::creature_route::Route::travelled>("travelled");
}
