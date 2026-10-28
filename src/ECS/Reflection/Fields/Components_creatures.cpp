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

#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureSquat.h"
#include "ECS/Components/CreatureSwap.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creatures(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creatures(entt::meta_ctx& context)
{
	Reflect<components::CreatureTattoos>(context)
	    .Field<&components::CreatureTattoos::slots>("slots")
	    .Field<&components::CreatureTattoos::pictures>("pictures")
	    .Field<&components::CreatureTattoos::shown>("shown")
	    .Field<&components::CreatureTattoos::picturesShown>("picturesShown")
	    .Field<&components::CreatureTattoos::shownPictures>("shownPictures")
	    .Field<&components::CreatureTattoos::revision>("revision");
	Reflect<components::TattooMasks>(context).Field<&components::TattooMasks::masks>("masks");
	Reflect<components::CreatureMarks>(context)
	    .Field<&components::CreatureMarks::marks>("marks")
	    .Field<&components::CreatureMarks::revision>("revision");
	Reflect<components::CreatureSkin>(context)
	    .Field<&components::CreatureSkin::skins>("skins")
	    .Field<&components::CreatureSkin::painted>("painted")
	    .Field<&components::CreatureSkin::revision>("revision");
	Reflect<components::CreatureDrips>(context).Field<&components::CreatureDrips::drips>("drips");
	Reflect<components::CreatureSpells>(context)
	    .Field<&components::CreatureSpells::spells>("spells")
	    .Field<&components::CreatureSpells::freeze>("freeze")
	    .Field<&components::CreatureSpells::fizz>("fizz")
	    .Field<&components::CreatureSpells::staticScroll>("staticScroll")
	    .Field<&components::CreatureSpells::invisible>("invisible")
	    .Field<&components::CreatureSpells::pausedMind>("pausedMind")
	    .Field<&components::CreatureSpells::smallestSize>("smallestSize")
	    .Field<&components::CreatureSpells::largestSize>("largestSize")
	    .Field<&components::CreatureSpells::cheat>("cheat");
	Reflect<components::CreatureSquat>(context)
	    .Field<&components::CreatureSquat::turns>("turns")
	    .Field<&components::CreatureSquat::smokePlan>("smokePlan");
	Reflect<components::CreatureSwap>(context)
	    .Field<&components::CreatureSwap::other>("other")
	    .Field<&components::CreatureSwap::target>("target")
	    .Field<&components::CreatureSwap::beam>("beam");
}
