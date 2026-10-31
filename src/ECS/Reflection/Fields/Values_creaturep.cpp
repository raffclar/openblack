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
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creaturep(entt::meta_ctx& context);
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

void openblack::ecs::reflection::RegisterFields_Values_creaturep(entt::meta_ctx& context)
{
	Reflect<openblack::creature_perceived_desires::PerceivedDesires>(context, ValueOnly {})
	    .Field<&openblack::creature_perceived_desires::PerceivedDesires::player>("player")
	    .Field<&openblack::creature_perceived_desires::PerceivedDesires::town>("town");
	Reflect<openblack::creature_physiology::Kept>(context, ValueOnly {})
	    .Field<&openblack::creature_physiology::Kept::age>("age")
	    .Field<&openblack::creature_physiology::Kept::turns>("turns")
	    .Field<&openblack::creature_physiology::Kept::energy>("energy")
	    .Field<&openblack::creature_physiology::Kept::exhaustion>("exhaustion");
	Reflect<openblack::creature_physiology::Needs>(context, ValueOnly {})
	    .Field<&openblack::creature_physiology::Needs::age>("age")
	    .Field<&openblack::creature_physiology::Needs::turns>("turns")
	    .Field<&openblack::creature_physiology::Needs::warmth>("warmth")
	    .Field<&openblack::creature_physiology::Needs::energy>("energy")
	    .Field<&openblack::creature_physiology::Needs::itchiness>("itchiness")
	    .Field<&openblack::creature_physiology::Needs::poo>("poo")
	    .Field<&openblack::creature_physiology::Needs::exhaustion>("exhaustion")
	    .Field<&openblack::creature_physiology::Needs::dehydration>("dehydration")
	    .Field<&openblack::creature_physiology::Needs::life>("life")
	    .Field<&openblack::creature_physiology::Needs::meals>("meals");
	Reflect<openblack::creature_planner::Plan>(context, ValueOnly {})
	    .Field<&openblack::creature_planner::Plan::desire>("desire")
	    .Field<&openblack::creature_planner::Plan::action>("action")
	    .Field<&openblack::creature_planner::Plan::object>("object")
	    .Field<&openblack::creature_planner::Plan::about>("about")
	    .Field<&openblack::creature_planner::Plan::goalUsefulness>("goalUsefulness")
	    .Field<&openblack::creature_planner::Plan::actionPriority>("actionPriority")
	    .Field<&openblack::creature_planner::Plan::priority>("priority")
	    .Field<&openblack::creature_planner::Plan::activityObject>("activityObject")
	    .Field<&openblack::creature_planner::Plan::instrument>("instrument");
	Reflect<openblack::creature_planner::PlannerState>(context, ValueOnly {})
	    .Field<&openblack::creature_planner::PlannerState::best>("best", {k_CreatureDesiresDesireNames})
	    .Field<&openblack::creature_planner::PlannerState::nextGoal>("nextGoal")
	    .Field<&openblack::creature_planner::PlannerState::current>("current")
	    .Field<&openblack::creature_planner::PlannerState::lastObject>("lastObject")
	    .Field<&openblack::creature_planner::PlannerState::lastAction>("lastAction")
	    .Field<&openblack::creature_planner::PlannerState::currentHasNoSteps>("currentHasNoSteps");
}
