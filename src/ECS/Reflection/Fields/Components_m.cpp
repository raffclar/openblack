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

#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicForest.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/MagicTreeWatch.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/MapPlace.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_m(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_m(entt::meta_ctx& context)
{
	Reflect<components::MagicFireBall>(context)
	    .Field<&components::MagicFireBall::strength>("strength")
	    .Field<&components::MagicFireBall::radius>("radius")
	    .Field<&components::MagicFireBall::infoRow>("infoRow")
	    .Field<&components::MagicFireBall::affectedByRain>("affectedByRain")
	    .Field<&components::MagicFireBall::player>("player")
	    .Field<&components::MagicFireBall::hasPlayer>("hasPlayer")
	    .Field<&components::MagicFireBall::lastTurn>("lastTurn")
	    .Field<&components::MagicFireBall::handTarget>("handTarget");
	Reflect<components::MagicForest>(context)
	    .Field<&components::MagicForest::spell>("spell")
	    .Field<&components::MagicForest::player>("player")
	    .Field<&components::MagicForest::centre>("centre")
	    .Field<&components::MagicForest::trees>("trees");
	Reflect<components::MagicTree>(context)
	    .Field<&components::MagicTree::forest>("forest")
	    .Field<&components::MagicTree::woodMultiplier>("woodMultiplier")
	    .Field<&components::MagicTree::impressiveValue>("impressiveValue");
	Reflect<components::MagicPile>(context)
	    .Field<&components::MagicPile::resource>("resource")
	    .Field<&components::MagicPile::player>("player");
	Reflect<components::MagicShield>(context)
	    .Field<&components::MagicShield::kind>("kind")
	    .Field<&components::MagicShield::spell>("spell")
	    .Field<&components::MagicShield::player>("player")
	    .Field<&components::MagicShield::radius>("radius")
	    .Field<&components::MagicShield::town>("town")
	    .Field<&components::MagicShield::reaction>("reaction")
	    .Field<&components::MagicShield::struckReaction>("struckReaction");
	Reflect<components::ShieldDome>(context)
	    .Field<&components::ShieldDome::shape>("shape")
	    .Field<&components::ShieldDome::state>("state")
	    .Field<&components::ShieldDome::pose>("pose")
	    .Field<&components::ShieldDome::previous>("previous")
	    .Field<&components::ShieldDome::age>("age")
	    .Field<&components::ShieldDome::solidScale>("solidScale")
	    .Field<&components::ShieldDome::alpha>("alpha")
	    .Field<&components::ShieldDome::mesh>("mesh")
	    .Field<&components::ShieldDome::halfExtent>("halfExtent")
	    .Field<&components::ShieldDome::effect>("effect")
	    .Field<&components::ShieldDome::hull>("hull")
	    .Field<&components::ShieldDome::hullOrigin>("hullOrigin")
	    .Field<&components::ShieldDome::hullScale>("hullScale");
	Reflect<components::AntiInfluence>(context)
	    .Field<&components::AntiInfluence::owner>("owner")
	    .Field<&components::AntiInfluence::radius>("radius");
	Reflect<components::VillagerShieldReaction>(context)
	    .Field<&components::VillagerShieldReaction::reaction>("reaction")
	    .Field<&components::VillagerShieldReaction::type>("type")
	    .Field<&components::VillagerShieldReaction::shield>("shield")
	    .Field<&components::VillagerShieldReaction::lookAt>("lookAt");
	Reflect<components::VillagerReactionMemory>(context).Field<&components::VillagerReactionMemory::memory>("memory");
	Reflect<components::MagicTreeWatch>(context)
	    .Field<&components::MagicTreeWatch::tree>("tree")
	    .Field<&components::MagicTreeWatch::next>("next")
	    .Field<&components::MagicTreeWatch::looked>("looked");
	Reflect<components::MapCellResident>(context)
	    .Field<&components::MapCellResident::placement>("placement")
	    .Field<&components::MapCellResident::coversOutline>("coversOutline")
	    .Field<&components::MapCellResident::moves>("moves")
	    .Field<&components::MapCellResident::cells>("cells")
	    .Field<&components::MapCellResident::filedAt>("filedAt");
	Reflect<components::MapCellMover> {context};
	Reflect<components::MapPlace>(context).Field<&components::MapPlace::place>("place").Field<&components::MapPlace::placedAt>(
	    "placedAt");
	Reflect<components::Mesh>(context)
	    .Field<&components::Mesh::id>("id")
	    .Field<&components::Mesh::submeshId>("submeshId")
	    .Field<&components::Mesh::bbSubmeshId>("bbSubmeshId");
	Reflect<components::TownImpression>(context)
	    .Field<&components::TownImpression::belief>("belief")
	    .Field<&components::TownImpression::boredom>("boredom")
	    .Field<&components::TownImpression::lastImpression>("lastImpression");
	Reflect<components::CreatureImpression>(context)
	    .Field<&components::CreatureImpression::byOwnPlayer>("byOwnPlayer")
	    .Field<&components::CreatureImpression::byOtherCreatures>("byOtherCreatures");
	Reflect<components::Mist>(context)
	    .Field<&components::Mist::size>("size")
	    .Field<&components::Mist::colour>("colour")
	    .Field<&components::Mist::shrinksEdgeOn>("shrinksEdgeOn")
	    .Field<&components::Mist::edgeShrink>("edgeShrink")
	    .Field<&components::Mist::counter>("counter")
	    .Field<&components::Mist::counterRemainder>("counterRemainder");
	Reflect<components::MistDome>(context)
	    .Field<&components::MistDome::dome>("dome")
	    .Field<&components::MistDome::uvOffset>("uvOffset")
	    .Field<&components::MistDome::colour>("colour");
	Reflect<components::Mobile>(context).Field<&components::Mobile::dummy>("dummy");
	Reflect<components::MobileStatic>(context).Field<&components::MobileStatic::type>("type");
	Reflect<components::MobileObject>(context).Field<&components::MobileObject::type>("type");
	Reflect<components::PlacedHeight>(context)
	    .Field<&components::PlacedHeight::height>("height")
	    .Field<&components::PlacedHeight::position>("position");
	Reflect<components::Rock> {context};
	Reflect<components::MorphWithTerrain>(context).Field<&components::MorphWithTerrain::dummy>("dummy");
}
