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

#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreatureObstacles.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creatureo(entt::meta_ctx& context);
}

void openblack::ecs::reflection::RegisterFields_Values_creatureo(entt::meta_ctx& context)
{
	Reflect<openblack::creature_object_actions::ApproachStall>(context, ValueOnly {})
	    .Field<&openblack::creature_object_actions::ApproachStall::turns>("turns")
	    .Field<&openblack::creature_object_actions::ApproachStall::note>("note");
	Reflect<openblack::creature_object_beliefs::Belief>(context, ValueOnly {})
	    .Field<&openblack::creature_object_beliefs::Belief::thing>("thing")
	    .Field<&openblack::creature_object_beliefs::Belief::kind>("kind")
	    .Field<&openblack::creature_object_beliefs::Belief::place>("place")
	    .Field<&openblack::creature_object_beliefs::Belief::inspected>("inspected")
	    .Field<&openblack::creature_object_beliefs::Belief::partOf>("partOf")
	    .Field<&openblack::creature_object_beliefs::Belief::order>("order")
	    .Field<&openblack::creature_object_beliefs::Belief::blockedTurn>("blockedTurn")
	    .Field<&openblack::creature_object_beliefs::Belief::attributes>("attributes")
	    .Field<&openblack::creature_object_beliefs::Belief::sourcesTurn>("sourcesTurn");
	Reflect<openblack::creature_object_beliefs::BeliefPlace>(context, ValueOnly {})
	    .Field<&openblack::creature_object_beliefs::BeliefPlace::kind>("kind")
	    .Field<&openblack::creature_object_beliefs::BeliefPlace::slot>("slot");
	Reflect<openblack::creature_object_beliefs::BeliefPool>(context, ValueOnly {})
	    .Field<&openblack::creature_object_beliefs::BeliefPool::slots>("slots")
	    .Field<&openblack::creature_object_beliefs::BeliefPool::used>("used")
	    .Field<&openblack::creature_object_beliefs::BeliefPool::oldest>("oldest");
	Reflect<openblack::creature_object_beliefs::StoreContents>(context, ValueOnly {})
	    .Field<&openblack::creature_object_beliefs::StoreContents::pools>("pools")
	    .Field<&openblack::creature_object_beliefs::StoreContents::index>("index")
	    .Field<&openblack::creature_object_beliefs::StoreContents::made>("made");
	Reflect<openblack::creature_object_beliefs::Sweep>(context, ValueOnly {})
	    .Field<&openblack::creature_object_beliefs::Sweep::left>("left")
	    .Field<&openblack::creature_object_beliefs::Sweep::ring>("ring")
	    .Field<&openblack::creature_object_beliefs::Sweep::step>("step");
	Reflect<openblack::creature_obstacles::Walker>(context, ValueOnly {})
	    .Field<&openblack::creature_obstacles::Walker::position>("position")
	    .Field<&openblack::creature_obstacles::Walker::height>("height")
	    .Field<&openblack::creature_obstacles::Walker::radius>("radius")
	    .Field<&openblack::creature_obstacles::Walker::scriptControlled>("scriptControlled")
	    .Field<&openblack::creature_obstacles::Walker::owner>("owner")
	    .Field<&openblack::creature_obstacles::Walker::mayBeTakenByVortex>("mayBeTakenByVortex");
}
