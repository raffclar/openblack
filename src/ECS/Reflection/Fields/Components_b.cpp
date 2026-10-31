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

#include "ECS/Components/Ball.h"
#include "ECS/Components/BlendedByMaterial.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/Breeding.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_b(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_b(entt::meta_ctx& context)
{
	Reflect<components::Ball>(context).Field<&components::Ball::destination>("destination");
	Reflect<components::BlendedByMaterial>(context).Field<&components::BlendedByMaterial::shineColour>("shineColour");
	Reflect<components::BoatFilmPart>(context)
	    .Field<&components::BoatFilmPart::role>("role")
	    .Field<&components::BoatFilmPart::index>("index")
	    .Field<&components::BoatFilmPart::clipTime>("clipTime")
	    .Field<&components::BoatFilmPart::castsShadow>("castsShadow");
	Reflect<components::DrawnMatrix>(context).Field<&components::DrawnMatrix::model>("model");
	Reflect<components::Lover>(context).Field<&components::Lover::partner>("partner");
	Reflect<components::BreederSpot>(context).Field<&components::BreederSpot::spot>("spot");
	Reflect<components::BuildingDamage>(context)
	    .Field<&components::BuildingDamage::mesh>("mesh")
	    .Field<&components::BuildingDamage::lastHitter>("lastHitter")
	    .Field<&components::BuildingDamage::drawMesh>("drawMesh")
	    .Field<&components::BuildingDamage::version>("version");
	Reflect<components::BrokenModels>(context).Field<&components::BrokenModels::made>("made");
	Reflect<components::BuildingPiece>(context)
	    .Field<&components::BuildingPiece::mesh>("mesh")
	    .Field<&components::BuildingPiece::parent>("parent")
	    .Field<&components::BuildingPiece::sourceMesh>("sourceMesh")
	    .Field<&components::BuildingPiece::turnsLeft>("turnsLeft")
	    .Field<&components::BuildingPiece::drawMesh>("drawMesh");
}
