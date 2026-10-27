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

#include "3D/HandMorph.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Hand/HandGrabRules.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_h(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_h(entt::meta_ctx& context)
{
	Reflect<openblack::hand_grab::HoldFacts>(context, ValueOnly {})
	    .Field<&openblack::hand_grab::HoldFacts::type>("type")
	    .Field<&openblack::hand_grab::HoldFacts::loweringMultiplier>("loweringMultiplier")
	    .Field<&openblack::hand_grab::HoldFacts::holdRadius>("holdRadius");
	Reflect<openblack::hand_grab::Tug>(context, ValueOnly {})
	    .Field<&openblack::hand_grab::Tug::axes>("axes")
	    .Field<&openblack::hand_grab::Tug::base>("base")
	    .Field<&openblack::hand_grab::Tug::spin>("spin")
	    .Field<&openblack::hand_grab::Tug::pullVelocity>("pullVelocity");
	Reflect<openblack::hand_morph::State>(context, ValueOnly {})
	    .Field<&openblack::hand_morph::State::target>("target")
	    .Field<&openblack::hand_morph::State::drawn>("drawn")
	    .Field<&openblack::hand_morph::State::inInfluence>("inInfluence");
}
