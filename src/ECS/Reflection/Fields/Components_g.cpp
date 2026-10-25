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

#include "ECS/Components/GodHand.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/GripLandscapeParticle.h"
#include "ECS/Components/GroundPrint.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_g(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_g(entt::meta_ctx& context)
{
	Reflect<components::GodHand>(context).Field<&components::GodHand::player>("player");
	Reflect<components::Graveyard>(context).Field<&components::Graveyard::burials>("burials");
	Reflect<components::StandInGraves>(context)
	    .Field<&components::StandInGraves::count>("count")
	    .Field<&components::StandInGraves::partsShown>("partsShown");
	Reflect<components::WalkingSkeleton> {context};
	Reflect<components::GripLandscapeParticle>(context)
	    .Field<&components::GripLandscapeParticle::centre>("centre")
	    .Field<&components::GripLandscapeParticle::offset>("offset")
	    .Field<&components::GripLandscapeParticle::age>("age")
	    .Field<&components::GripLandscapeParticle::frame>("frame");
	Reflect<components::GroundPrint>(context).Field<&components::GroundPrint::made>("made");
	Reflect<components::GuidanceHeard>(context)
	    .Field<&components::GuidanceHeard::lastTurn>("lastTurn")
	    .Field<&components::GuidanceHeard::alignmentDrift>("alignmentDrift")
	    .Field<&components::GuidanceHeard::oneOffSaid>("oneOffSaid")
	    .Field<&components::GuidanceHeard::lastDesireVoice>("lastDesireVoice")
	    .Field<&components::GuidanceHeard::seenThings>("seenThings");
}
