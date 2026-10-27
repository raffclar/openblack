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

#include "Creature/CreatureHair.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureHeadLook.h"
#include "Creature/CreatureHelp.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creatureh(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 42> k_CreatureHelpKindNames {"LessonIncreaseSource",
                                                                    "LessonDecreaseSource",
                                                                    "LessonIncreaseDesire",
                                                                    "LessonDecreaseDesire",
                                                                    "LessonIncreaseOpinion",
                                                                    "LessonDecreaseOpinion",
                                                                    "LessonNormalAction",
                                                                    "LessonMagicAction",
                                                                    "NearlyLearntNormalAction",
                                                                    "NearlyLearntMagicAction",
                                                                    "CantLearnNormalActionYet",
                                                                    "CantLearnMagicActionYet",
                                                                    "StageInitial",
                                                                    "StageLearnToEat",
                                                                    "StagePunishment",
                                                                    "StageLeashPullAndPickUp",
                                                                    "StageLeashAttachToHouse",
                                                                    "StageMeetGuide",
                                                                    "StageFriendsWithGuide",
                                                                    "StageGuideExplainsHistory",
                                                                    "StageLearnSpell",
                                                                    "StageLearnToImpressTown",
                                                                    "StageLearnToFight",
                                                                    "StageLearnToHelpTown",
                                                                    "StageLeashGoodAndEvil",
                                                                    "StageFullyMature",
                                                                    "ReceivedSpell",
                                                                    "FinishedReceivingSpell",
                                                                    "Tutorial",
                                                                    "CurrentSource",
                                                                    "PhysicalProblem",
                                                                    "MentalProblem",
                                                                    "PromptHasntLearntNormalAction",
                                                                    "TownAttitudeToCreature",
                                                                    "CannotLearnActionInThisStage",
                                                                    "ShowStageCriteria",
                                                                    "SomethingInterestingOffScreen",
                                                                    "MiscellaneousStacked",
                                                                    "MiscellaneousUnstacked",
                                                                    "MessedUpAction",
                                                                    "FailureExplanation",
                                                                    "CurrentDesire"};
static_assert(static_cast<size_t>(openblack::creature_help::Kind::_Count) == k_CreatureHelpKindNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Values_creatureh(entt::meta_ctx& context)
{
	Reflect<openblack::creature_hair::Strand>(context, ValueOnly {})
	    .Field<&openblack::creature_hair::Strand::positions>("positions")
	    .Field<&openblack::creature_hair::Strand::velocities>("velocities");
	Reflect<openblack::creature_hand_glow::Glow>(context, ValueOnly {})
	    .Field<&openblack::creature_hand_glow::Glow::power>("power")
	    .Field<&openblack::creature_hand_glow::Glow::scale>("scale");
	Reflect<openblack::creature_hand_glow::Motion>(context, ValueOnly {})
	    .Field<&openblack::creature_hand_glow::Motion::turn>("turn")
	    .Field<&openblack::creature_hand_glow::Motion::slide>("slide");
	Reflect<openblack::creature_head_look::RouteLook>(context, ValueOnly {})
	    .Field<&openblack::creature_head_look::RouteLook::countdownMs>("countdownMs")
	    .Field<&openblack::creature_head_look::RouteLook::point>("point");
	Reflect<openblack::creature_help::Entry>(context, ValueOnly {})
	    .Field<&openblack::creature_help::Entry::parameter>("parameter")
	    .Field<&openblack::creature_help::Entry::thing>("thing")
	    .Field<&openblack::creature_help::Entry::position>("position")
	    .Field<&openblack::creature_help::Entry::toldTurn>("toldTurn")
	    .Field<&openblack::creature_help::Entry::waiting>("waiting");
	Reflect<openblack::creature_help::Memory>(context, ValueOnly {})
	    .Field<&openblack::creature_help::Memory::entries>("entries", {k_CreatureHelpKindNames})
	    .Field<&openblack::creature_help::Memory::physicalProblemTurn>("physicalProblemTurn")
	    .Field<&openblack::creature_help::Memory::tutorialTurn>("tutorialTurn")
	    .Field<&openblack::creature_help::Memory::tutorialDone>("tutorialDone")
	    .Field<&openblack::creature_help::Memory::tutorialToldTurn>("tutorialToldTurn")
	    .Field<&openblack::creature_help::Memory::handOver>("handOver")
	    .Field<&openblack::creature_help::Memory::clearedAtStage>("clearedAtStage");
}
