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

#include "Creature/CreatureDesires.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace components = openblack::ecs::components;

namespace openblack::ecs::reflection
{
void RegisterFields_Components_creaturem(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 40> k_CreatureDesiresDesireNames {"Impress",
                                                                         "Compassion",
                                                                         "Anger",
                                                                         "Play",
                                                                         "Hunger",
                                                                         "Fear",
                                                                         "Curiosity",
                                                                         "Poo",
                                                                         "Tiredness",
                                                                         "IdleWithPlayer",
                                                                         "Wanderlust",
                                                                         "Puke",
                                                                         "BuildHome",
                                                                         "BringStuffHome",
                                                                         "Water",
                                                                         "RestoreHealth",
                                                                         "BeFriends",
                                                                         "AttractAttention",
                                                                         "ManifestState",
                                                                         "GetWarmer",
                                                                         "GetColder",
                                                                         "Scratch",
                                                                         "RunAwayFromPlayer",
                                                                         "Rest",
                                                                         "ObeyPlayer",
                                                                         "Illness",
                                                                         "ObeyCreature",
                                                                         "Sadness",
                                                                         "StayNearHome",
                                                                         "TellPlayer",
                                                                         "PlayWithPlayer",
                                                                         "TellCreature",
                                                                         "EducateFriend",
                                                                         "FollowPlayerDesire",
                                                                         "GetHigh",
                                                                         "HangAroundAtHome",
                                                                         "MentalIllness",
                                                                         "MissFriend",
                                                                         "LookAround",
                                                                         "Steal"};
static_assert(static_cast<size_t>(openblack::creature_desires::Desire::_Count) == k_CreatureDesiresDesireNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Components_creaturem(entt::meta_ctx& context)
{
	Reflect<components::CreatureMindState>(context)
	    .Field<&components::CreatureMindState::desires>("desires")
	    .Field<&components::CreatureMindState::idle>("idle")
	    .Field<&components::CreatureMindState::scriptPlay>("scriptPlay")
	    .Field<&components::CreatureMindState::look>("look")
	    .Field<&components::CreatureMindState::goingLook>("goingLook")
	    .Field<&components::CreatureMindState::noticedMover>("noticedMover")
	    .Field<&components::CreatureMindState::lookingAbout>("lookingAbout")
	    .Field<&components::CreatureMindState::scriptFocus>("scriptFocus")
	    .Field<&components::CreatureMindState::innate>("innate")
	    .Field<&components::CreatureMindState::attention>("attention")
	    .Field<&components::CreatureMindState::playerInterest>("playerInterest")
	    .Field<&components::CreatureMindState::impressFromSeeing>("impressFromSeeing")
	    .Field<&components::CreatureMindState::turnsLookedAt>("turnsLookedAt")
	    .Field<&components::CreatureMindState::beliefs>("beliefs")
	    .Field<&components::CreatureMindState::sweep>("sweep")
	    .Field<&components::CreatureMindState::lastTown>("lastTown")
	    .Field<&components::CreatureMindState::nearestTemple>("nearestTemple")
	    .Field<&components::CreatureMindState::feedbackSeconds>("feedbackSeconds")
	    .Field<&components::CreatureMindState::feedbackWasStroke>("feedbackWasStroke")
	    .Field<&components::CreatureMindState::attitudeToPlayer>("attitudeToPlayer")
	    .Field<&components::CreatureMindState::perceivedDesires>("perceivedDesires")
	    .Field<&components::CreatureMindState::lastFeedback>("lastFeedback")
	    .Field<&components::CreatureMindState::interactionMagnitude>("interactionMagnitude")
	    .Field<&components::CreatureMindState::actionCounts>("actionCounts")
	    .Field<&components::CreatureMindState::underway>("underway")
	    .Field<&components::CreatureMindState::lastActionDone>("lastActionDone")
	    .Field<&components::CreatureMindState::developmentPhase>("developmentPhase")
	    .Field<&components::CreatureMindState::desiresPhase>("desiresPhase")
	    .Field<&components::CreatureMindState::inDevelopmentScript>("inDevelopmentScript")
	    .Field<&components::CreatureMindState::paused>("paused")
	    .Field<&components::CreatureMindState::mayAttackOwnPlayer>("mayAttackOwnPlayer")
	    .Field<&components::CreatureMindState::showDesireOnHold>("showDesireOnHold")
	    .Field<&components::CreatureMindState::heldSinceRestart>("heldSinceRestart")
	    .Field<&components::CreatureMindState::releaseFeedback>("releaseFeedback")
	    .Field<&components::CreatureMindState::releaseFeedbackTurns>("releaseFeedbackTurns")
	    .Field<&components::CreatureMindState::ownPlanSinceStop>("ownPlanSinceStop")
	    .Field<&components::CreatureMindState::turnsWithoutOwnPlan>("turnsWithoutOwnPlan")
	    .Field<&components::CreatureMindState::planForced>("planForced")
	    .Field<&components::CreatureMindState::forcedPlanCounts>("forcedPlanCounts")
	    .Field<&components::CreatureMindState::ledByLeash>("ledByLeash")
	    .Field<&components::CreatureMindState::emergencyRunning>("emergencyRunning")
	    .Field<&components::CreatureMindState::learnt>("learnt")
	    .Field<&components::CreatureMindState::planner>("planner")
	    .Field<&components::CreatureMindState::townCompassion>("townCompassion")
	    .Field<&components::CreatureMindState::planActive>("planActive")
	    .Field<&components::CreatureMindState::satisfiedByEffect>("satisfiedByEffect")
	    .Field<&components::CreatureMindState::desireSeenTo>("desireSeenTo")
	    .Field<&components::CreatureMindState::observedKinds>("observedKinds")
	    .Field<&components::CreatureMindState::keptPlan>("keptPlan")
	    .Field<&components::CreatureMindState::refusedFoodSorts>("refusedFoodSorts")
	    .Field<&components::CreatureMindState::takenFood>("takenFood")
	    .Field<&components::CreatureMindState::takenFrom>("takenFrom")
	    .Field<&components::CreatureMindState::planSerial>("planSerial")
	    .Field<&components::CreatureMindState::agendaSeen>("agendaSeen")
	    .Field<&components::CreatureMindState::stepTurns>("stepTurns")
	    .Field<&components::CreatureMindState::agendaTurns>("agendaTurns")
	    .Field<&components::CreatureMindState::agendaTurnsSerial>("agendaTurnsSerial")
	    .Field<&components::CreatureMindState::leftAreaActions>("leftAreaActions")
	    .Field<&components::CreatureMindState::agendaRefused>("agendaRefused")
	    .Field<&components::CreatureMindState::paceSerial>("paceSerial")
	    .Field<&components::CreatureMindState::planActions>("planActions")
	    .Field<&components::CreatureMindState::plannedActions>("plannedActions", {k_CreatureDesiresDesireNames})
	    .Field<&components::CreatureMindState::lastDoneTurn>("lastDoneTurn")
	    .Field<&components::CreatureMindState::runLookBack>("runLookBack")
	    .Field<&components::CreatureMindState::turn>("turn")
	    .Field<&components::CreatureMindState::plannedTurn>("plannedTurn")
	    .Field<&components::CreatureMindState::lastMeterTurn>("lastMeterTurn")
	    .Field<&components::CreatureMindState::creditedAction>("creditedAction")
	    .Field<&components::CreatureMindState::creditedSerial>("creditedSerial")
	    .Field<&components::CreatureMindState::thinksWithoutPlan>("thinksWithoutPlan")
	    .Field<&components::CreatureMindState::thinksWithoutPlanSerial>("thinksWithoutPlanSerial")
	    .Field<&components::CreatureMindState::trainer>("trainer")
	    .Field<&components::CreatureMindState::pendingFile>("pendingFile")
	    .Field<&components::CreatureMindState::resourceChecked>("resourceChecked")
	    .Field<&components::CreatureMindState::pendingTeaching>("pendingTeaching")
	    .Field<&components::CreatureMindState::leash>("leash")
	    .Field<&components::CreatureMindState::heldSortCounts>("heldSortCounts")
	    .Field<&components::CreatureMindState::copiedAt>("copiedAt")
	    .Field<&components::CreatureMindState::copiedKind>("copiedKind")
	    .Field<&components::CreatureMindState::followUp>("followUp")
	    .Field<&components::CreatureMindState::followUpValue>("followUpValue")
	    .Field<&components::CreatureMindState::plantedBy>("plantedBy")
	    .Field<&components::CreatureMindState::observedDeed>("observedDeed")
	    .Field<&components::CreatureMindState::clearingWaySerial>("clearingWaySerial")
	    .Field<&components::CreatureMindState::walkOnTo>("walkOnTo")
	    .Field<&components::CreatureMindState::leashCast>("leashCast")
	    .Field<&components::CreatureMindState::feedSiteFlag>("feedSiteFlag");
}
