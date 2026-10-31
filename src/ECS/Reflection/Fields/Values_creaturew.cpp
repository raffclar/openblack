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

#include "Creature/CreatureWatching.h"
#include "Creature/CreatureWebPage.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturew(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creaturew(entt::meta_ctx& context)
{
	Reflect<openblack::creature_watching::Knowledge>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::Knowledge::skillsSeen>("skillsSeen")
	    .Field<&openblack::creature_watching::Knowledge::miraclesSeen>("miraclesSeen")
	    .Field<&openblack::creature_watching::Knowledge::skillsKnown>("skillsKnown")
	    .Field<&openblack::creature_watching::Knowledge::miraclesKnown>("miraclesKnown")
	    .Field<&openblack::creature_watching::Knowledge::skillsKnownPastTable>("skillsKnownPastTable")
	    .Field<&openblack::creature_watching::Knowledge::miraclesKnownPastTable>("miraclesKnownPastTable");
	Reflect<openblack::creature_watching::Mimicry>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::Mimicry::rule>("rule")
	    .Field<&openblack::creature_watching::Mimicry::stage>("stage")
	    .Field<&openblack::creature_watching::Mimicry::done>("done")
	    .Field<&openblack::creature_watching::Mimicry::limit>("limit")
	    .Field<&openblack::creature_watching::Mimicry::stageStart>("stageStart")
	    .Field<&openblack::creature_watching::Mimicry::object>("object")
	    .Field<&openblack::creature_watching::Mimicry::magic>("magic");
	Reflect<openblack::creature_watching::ObservedDeed>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::ObservedDeed::rule>("rule")
	    .Field<&openblack::creature_watching::ObservedDeed::object>("object")
	    .Field<&openblack::creature_watching::ObservedDeed::magic>("magic");
	Reflect<openblack::creature_watching::Sighting>(context, ValueOnly {})
	    .Field<&openblack::creature_watching::Sighting::count>("count")
	    .Field<&openblack::creature_watching::Sighting::turn>("turn");
	Reflect<openblack::creature_web_page::Picture>(context, ValueOnly {})
	    .Field<&openblack::creature_web_page::Picture::score>("score")
	    .Field<&openblack::creature_web_page::Picture::age>("age")
	    .Field<&openblack::creature_web_page::Picture::action>("action");
}
