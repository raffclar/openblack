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

#include "ECS/Components/ObjectGlow.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_o(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_o(entt::meta_ctx& context)
{
	Reflect<components::ObjectGlow>(context)
	    .Field<&components::ObjectGlow::rgb>("rgb")
	    .Field<&components::ObjectGlow::turnsUnset>("turnsUnset");
	Reflect<components::OneOffSpellSeed>(context)
	    .Field<&components::OneOffSpellSeed::seedType>("seedType")
	    .Field<&components::OneOffSpellSeed::magicType>("magicType")
	    .Field<&components::OneOffSpellSeed::position>("position")
	    .Field<&components::OneOffSpellSeed::powerUp>("powerUp")
	    .Field<&components::OneOffSpellSeed::multiplier>("multiplier")
	    .Field<&components::OneOffSpellSeed::middle>("middle")
	    .Field<&components::OneOffSpellSeed::spin>("spin")
	    .Field<&components::OneOffSpellSeed::ringSpin>("ringSpin")
	    .Field<&components::OneOffSpellSeed::glintFrame>("glintFrame")
	    .Field<&components::OneOffSpellSeed::phialFrame>("phialFrame")
	    .Field<&components::OneOffSpellSeed::phialPhase>("phialPhase")
	    .Field<&components::OneOffSpellSeed::seedPlacement>("seedPlacement")
	    .Field<&components::OneOffSpellSeed::drawnModel>("drawnModel")
	    .Field<&components::OneOffSpellSeed::drawnTurn>("drawnTurn")
	    .Field<&components::OneOffSpellSeed::dispenser>("dispenser");
}
