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

#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creatureo(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creatureo(entt::meta_ctx& context)
{
	Reflect<components::CreatureObjectAction>(context)
	    .Field<&components::CreatureObjectAction::kind>("kind")
	    .Field<&components::CreatureObjectAction::phase>("phase")
	    .Field<&components::CreatureObjectAction::status>("status")
	    .Field<&components::CreatureObjectAction::target>("target")
	    .Field<&components::CreatureObjectAction::point>("point")
	    .Field<&components::CreatureObjectAction::animations>("animations")
	    .Field<&components::CreatureObjectAction::weights>("weights")
	    .Field<&components::CreatureObjectAction::animationCount>("animationCount")
	    .Field<&components::CreatureObjectAction::mirrored>("mirrored")
	    .Field<&components::CreatureObjectAction::timeMs>("timeMs")
	    .Field<&components::CreatureObjectAction::durationMs>("durationMs")
	    .Field<&components::CreatureObjectAction::hurried>("hurried")
	    .Field<&components::CreatureObjectAction::eventMs>("eventMs")
	    .Field<&components::CreatureObjectAction::eventDone>("eventDone")
	    .Field<&components::CreatureObjectAction::holdMs>("holdMs")
	    .Field<&components::CreatureObjectAction::pointSeconds>("pointSeconds")
	    .Field<&components::CreatureObjectAction::pointsUntilTold>("pointsUntilTold")
	    .Field<&components::CreatureObjectAction::pointEnding>("pointEnding")
	    .Field<&components::CreatureObjectAction::pointMs>("pointMs")
	    .Field<&components::CreatureObjectAction::pointTurned>("pointTurned")
	    .Field<&components::CreatureObjectAction::reach>("reach")
	    .Field<&components::CreatureObjectAction::maxReach>("maxReach")
	    .Field<&components::CreatureObjectAction::attempts>("attempts")
	    .Field<&components::CreatureObjectAction::walked>("walked")
	    .Field<&components::CreatureObjectAction::stall>("stall")
	    .Field<&components::CreatureObjectAction::turnedToReach>("turnedToReach")
	    .Field<&components::CreatureObjectAction::waitedForWay>("waitedForWay")
	    .Field<&components::CreatureObjectAction::flightSeconds>("flightSeconds")
	    .Field<&components::CreatureObjectAction::givenFlightSeconds>("givenFlightSeconds")
	    .Field<&components::CreatureObjectAction::waitsForLanding>("waitsForLanding")
	    .Field<&components::CreatureObjectAction::thrown>("thrown")
	    .Field<&components::CreatureObjectAction::catchHands>("catchHands")
	    .Field<&components::CreatureObjectAction::catching>("catching")
	    .Field<&components::CreatureObjectAction::catchTurned>("catchTurned")
	    .Field<&components::CreatureObjectAction::catchHeight>("catchHeight")
	    .Field<&components::CreatureObjectAction::swallows>("swallows")
	    .Field<&components::CreatureObjectAction::eaten>("eaten")
	    .Field<&components::CreatureObjectAction::mealEnergy>("mealEnergy")
	    .Field<&components::CreatureObjectAction::failure>("failure")
	    .Field<&components::CreatureObjectAction::failureKind>("failureKind");
	Reflect<components::CreatureDroppedObject>(context).Field<&components::CreatureDroppedObject::object>("object");
	Reflect<components::CreatureHeldObject>(context)
	    .Field<&components::CreatureHeldObject::object>("object")
	    .Field<&components::CreatureHeldObject::mirrored>("mirrored")
	    .Field<&components::CreatureHeldObject::bone>("bone");
	Reflect<components::PendingCatch>(context).Field<&components::PendingCatch::object>("object");
	Reflect<components::CatchHands>(context).Field<&components::CatchHands::hands>("hands");
	Reflect<components::PickUpReach>(context).Field<&components::PickUpReach::points>("points");
	Reflect<components::StompReach>(context).Field<&components::StompReach::points>("points");
	Reflect<components::TossHandVelocity>(context).Field<&components::TossHandVelocity::velocity>("velocity");
	Reflect<components::HeldByCreature>(context)
	    .Field<&components::HeldByCreature::creature>("creature")
	    .Field<&components::HeldByCreature::drawn>("drawn");
}
