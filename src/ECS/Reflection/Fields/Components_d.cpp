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

#include "ECS/Components/Dance.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/DrawImportance.h"
#include "ECS/Components/DrinkWaypoint.h"
#include "ECS/Components/DrinkingWater.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_d(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_d(entt::meta_ctx& context)
{
	Reflect<components::DanceMove>(context)
	    .Field<&components::DanceMove::action>("action")
	    .Field<&components::DanceMove::first>("first")
	    .Field<&components::DanceMove::second>("second")
	    .Field<&components::DanceMove::end>("end");
	Reflect<components::DanceGroup>(context)
	    .Field<&components::DanceGroup::name>("name")
	    .Field<&components::DanceGroup::dancers>("dancers")
	    .Field<&components::DanceGroup::limited>("limited")
	    .Field<&components::DanceGroup::quota>("quota")
	    .Field<&components::DanceGroup::weight>("weight")
	    .Field<&components::DanceGroup::limitedDancers>("limitedDancers")
	    .Field<&components::DanceGroup::danceType>("danceType")
	    .Field<&components::DanceGroup::sexes>("sexes")
	    .Field<&components::DanceGroup::formation>("formation")
	    .Field<&components::DanceGroup::inFormation>("inFormation")
	    .Field<&components::DanceGroup::flag>("flag")
	    .Field<&components::DanceGroup::shape>("shape")
	    .Field<&components::DanceGroup::radius>("radius")
	    .Field<&components::DanceGroup::offset>("offset")
	    .Field<&components::DanceGroup::rotation>("rotation")
	    .Field<&components::DanceGroup::spin>("spin")
	    .Field<&components::DanceGroup::move>("move")
	    .Field<&components::DanceGroup::spinRate>("spinRate")
	    .Field<&components::DanceGroup::rotationRate>("rotationRate")
	    .Field<&components::DanceGroup::radiusRate>("radiusRate")
	    .Field<&components::DanceGroup::offsetRate>("offsetRate")
	    .Field<&components::DanceGroup::moved>("moved")
	    .Field<&components::DanceGroup::parent>("parent")
	    .Field<&components::DanceGroup::indexInParent>("indexInParent")
	    .Field<&components::DanceGroup::children>("children");
	Reflect<components::DanceGroups>(context)
	    .Field<&components::DanceGroups::all>("all")
	    .Field<&components::DanceGroups::limited>("limited")
	    .Field<&components::DanceGroups::shared>("shared")
	    .Field<&components::DanceGroups::round>("round")
	    .Field<&components::DanceGroups::roundLength>("roundLength");
	Reflect<components::Dance>(context)
	    .Field<&components::Dance::type>("type")
	    .Field<&components::Dance::place>("place")
	    .Field<&components::Dance::angle>("angle")
	    .Field<&components::Dance::owner>("owner")
	    .Field<&components::Dance::state>("state")
	    .Field<&components::Dance::autostart>("autostart")
	    .Field<&components::Dance::madeByScript>("madeByScript")
	    .Field<&components::Dance::speed>("speed")
	    .Field<&components::Dance::rate>("rate")
	    .Field<&components::Dance::dancingRate>("dancingRate")
	    .Field<&components::Dance::clock>("clock")
	    .Field<&components::Dance::loopLength>("loopLength")
	    .Field<&components::Dance::duration>("duration")
	    .Field<&components::Dance::startTurn>("startTurn")
	    .Field<&components::Dance::dancers>("dancers")
	    .Field<&components::Dance::onTheirWay>("onTheirWay")
	    .Field<&components::Dance::firstDancerTurn>("firstDancerTurn")
	    .Field<&components::Dance::handNearTurns>("handNearTurns")
	    .Field<&components::Dance::cameraWanted>("cameraWanted")
	    .Field<&components::Dance::cameraMade>("cameraMade")
	    .Field<&components::Dance::cameraAsked>("cameraAsked")
	    .Field<&components::Dance::groups>("groups")
	    .Field<&components::Dance::file>("file");
	Reflect<components::Dancer>(context).Field<&components::Dancer::dance>("dance").Field<&components::Dancer::group>("group");
	Reflect<components::DeadTree>(context)
	    .Field<&components::DeadTree::type>("type")
	    .Field<&components::DeadTree::woodMultiplier>("woodMultiplier")
	    .Field<&components::DeadTree::felled>("felled")
	    .Field<&components::DeadTree::model>("model");
	Reflect<components::DestructionGhost>(context)
	    .Field<&components::DestructionGhost::mesh>("mesh")
	    .Field<&components::DestructionGhost::model>("model")
	    .Field<&components::DestructionGhost::millisecondsLeft>("millisecondsLeft")
	    .Field<&components::DestructionGhost::shown>("shown");
	Reflect<components::DetailMeshes>(context)
	    .Field<&components::DetailMeshes::meshes>("meshes")
	    .Field<&components::DetailMeshes::importance>("importance");
	Reflect<components::Disciple>(context)
	    .Field<&components::Disciple::type>("type")
	    .Field<&components::Disciple::tradeTown>("tradeTown")
	    .Field<&components::Disciple::tradeResource>("tradeResource");
	Reflect<components::DiscipleFollower>(context).Field<&components::DiscipleFollower::type>("type");
	Reflect<components::MissionaryControl>(context)
	    .Field<&components::MissionaryControl::villager>("villager")
	    .Field<&components::MissionaryControl::town>("town")
	    .Field<&components::MissionaryControl::player>("player")
	    .Field<&components::MissionaryControl::reaction>("reaction");
	Reflect<components::DiscipleHeld>(context)
	    .Field<&components::DiscipleHeld::atPickUp>("atPickUp")
	    .Field<&components::DiscipleHeld::tradeTown>("tradeTown")
	    .Field<&components::DiscipleHeld::touchedDown>("touchedDown");
	Reflect<components::DiscipleInspect>(context).Field<&components::DiscipleInspect::thing>("thing");
	Reflect<components::DiscipleMark>(context)
	    .Field<&components::DiscipleMark::villager>("villager")
	    .Field<&components::DiscipleMark::symbol>("symbol");
	Reflect<components::DrawImportance>(context).Field<&components::DrawImportance::importance>("importance");
	Reflect<components::DrinkWaypoint>(context).Field<&components::DrinkWaypoint::number>("number");
	Reflect<components::DrinkingWater>(context).Field<&components::DrinkingWater::place>("place");
}
