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

#include "ECS/Components/Creature.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creature(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creature(entt::meta_ctx& context)
{
	Reflect<components::Creature>(context)
	    .Field<&components::Creature::owner>("owner")
	    .Field<&components::Creature::leashable>("leashable")
	    .Field<&components::Creature::species>("species")
	    .Field<&components::Creature::mind>("mind")
	    .Field<&components::Creature::alignment>("alignment")
	    .Field<&components::Creature::pendingAlignment>("pendingAlignment")
	    .Field<&components::Creature::fatness>("fatness")
	    .Field<&components::Creature::strength>("strength")
	    .Field<&components::Creature::size>("size")
	    .Field<&components::Creature::made>("made")
	    .Field<&components::Creature::creationIndex>("creationIndex")
	    .Field<&components::Creature::penSize>("penSize")
	    .Field<&components::Creature::autoScale>("autoScale")
	    .Field<&components::Creature::magicMushroomsEaten>("magicMushroomsEaten")
	    .Field<&components::Creature::objectsDestroyed>("objectsDestroyed")
	    .Field<&components::Creature::canDie>("canDie")
	    .Field<&components::Creature::mayGoThroughVortex>("mayGoThroughVortex")
	    .Field<&components::Creature::name>("name");
}
