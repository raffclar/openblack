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

#include "ECS/Components/LandForest.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/LeashOrderSparkles.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/LightBeam.h"
#include "ECS/Components/ListPlace.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/LivingPhysics.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_l(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 3> k_EcsComponentsLivingActionIndexNames {"Top", "Final", "Previous"};
static_assert(static_cast<size_t>(openblack::ecs::components::LivingAction::Index::_Count) ==
              k_EcsComponentsLivingActionIndexNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Components_l(entt::meta_ctx& context)
{
	Reflect<components::LandForest>(context)
	    .Field<&components::LandForest::id>("id")
	    .Field<&components::LandForest::bigForest>("bigForest")
	    .Field<&components::LandForest::scenic>("scenic")
	    .Field<&components::LandForest::made>("made");
	Reflect<components::ForestTurns>(context)
	    .Field<&components::ForestTurns::id>("id")
	    .Field<&components::ForestTurns::made>("made")
	    .Field<&components::ForestTurns::emptyCountdown>("emptyCountdown")
	    .Field<&components::ForestTurns::spreadCounter>("spreadCounter");
	Reflect<components::TownForests>(context).Field<&components::TownForests::forests>("forests");
	Reflect<components::LearningMarker>(context)
	    .Field<&components::LearningMarker::text>("text")
	    .Field<&components::LearningMarker::position>("position")
	    .Field<&components::LearningMarker::size>("size")
	    .Field<&components::LearningMarker::milliseconds>("milliseconds")
	    .Field<&components::LearningMarker::madeTurn>("madeTurn");
	Reflect<components::LeashOrderSparkles>(context).Field<&components::LeashOrderSparkles::effect>("effect");
	Reflect<components::LeftGame>(context).Field<&components::LeftGame::turn>("turn");
	Reflect<components::LightBeam>(context).Field<&components::LightBeam::cone>("cone");
	Reflect<components::ListPlace>(context).Field<&components::ListPlace::made>("made");
	Reflect<components::LivingAction>(context)
	    .Field<&components::LivingAction::states>("states", {k_EcsComponentsLivingActionIndexNames})
	    .Field<&components::LivingAction::turnsUntilStateChange>("turnsUntilStateChange")
	    .Field<&components::LivingAction::turnsSinceStateChange>("turnsSinceStateChange");
	Reflect<components::LivingListPlace>(context).Field<&components::LivingListPlace::made>("made");
	Reflect<components::VillagerClip>(context)
	    .Field<&components::VillagerClip::state>("state")
	    .Field<&components::VillagerClip::clip>("clip");
	Reflect<components::LastInteractingPlayer>(context).Field<&components::LastInteractingPlayer::player>("player");
	Reflect<components::WatchedFlyingObject>(context).Field<&components::WatchedFlyingObject::object>("object");
	Reflect<components::LivingReaction>(context)
	    .Field<&components::LivingReaction::reaction>("reaction")
	    .Field<&components::LivingReaction::type>("type")
	    .Field<&components::LivingReaction::startTurn>("startTurn")
	    .Field<&components::LivingReaction::joined>("joined")
	    .Field<&components::LivingReaction::creatureMemory>("creatureMemory")
	    .Field<&components::LivingReaction::sawFlying>("sawFlying")
	    .Field<&components::LivingReaction::sawCrushed>("sawCrushed");
	Reflect<components::CreatureReactionsOff> {context};
}
