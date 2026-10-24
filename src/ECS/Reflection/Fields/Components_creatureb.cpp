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

#include "ECS/Components/CreatureBody.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creatureb(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creatureb(entt::meta_ctx& context)
{
	Reflect<components::CreatureMorph>(context)
	    .Field<&components::CreatureMorph::shownFatness>("shownFatness")
	    .Field<&components::CreatureMorph::drawn>("drawn")
	    .Field<&components::CreatureMorph::strengthLook>("strengthLook")
	    .Field<&components::CreatureMorph::alignmentLook>("alignmentLook")
	    .Field<&components::CreatureMorph::revision>("revision");
	Reflect<components::CreatureAnimation>(context)
	    .Field<&components::CreatureAnimation::body>("body")
	    .Field<&components::CreatureAnimation::actionMoment>("actionMoment")
	    .Field<&components::CreatureAnimation::slots>("slots")
	    .Field<&components::CreatureAnimation::face>("face")
	    .Field<&components::CreatureAnimation::gesture>("gesture")
	    .Field<&components::CreatureAnimation::wobble>("wobble")
	    .Field<&components::CreatureAnimation::sway>("sway")
	    .Field<&components::CreatureAnimation::lookAt>("lookAt")
	    .Field<&components::CreatureAnimation::yaw>("yaw")
	    .Field<&components::CreatureAnimation::pitch>("pitch")
	    .Field<&components::CreatureAnimation::playbackScale>("playbackScale")
	    .Field<&components::CreatureAnimation::realTime>("realTime")
	    .Field<&components::CreatureAnimation::breathPhase>("breathPhase")
	    .Field<&components::CreatureAnimation::breathPeriod>("breathPeriod")
	    .Field<&components::CreatureAnimation::breathTarget>("breathTarget")
	    .Field<&components::CreatureAnimation::builtRevision>("builtRevision")
	    .Field<&components::CreatureAnimation::skeleton>("skeleton")
	    .Field<&components::CreatureAnimation::animations>("animations")
	    .Field<&components::CreatureAnimation::pair>("pair")
	    .Field<&components::CreatureAnimation::mirror>("mirror")
	    .Field<&components::CreatureAnimation::boneMatrices>("boneMatrices")
	    .Field<&components::CreatureAnimation::olderTurnPose>("olderTurnPose")
	    .Field<&components::CreatureAnimation::newerTurnPose>("newerTurnPose")
	    .Field<&components::CreatureAnimation::posedThisTurn>("posedThisTurn")
	    .Field<&components::CreatureAnimation::lastTurnBones>("lastTurnBones");
	Reflect<components::CreatureEyes>(context)
	    .Field<&components::CreatureEyes::mode>("mode")
	    .Field<&components::CreatureEyes::openness>("openness")
	    .Field<&components::CreatureEyes::blink>("blink")
	    .Field<&components::CreatureEyes::lookAt>("lookAt")
	    .Field<&components::CreatureEyes::look>("look")
	    .Field<&components::CreatureEyes::lookStarted>("lookStarted")
	    .Field<&components::CreatureEyes::drawn>("drawn")
	    .Field<&components::CreatureEyes::lidColour>("lidColour");
}
