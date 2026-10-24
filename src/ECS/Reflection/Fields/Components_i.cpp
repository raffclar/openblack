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

#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/InterfaceLimits.h"
#include "ECS/Components/IntroHand.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_i(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_i(entt::meta_ctx& context)
{
	Reflect<components::Indestructible> {context};
	Reflect<components::InfluenceSource>(context)
	    .Field<&components::InfluenceSource::player>("player")
	    .Field<&components::InfluenceSource::radius>("radius")
	    .Field<&components::InfluenceSource::anti>("anti")
	    .Field<&components::InfluenceSource::follows>("follows");
	Reflect<components::TownInfluence>(context)
	    .Field<&components::TownInfluence::radius>("radius")
	    .Field<&components::TownInfluence::drawnRadius>("drawnRadius");
	Reflect<components::CitadelInfluence>(context)
	    .Field<&components::CitadelInfluence::reach>("reach")
	    .Field<&components::CitadelInfluence::drawnRadius>("drawnRadius");
	Reflect<components::VirtualInfluence>(context)
	    .Field<&components::VirtualInfluence::state>("state")
	    .Field<&components::VirtualInfluence::hum>("hum");
	Reflect<components::InterfaceLimits>(context)
	    .Field<&components::InterfaceLimits::onlyHighlights>("onlyHighlights")
	    .Field<&components::InterfaceLimits::noHolding>("noHolding");
	Reflect<components::IntroHand>(context)
	    .Field<&components::IntroHand::clip>("clip")
	    .Field<&components::IntroHand::time>("time")
	    .Field<&components::IntroHand::yaw>("yaw")
	    .Field<&components::IntroHand::scale>("scale");
}
