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

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Construction.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_co(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_co(entt::meta_ctx& context)
{
	Reflect<components::ComputerPlayer>(context)
	    .Field<&components::ComputerPlayer::switchedOn>("switchedOn")
	    .Field<&components::ComputerPlayer::marked>("marked")
	    .Field<&components::ComputerPlayer::started>("started")
	    .Field<&components::ComputerPlayer::creatureLike>("creatureLike")
	    .Field<&components::ComputerPlayer::mind>("mind")
	    .Field<&components::ComputerPlayer::hand>("hand")
	    .Field<&components::ComputerPlayer::casts>("casts");
	Reflect<components::PlannedTemple>(context)
	    .Field<&components::PlannedTemple::townId>("townId")
	    .Field<&components::PlannedTemple::owner>("owner")
	    .Field<&components::PlannedTemple::yAngle>("yAngle");
	Reflect<components::PlannedAbode>(context)
	    .Field<&components::PlannedAbode::townId>("townId")
	    .Field<&components::PlannedAbode::info>("info")
	    .Field<&components::PlannedAbode::yAngle>("yAngle")
	    .Field<&components::PlannedAbode::wasBuilt>("wasBuilt");
	Reflect<components::BuildingSite>(context)
	    .Field<&components::BuildingSite::town>("town")
	    .Field<&components::BuildingSite::desire>("desire")
	    .Field<&components::BuildingSite::repair>("repair")
	    .Field<&components::BuildingSite::takesBroughtWood>("takesBroughtWood")
	    .Field<&components::BuildingSite::workers>("workers")
	    .Field<&components::BuildingSite::workerCount>("workerCount")
	    .Field<&components::BuildingSite::places>("places")
	    .Field<&components::BuildingSite::piles>("piles");
	Reflect<components::BuildingSites>(context).Field<&components::BuildingSites::sites>("sites");
	Reflect<components::SitePile> {context};
}
