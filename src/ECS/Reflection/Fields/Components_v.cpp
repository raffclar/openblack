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

#include "ECS/Components/Velocity.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerCreatureInspection.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/VillagerHiding.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/VillagerWork.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_v(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_v(entt::meta_ctx& context)
{
	Reflect<components::Velocity>(context)
	    .Field<&components::Velocity::dX>("dX")
	    .Field<&components::Velocity::dY>("dY")
	    .Field<&components::Velocity::dZ>("dZ");
	Reflect<components::VillageLight>(context)
	    .Field<&components::VillageLight::kind>("kind")
	    .Field<&components::VillageLight::flickerTimer>("flickerTimer")
	    .Field<&components::VillageLight::flicker>("flicker")
	    .Field<&components::VillageLight::glowSize>("glowSize")
	    .Field<&components::VillageLight::owner>("owner");
	Reflect<components::VillageLightSprite>(context)
	    .Field<&components::VillageLightSprite::light>("light")
	    .Field<&components::VillageLightSprite::index>("index");
	Reflect<components::VillageTotem>(context)
	    .Field<&components::VillageTotem::townCentre>("townCentre")
	    .Field<&components::VillageTotem::icon>("icon")
	    .Field<&components::VillageTotem::restY>("restY")
	    .Field<&components::VillageTotem::ease>("ease")
	    .Field<&components::VillageTotem::held>("held")
	    .Field<&components::VillageTotem::gripped>("gripped")
	    .Field<&components::VillageTotem::ghost>("ghost")
	    .Field<&components::VillageTotem::ghostIcon>("ghostIcon")
	    .Field<&components::VillageTotem::lastShownRise>("lastShownRise")
	    .Field<&components::VillageTotem::quietOnce>("quietOnce");
	Reflect<components::VillageTotemIcon> {context};
	Reflect<components::VillageTotemGhost> {context};
	Reflect<components::VillageTotemGhostHidden> {context};
	Reflect<components::Villager>(context)
	    .Field<&components::Villager::life>("life")
	    .Field<&components::Villager::birthTurn>("birthTurn")
	    .Field<&components::Villager::food>("food")
	    .Field<&components::Villager::lifeStage>("lifeStage")
	    .Field<&components::Villager::sex>("sex")
	    .Field<&components::Villager::tribe>("tribe")
	    .Field<&components::Villager::number>("number")
	    .Field<&components::Villager::task>("task")
	    .Field<&components::Villager::town>("town")
	    .Field<&components::Villager::abode>("abode")
	    .Field<&components::Villager::lastCheckTurn>("lastCheckTurn")
	    .Field<&components::Villager::carried>("carried")
	    .Field<&components::Villager::transitionPlaying>("transitionPlaying")
	    .Field<&components::Villager::intoClipDue>("intoClipDue")
	    .Field<&components::Villager::woken>("woken")
	    .Field<&components::Villager::foodHeld>("foodHeld")
	    .Field<&components::Villager::woodHeld>("woodHeld")
	    .Field<&components::Villager::woodGraphic>("woodGraphic")
	    .Field<&components::Villager::buildingSite>("buildingSite")
	    .Field<&components::Villager::buildPlace>("buildPlace")
	    .Field<&components::Villager::foodSpeedUp>("foodSpeedUp")
	    .Field<&components::Villager::creationIndex>("creationIndex")
	    .Field<&components::Villager::pregnancy>("pregnancy")
	    .Field<&components::Villager::mother>("mother")
	    .Field<&components::Villager::crechePath>("crechePath")
	    .Field<&components::Villager::crecheLeg>("crecheLeg")
	    .Field<&components::Villager::kind>("kind");
	Reflect<components::VillagerCreatureInspection>(context)
	    .Field<&components::VillagerCreatureInspection::keep>("keep")
	    .Field<&components::VillagerCreatureInspection::lookTurns>("lookTurns");
	Reflect<components::VillagerDeath>(context)
	    .Field<&components::VillagerDeath::turnsLeft>("turnsLeft")
	    .Field<&components::VillagerDeath::skeleton>("skeleton")
	    .Field<&components::VillagerDeath::reason>("reason")
	    .Field<&components::VillagerDeath::killer>("killer")
	    .Field<&components::VillagerDeath::onBack>("onBack")
	    .Field<&components::VillagerDeath::noticed>("noticed");
	Reflect<components::VillagerHiding>(context)
	    .Field<&components::VillagerHiding::building>("building")
	    .Field<&components::VillagerHiding::turns>("turns")
	    .Field<&components::VillagerHiding::threat>("threat")
	    .Field<&components::VillagerHiding::threatKind>("threatKind");
	Reflect<components::VillagerPanic>(context).Field<&components::VillagerPanic::point>("point");
	Reflect<components::NamedVillager>(context).Field<&components::NamedVillager::row>("row");
	Reflect<components::DetailsShown>(context).Field<&components::DetailsShown::dummy>("dummy");
	Reflect<components::NameHidden>(context).Field<&components::NameHidden::dummy>("dummy");
	Reflect<components::VillagerPose>(context)
	    .Field<&components::VillagerPose::clip>("clip")
	    .Field<&components::VillagerPose::place>("place")
	    .Field<&components::VillagerPose::groundCarry>("groundCarry")
	    .Field<&components::VillagerPose::bones>("bones")
	    .Field<&components::VillagerPose::clipBones>("clipBones")
	    .Field<&components::VillagerPose::turnStart>("turnStart")
	    .Field<&components::VillagerPose::movedLastTurn>("movedLastTurn")
	    .Field<&components::VillagerPose::drawnAt>("drawnAt")
	    .Field<&components::VillagerPose::drawnHeading>("drawnHeading")
	    .Field<&components::VillagerPose::easedHeading>("easedHeading")
	    .Field<&components::VillagerPose::detailedHeading>("detailedHeading")
	    .Field<&components::VillagerPose::slopeLift>("slopeLift")
	    .Field<&components::VillagerPose::clipBlend>("clipBlend");
	Reflect<components::WorkTarget>(context).Field<&components::WorkTarget::thing>("thing");
	Reflect<components::FieldWorkSpot>(context).Field<&components::FieldWorkSpot::position>("position");
	Reflect<components::WoodReactionTarget>(context)
	    .Field<&components::WoodReactionTarget::wood>("wood")
	    .Field<&components::WoodReactionTarget::startTurn>("startTurn");
	Reflect<components::ShepherdWork>(context)
	    .Field<&components::ShepherdWork::countdown>("countdown")
	    .Field<&components::ShepherdWork::pending>("pending");
	Reflect<components::MoveTarget>(context).Field<&components::MoveTarget::thing>("thing");
	Reflect<components::Vortex>(context)
	    .Field<&components::Vortex::type>("type")
	    .Field<&components::Vortex::state>("state")
	    .Field<&components::Vortex::stateStartTurn>("stateStartTurn")
	    .Field<&components::Vortex::centre>("centre")
	    .Field<&components::Vortex::altitude>("altitude")
	    .Field<&components::Vortex::levelApplied>("levelApplied")
	    .Field<&components::Vortex::groundHeights>("groundHeights")
	    .Field<&components::Vortex::groundAverage>("groundAverage")
	    .Field<&components::Vortex::beforeLandEffect>("beforeLandEffect")
	    .Field<&components::Vortex::afterLandEffect>("afterLandEffect")
	    .Field<&components::Vortex::objectMoverEffect>("objectMoverEffect")
	    .Field<&components::Vortex::lightMapEffect>("lightMapEffect")
	    .Field<&components::Vortex::arrival>("arrival")
	    .Field<&components::Vortex::throwingOut>("throwingOut");
	Reflect<components::DrawnIntoVortex>(context)
	    .Field<&components::DrawnIntoVortex::vortex>("vortex")
	    .Field<&components::DrawnIntoVortex::flight>("flight");
}
