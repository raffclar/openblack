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

#include "Animals/AnimalMove.h"
#include "Animals/GrazerRules.h"
#include "Audio/AnimEffectKeys.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_a(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_a(entt::meta_ctx& context)
{
	Reflect<openblack::animals::Move>(context, ValueOnly {})
	    .Field<&openblack::animals::Move::position>("position")
	    .Field<&openblack::animals::Move::goal>("goal")
	    .Field<&openblack::animals::Move::step>("step")
	    .Field<&openblack::animals::Move::angle>("angle")
	    .Field<&openblack::animals::Move::speed>("speed")
	    .Field<&openblack::animals::Move::stage>("stage");
	Reflect<openblack::animals::grazers::Needs>(context, ValueOnly {})
	    .Field<&openblack::animals::grazers::Needs::hunger>("hunger")
	    .Field<&openblack::animals::grazers::Needs::sleep>("sleep")
	    .Field<&openblack::animals::grazers::Needs::breed>("breed");
	Reflect<openblack::audio::AnimEffectKeys>(context, ValueOnly {})
	    .Field<&openblack::audio::AnimEffectKeys::size>("size")
	    .Field<&openblack::audio::AnimEffectKeys::alignment>("alignment")
	    .Field<&openblack::audio::AnimEffectKeys::object>("object")
	    .Field<&openblack::audio::AnimEffectKeys::surface>("surface")
	    .Field<&openblack::audio::AnimEffectKeys::action>("action");
}
