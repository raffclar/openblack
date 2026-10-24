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

#include "ECS/Components/Abode.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/AlignmentHistory.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_a(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_a(entt::meta_ctx& context)
{
	Reflect<components::Abode>(context)
	    .Field<&components::Abode::type>("type")
	    .Field<&components::Abode::townId>("townId")
	    .Field<&components::Abode::foodAmount>("foodAmount")
	    .Field<&components::Abode::woodAmount>("woodAmount")
	    .Field<&components::Abode::inhabitants>("inhabitants")
	    .Field<&components::Abode::couple>("couple")
	    .Field<&components::Abode::presentAtHome>("presentAtHome")
	    .Field<&components::Abode::info>("info")
	    .Field<&components::Abode::putUpForMending>("putUpForMending")
	    .Field<&components::Abode::age>("age")
	    .Field<&components::Abode::neglect>("neglect");
	Reflect<components::Alignment>(context)
	    .Field<&components::Alignment::value>("value")
	    .Field<&components::Alignment::pending>("pending");
	Reflect<components::AlignmentHistory>(context)
	    .Field<&components::AlignmentHistory::totals>("totals")
	    .Field<&components::AlignmentHistory::counts>("counts");
	Reflect<components::Animal>(context)
	    .Field<&components::Animal::type>("type")
	    .Field<&components::Animal::owner>("owner")
	    .Field<&components::Animal::flock>("flock")
	    .Field<&components::Animal::town>("town")
	    .Field<&components::Animal::reactingTo>("reactingTo")
	    .Field<&components::Animal::reaction>("reaction")
	    .Field<&components::Animal::state>("state")
	    .Field<&components::Animal::finalState>("finalState")
	    .Field<&components::Animal::afterClip>("afterClip")
	    .Field<&components::Animal::stateBeforeReaction>("stateBeforeReaction")
	    .Field<&components::Animal::turnsInState>("turnsInState")
	    .Field<&components::Animal::move>("move")
	    .Field<&components::Animal::walksRound>("walksRound")
	    .Field<&components::Animal::goalHeight>("goalHeight")
	    .Field<&components::Animal::height>("height")
	    .Field<&components::Animal::bank>("bank")
	    .Field<&components::Animal::hunger>("hunger")
	    .Field<&components::Animal::animation>("animation")
	    .Field<&components::Animal::clipPlace>("clipPlace")
	    .Field<&components::Animal::groundCarry>("groundCarry")
	    .Field<&components::Animal::previousPosition>("previousPosition")
	    .Field<&components::Animal::position>("position")
	    .Field<&components::Animal::previousHeading>("previousHeading")
	    .Field<&components::Animal::heading>("heading")
	    .Field<&components::Animal::life>("life")
	    .Field<&components::Animal::birthTurn>("birthTurn")
	    .Field<&components::Animal::turnsUntilStateChange>("turnsUntilStateChange")
	    .Field<&components::Animal::landingPose>("landingPose");
	Reflect<components::Grazer>(context).Field<&components::Grazer::needs>("needs").Field<&components::Grazer::sleepingCell>(
	    "sleepingCell");
	Reflect<components::TempleBirds>(context)
	    .Field<&components::TempleBirds::flock>("flock")
	    .Field<&components::TempleBirds::look>("look");
	Reflect<components::SpellAnimal>(context)
	    .Field<&components::SpellAnimal::spell>("spell")
	    .Field<&components::SpellAnimal::fade>("fade")
	    .Field<&components::SpellAnimal::previous>("previous");
	Reflect<components::SpellWolf>(context)
	    .Field<&components::SpellWolf::finalDestination>("finalDestination")
	    .Field<&components::SpellWolf::corridor>("corridor")
	    .Field<&components::SpellWolf::prey>("prey")
	    .Field<&components::SpellWolf::huntStart>("huntStart")
	    .Field<&components::SpellWolf::remembered>("remembered")
	    .Field<&components::SpellWolf::food>("food");
	Reflect<components::AnimalPose>(context)
	    .Field<&components::AnimalPose::bones>("bones")
	    .Field<&components::AnimalPose::light>("light")
	    .Field<&components::AnimalPose::colour>("colour")
	    .Field<&components::AnimalPose::alpha>("alpha")
	    .Field<&components::AnimalPose::cutBelow>("cutBelow")
	    .Field<&components::AnimalPose::aboveCutShown>("aboveCutShown")
	    .Field<&components::AnimalPose::underSea>("underSea");
	Reflect<components::BroughtDown> {context};
	Reflect<components::AnimatedStatic>(context)
	    .Field<&components::AnimatedStatic::type>("type")
	    .Field<&components::AnimatedStatic::openState>("openState")
	    .Field<&components::AnimatedStatic::gateStones>("gateStones");
	Reflect<components::AnimatedStaticPose>(context)
	    .Field<&components::AnimatedStaticPose::place>("place")
	    .Field<&components::AnimatedStaticPose::restingPlace>("restingPlace")
	    .Field<&components::AnimatedStaticPose::inDrawList>("inDrawList")
	    .Field<&components::AnimatedStaticPose::onScreen>("onScreen")
	    .Field<&components::AnimatedStaticPose::bones>("bones")
	    .Field<&components::AnimatedStaticPose::stones>("stones");
	Reflect<components::PlinthStone>(context)
	    .Field<&components::PlinthStone::plinth>("plinth")
	    .Field<&components::PlinthStone::pickable>("pickable");
	Reflect<components::GateLantern>(context).Field<&components::GateLantern::gate>("gate");
	Reflect<components::AppleBarrel>(context)
	    .Field<&components::AppleBarrel::store>("store")
	    .Field<&components::AppleBarrel::worm>("worm");
	Reflect<components::AppleBarrelWorm>(context)
	    .Field<&components::AppleBarrelWorm::barrel>("barrel")
	    .Field<&components::AppleBarrelWorm::place>("place");
	Reflect<components::AtHome>(context).Field<&components::AtHome::beenToBed>("beenToBed");
	Reflect<components::AudioEmitter>(context)
	    .RunField<&components::AudioEmitter::sourceId>("sourceId")
	    .Field<&components::AudioEmitter::soundId>("soundId")
	    .Field<&components::AudioEmitter::priority>("priority")
	    .Field<&components::AudioEmitter::spatial>("spatial")
	    .Field<&components::AudioEmitter::position>("position")
	    .Field<&components::AudioEmitter::lockedAxes>("lockedAxes")
	    .Field<&components::AudioEmitter::gain>("gain")
	    .Field<&components::AudioEmitter::volume>("volume")
	    .Field<&components::AudioEmitter::pitchPercent>("pitchPercent")
	    .Field<&components::AudioEmitter::minDistance>("minDistance")
	    .Field<&components::AudioEmitter::maxDistance>("maxDistance")
	    .Field<&components::AudioEmitter::distanceScale>("distanceScale")
	    .Field<&components::AudioEmitter::headerSet>("headerSet")
	    .Field<&components::AudioEmitter::loop>("loop")
	    .Field<&components::AudioEmitter::state>("state")
	    .Field<&components::AudioEmitter::music>("music")
	    .Field<&components::AudioEmitter::owner>("owner")
	    .Field<&components::AudioEmitter::follows>("follows")
	    .Field<&components::AudioEmitter::bank>("bank")
	    .Field<&components::AudioEmitter::group>("group");
}
