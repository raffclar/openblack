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

#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturel(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Components_creaturel(entt::meta_ctx& context)
{
	Reflect<components::CreatureLeash>(context)
	    .Field<&components::CreatureLeash::known>("known")
	    .Field<&components::CreatureLeash::worn>("worn")
	    .Field<&components::CreatureLeash::leashWorks>("leashWorks")
	    .Field<&components::CreatureLeash::selected>("selected")
	    .Field<&components::CreatureLeash::drawn>("drawn")
	    .Field<&components::CreatureLeash::confinementCentre>("confinementCentre")
	    .Field<&components::CreatureLeash::confinementRadius>("confinementRadius")
	    .Field<&components::CreatureLeash::home>("home")
	    .Field<&components::CreatureLeash::homeOnMap>("homeOnMap")
	    .Field<&components::CreatureLeash::returnTo>("returnTo")
	    .Field<&components::CreatureLeash::returnSerial>("returnSerial")
	    .Field<&components::CreatureLeash::order>("order")
	    .Field<&components::CreatureLeash::help>("help")
	    .Field<&components::CreatureLeash::helpDesire>("helpDesire");
	Reflect<components::LeashMarker>(context).Field<&components::LeashMarker::creature>("creature");
	Reflect<components::LeashPost>(context)
	    .Field<&components::LeashPost::type>("type")
	    .Field<&components::LeashPost::owner>("owner")
	    .Field<&components::LeashPost::selected>("selected")
	    .Field<&components::LeashPost::point>("point")
	    .Field<&components::LeashPost::look>("look")
	    .Field<&components::LeashPost::hung>("hung")
	    .Field<&components::LeashPost::glow>("glow");
	Reflect<components::CreatureLocomotion>(context)
	    .Field<&components::CreatureLocomotion::motion>("motion")
	    .Field<&components::CreatureLocomotion::started>("started")
	    .Field<&components::CreatureLocomotion::heading>("heading")
	    .Field<&components::CreatureLocomotion::targetHeading>("targetHeading")
	    .Field<&components::CreatureLocomotion::speed>("speed")
	    .Field<&components::CreatureLocomotion::fraction>("fraction")
	    .Field<&components::CreatureLocomotion::speeds>("speeds")
	    .Field<&components::CreatureLocomotion::scale>("scale")
	    .Field<&components::CreatureLocomotion::radius>("radius")
	    .Field<&components::CreatureLocomotion::shownRadius>("shownRadius")
	    .Field<&components::CreatureLocomotion::destination>("destination")
	    .Field<&components::CreatureLocomotion::ring>("ring")
	    .Field<&components::CreatureLocomotion::ontoDestination>("ontoDestination")
	    .Field<&components::CreatureLocomotion::follower>("follower")
	    .Field<&components::CreatureLocomotion::routeWalker>("routeWalker")
	    .Field<&components::CreatureLocomotion::firstSearchTurn>("firstSearchTurn")
	    .Field<&components::CreatureLocomotion::routeReady>("routeReady")
	    .Field<&components::CreatureLocomotion::searchHeld>("searchHeld")
	    .Field<&components::CreatureLocomotion::failed>("failed")
	    .Field<&components::CreatureLocomotion::gaveUp>("gaveUp")
	    .Field<&components::CreatureLocomotion::clearingWay>("clearingWay")
	    .Field<&components::CreatureLocomotion::facingOnly>("facingOnly")
	    .Field<&components::CreatureLocomotion::following>("following")
	    .Field<&components::CreatureLocomotion::followDistance>("followDistance")
	    .Field<&components::CreatureLocomotion::move>("move")
	    .Field<&components::CreatureLocomotion::walkTimeMs>("walkTimeMs")
	    .Field<&components::CreatureLocomotion::distance>("distance")
	    .Field<&components::CreatureLocomotion::fidgetMs>("fidgetMs")
	    .Field<&components::CreatureLocomotion::fidgeted>("fidgeted")
	    .Field<&components::CreatureLocomotion::routeLook>("routeLook")
	    .Field<&components::CreatureLocomotion::routePoint>("routePoint")
	    .Field<&components::CreatureLocomotion::fromPosition>("fromPosition")
	    .Field<&components::CreatureLocomotion::toPosition>("toPosition")
	    .Field<&components::CreatureLocomotion::fromHeading>("fromHeading")
	    .Field<&components::CreatureLocomotion::toHeading>("toHeading")
	    .Field<&components::CreatureLocomotion::headingSnaps>("headingSnaps")
	    .Field<&components::CreatureLocomotion::tracks>("tracks");
}
