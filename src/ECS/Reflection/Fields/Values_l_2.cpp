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

#include "3D/Lightning.h"
#include "Creature/LeashRope.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_l_2(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_l_2(entt::meta_ctx& context)
{
	Reflect<openblack::leash_rope::Look>(context, ValueOnly {})
	    .Field<&openblack::leash_rope::Look::halfWidth>("halfWidth")
	    .Field<&openblack::leash_rope::Look::v0>("v0")
	    .Field<&openblack::leash_rope::Look::v1>("v1")
	    .Field<&openblack::leash_rope::Look::uScale>("uScale");
	Reflect<openblack::leash_rope::Node>(context, ValueOnly {})
	    .Field<&openblack::leash_rope::Node::position>("position")
	    .Field<&openblack::leash_rope::Node::velocity>("velocity")
	    .Field<&openblack::leash_rope::Node::stretch>("stretch");
	Reflect<openblack::leash_rope::Rope>(context, ValueOnly {})
	    .Field<&openblack::leash_rope::Rope::start>("start")
	    .Field<&openblack::leash_rope::Rope::end>("end")
	    .Field<&openblack::leash_rope::Rope::nodes>("nodes")
	    .Field<&openblack::leash_rope::Rope::slackLength>("slackLength")
	    .Field<&openblack::leash_rope::Rope::maxLength>("maxLength")
	    .Field<&openblack::leash_rope::Rope::look>("look")
	    .Field<&openblack::leash_rope::Rope::tension>("tension");
	Reflect<openblack::lightning::Flash>(context, ValueOnly {})
	    .Field<&openblack::lightning::Flash::position>("position")
	    .Field<&openblack::lightning::Flash::radius>("radius")
	    .Field<&openblack::lightning::Flash::strength>("strength")
	    .Field<&openblack::lightning::Flash::time>("time")
	    .Field<&openblack::lightning::Flash::active>("active");
}
