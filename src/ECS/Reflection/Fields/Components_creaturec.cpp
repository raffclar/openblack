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

#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturec(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creaturec(entt::meta_ctx& context)
{
	Reflect<components::CreatureCasting>(context)
	    .Field<&components::CreatureCasting::move>("move")
	    .Field<&components::CreatureCasting::outcome>("outcome")
	    .Field<&components::CreatureCasting::target>("target")
	    .Field<&components::CreatureCasting::keep>("keep")
	    .Field<&components::CreatureCasting::settleSeconds>("settleSeconds")
	    .Field<&components::CreatureCasting::run>("run")
	    .Field<&components::CreatureCasting::phase>("phase")
	    .Field<&components::CreatureCasting::turns>("turns")
	    .Field<&components::CreatureCasting::holdTurns>("holdTurns")
	    .Field<&components::CreatureCasting::stuckCheck>("stuckCheck")
	    .Field<&components::CreatureCasting::destination>("destination")
	    .Field<&components::CreatureCasting::fizzle>("fizzle")
	    .Field<&components::CreatureCasting::lastFizzleTarget>("lastFizzleTarget");
	Reflect<components::CreatureCopy>(context)
	    .Field<&components::CreatureCopy::creature>("creature")
	    .Field<&components::CreatureCopy::size>("size");
	Reflect<components::CaveCreature>(context)
	    .Field<&components::CaveCreature::state>("state")
	    .Field<&components::CaveCreature::light>("light")
	    .Field<&components::CaveCreature::tattooSites>("tattooSites");
}
