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

#include "ECS/Components/Hand.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/HandGlow.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandLockedCreature.h"
#include "ECS/Components/HandMiracleFx.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/HandOnCreature.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/Heartbeat.h"
#include "ECS/Components/HeldDrawTrack.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_h(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_h(entt::meta_ctx& context)
{
	Reflect<components::Hand>(context)
	    .Field<&components::Hand::rightHanded>("rightHanded")
	    .Field<&components::Hand::renderType>("renderType")
	    .Field<&components::Hand::boneMatrices>("boneMatrices");
	Reflect<components::HandClicked>(context)
	    .Field<&components::HandClicked::thing>("thing")
	    .Field<&components::HandClicked::thingTurn>("thingTurn")
	    .Field<&components::HandClicked::place>("place")
	    .Field<&components::HandClicked::placeTurn>("placeTurn");
	Reflect<components::HandGlow>(context).Field<&components::HandGlow::rgb>("rgb");
	Reflect<components::InHand>(context)
	    .Field<&components::InHand::hand>("hand")
	    .Field<&components::InHand::rotation>("rotation")
	    .Field<&components::InHand::scale>("scale");
	Reflect<components::HeldLight>(context)
	    .Field<&components::HeldLight::takenAt>("takenAt")
	    .Field<&components::HeldLight::drawn>("drawn")
	    .Field<&components::HeldLight::point>("point")
	    .Field<&components::HeldLight::viewDepth>("viewDepth")
	    .Field<&components::HeldLight::tint>("tint")
	    .Field<&components::HeldLight::added>("added")
	    .Field<&components::HeldLight::treeGrey>("treeGrey");
	Reflect<components::CannotBePickedUp> {context};
	Reflect<components::HandGrab>(context)
	    .Field<&components::HandGrab::state>("state")
	    .Field<&components::HandGrab::object>("object")
	    .Field<&components::HandGrab::pressMs>("pressMs")
	    .Field<&components::HandGrab::pressTurn>("pressTurn")
	    .Field<&components::HandGrab::waits>("waits")
	    .Field<&components::HandGrab::pullSeconds>("pullSeconds")
	    .Field<&components::HandGrab::pulling>("pulling")
	    .Field<&components::HandGrab::grips>("grips")
	    .Field<&components::HandGrab::tug>("tug")
	    .Field<&components::HandGrab::pullPosition>("pullPosition")
	    .Field<&components::HandGrab::pullRotation>("pullRotation")
	    .Field<&components::HandGrab::pullScale>("pullScale")
	    .Field<&components::HandGrab::stretch>("stretch")
	    .Field<&components::HandGrab::holdDistance>("holdDistance")
	    .Field<&components::HandGrab::pullPlanePoint>("pullPlanePoint")
	    .Field<&components::HandGrab::pullPlaneNormal>("pullPlaneNormal")
	    .Field<&components::HandGrab::hold>("hold")
	    .Field<&components::HandGrab::lowering>("lowering")
	    .Field<&components::HandGrab::rise>("rise")
	    .Field<&components::HandGrab::spring>("spring")
	    .Field<&components::HandGrab::springOn>("springOn")
	    .Field<&components::HandGrab::springPending>("springPending")
	    .Field<&components::HandGrab::recordedThrow>("recordedThrow")
	    .Field<&components::HandGrab::lastThrow>("lastThrow")
	    .Field<&components::HandGrab::lastTarget>("lastTarget")
	    .Field<&components::HandGrab::scoopSource>("scoopSource")
	    .Field<&components::HandGrab::scoopTurns>("scoopTurns")
	    .Field<&components::HandGrab::locking>("locking")
	    .Field<&components::HandGrab::scoopStream>("scoopStream")
	    .Field<&components::HandGrab::scoopSound>("scoopSound")
	    .Field<&components::HandGrab::scoopAnchor>("scoopAnchor")
	    .Field<&components::HandGrab::released>("released")
	    .Field<&components::HandGrab::releaseSpinMs>("releaseSpinMs")
	    .Field<&components::HandGrab::pourEffect>("pourEffect")
	    .Field<&components::HandGrab::pourSeconds>("pourSeconds")
	    .Field<&components::HandGrab::scoopStreamSeconds>("scoopStreamSeconds")
	    .Field<&components::HandGrab::handSize>("handSize")
	    .Field<&components::HandGrab::handPoint>("handPoint")
	    .Field<&components::HandGrab::offeredTo>("offeredTo")
	    .Field<&components::HandGrab::offer>("offer")
	    .Field<&components::HandGrab::givenTo>("givenTo")
	    .Field<&components::HandGrab::offerTurn>("offerTurn")
	    .Field<&components::HandGrab::lastHand>("lastHand")
	    .Field<&components::HandGrab::lastPickedUp>("lastPickedUp")
	    .Field<&components::HandGrab::lastDropped>("lastDropped");
	Reflect<components::HandLockedCreature>(context).Field<&components::HandLockedCreature::creature>("creature");
	Reflect<components::HandMiracleFx>(context)
	    .Field<&components::HandMiracleFx::bands>("bands")
	    .Field<&components::HandMiracleFx::glowing>("glowing")
	    .Field<&components::HandMiracleFx::glowFrame>("glowFrame");
	Reflect<components::HandMorph>(context)
	    .Field<&components::HandMorph::state>("state")
	    .Field<&components::HandMorph::point>("point")
	    .Field<&components::HandMorph::pointInInfluence>("pointInInfluence")
	    .Field<&components::HandMorph::skins>("skins")
	    .Field<&components::HandMorph::revision>("revision");
	Reflect<components::HandOnCreature>(context)
	    .Field<&components::HandOnCreature::creature>("creature")
	    .Field<&components::HandOnCreature::sum>("sum")
	    .Field<&components::HandOnCreature::clocks>("clocks")
	    .Field<&components::HandOnCreature::lastPart>("lastPart")
	    .Field<&components::HandOnCreature::sinceStepMs>("sinceStepMs")
	    .Field<&components::HandOnCreature::lastCursor>("lastCursor")
	    .Field<&components::HandOnCreature::lastPoint>("lastPoint")
	    .Field<&components::HandOnCreature::wasOnBody>("wasOnBody")
	    .Field<&components::HandOnCreature::speed>("speed")
	    .Field<&components::HandOnCreature::hardSlaps>("hardSlaps")
	    .Field<&components::HandOnCreature::cycle>("cycle")
	    .Field<&components::HandOnCreature::byCommand>("byCommand")
	    .Field<&components::HandOnCreature::heldMs>("heldMs")
	    .Field<&components::HandOnCreature::strokedOrSlapped>("strokedOrSlapped")
	    .Field<&components::HandOnCreature::up>("up")
	    .Field<&components::HandOnCreature::upStarted>("upStarted")
	    .Field<&components::HandOnCreature::frameCursor>("frameCursor")
	    .Field<&components::HandOnCreature::distance>("distance")
	    .Field<&components::HandOnCreature::distanceStarted>("distanceStarted")
	    .Field<&components::HandOnCreature::shownAt>("shownAt");
	Reflect<components::HandLastFeedback>(context).Field<&components::HandLastFeedback::sum>("sum");
	Reflect<components::HandStillness>(context)
	    .Field<&components::HandStillness::stillTurns>("stillTurns")
	    .Field<&components::HandStillness::drift>("drift")
	    .Field<&components::HandStillness::last>("last")
	    .Field<&components::HandStillness::velocity>("velocity");
	Reflect<components::Heartbeat>(context).Field<&components::Heartbeat::state>("state");
	Reflect<components::HeldDrawTrack>(context)
	    .Field<&components::HeldDrawTrack::from>("from")
	    .Field<&components::HeldDrawTrack::elapsedMs>("elapsedMs")
	    .Field<&components::HeldDrawTrack::intervalMs>("intervalMs");
	Reflect<components::HiddenByState> {context};
	Reflect<components::HighDetail>(context)
	    .Field<&components::HighDetail::usualModel>("usualModel")
	    .Field<&components::HighDetail::usualDetailModels>("usualDetailModels")
	    .Field<&components::HighDetail::face>("face")
	    .Field<&components::HighDetail::orders>("orders")
	    .Field<&components::HighDetail::heldAt>("heldAt")
	    .Field<&components::HighDetail::eyes>("eyes")
	    .Field<&components::HighDetail::drawnEyes>("drawnEyes");
}
