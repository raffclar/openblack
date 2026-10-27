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

#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureDestruction.h"
#include "ECS/Components/CreatureDrawnFeet.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creatured(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creatured(entt::meta_ctx& context)
{
	Reflect<components::CreatureDance>(context)
	    .Field<&components::CreatureDance::dance>("dance")
	    .Field<&components::CreatureDance::startedBy>("startedBy")
	    .Field<&components::CreatureDance::dancers>("dancers")
	    .Field<&components::CreatureDance::calledBy>("calledBy")
	    .Field<&components::CreatureDance::lastPlaces>("lastPlaces")
	    .Field<&components::CreatureDance::lastImpressiveTurn>("lastImpressiveTurn")
	    .Field<&components::CreatureDance::lastImpressiveType>("lastImpressiveType");
	Reflect<components::CreatureDestruction>(context)
	    .Field<&components::CreatureDestruction::on>("on")
	    .Field<&components::CreatureDestruction::last>("last")
	    .Field<&components::CreatureDestruction::radius>("radius")
	    .Field<&components::CreatureDestruction::seen>("seen");
	Reflect<components::CreatureDrawnFeet>(context)
	    .Field<&components::CreatureDrawnFeet::right>("right")
	    .Field<&components::CreatureDrawnFeet::left>("left");
}
