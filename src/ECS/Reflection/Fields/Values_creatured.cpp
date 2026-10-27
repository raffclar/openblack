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

#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creatured(entt::meta_ctx& context);
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

void openblack::ecs::reflection::RegisterFields_Values_creatured(entt::meta_ctx& context)
{
	Reflect<openblack::creature_desire_sources::Attention>(context, ValueOnly {})
	    .Field<&openblack::creature_desire_sources::Attention::turnsWatched>("turnsWatched")
	    .Field<&openblack::creature_desire_sources::Attention::turnsIgnored>("turnsIgnored")
	    .Field<&openblack::creature_desire_sources::Attention::turnsSinceInteraction>("turnsSinceInteraction");
	Reflect<openblack::creature_desire_sources::InnateTraits>(context, ValueOnly {})
	    .Field<&openblack::creature_desire_sources::InnateTraits::kindness>("kindness")
	    .Field<&openblack::creature_desire_sources::InnateTraits::aggression>("aggression")
	    .Field<&openblack::creature_desire_sources::InnateTraits::lethargy>("lethargy")
	    .Field<&openblack::creature_desire_sources::InnateTraits::friendliness>("friendliness")
	    .Field<&openblack::creature_desire_sources::InnateTraits::communicativeness>("communicativeness")
	    .Field<&openblack::creature_desire_sources::InnateTraits::sixth>("sixth")
	    .Field<&openblack::creature_desire_sources::InnateTraits::objectsLookWeight>("objectsLookWeight")
	    .Field<&openblack::creature_desire_sources::InnateTraits::mountainsLookWeight>("mountainsLookWeight")
	    .Field<&openblack::creature_desire_sources::InnateTraits::waterLookWeight>("waterLookWeight");
	Reflect<openblack::creature_desire_sources::PlayerInterest>(context, ValueOnly {})
	    .Field<&openblack::creature_desire_sources::PlayerInterest::value>("value")
	    .Field<&openblack::creature_desire_sources::PlayerInterest::handMovement>("handMovement")
	    .Field<&openblack::creature_desire_sources::PlayerInterest::lastHand>("lastHand");
	Reflect<openblack::creature_desires::DesireState>(context, ValueOnly {})
	    .Field<&openblack::creature_desires::DesireState::activated>("activated")
	    .Field<&openblack::creature_desires::DesireState::value>("value")
	    .Field<&openblack::creature_desires::DesireState::max>("max")
	    .Field<&openblack::creature_desires::DesireState::decay>("decay")
	    .Field<&openblack::creature_desires::DesireState::increaseSeconds>("increaseSeconds")
	    .Field<&openblack::creature_desires::DesireState::suppressedTurns>("suppressedTurns")
	    .Field<&openblack::creature_desires::DesireState::weight>("weight")
	    .Field<&openblack::creature_desires::DesireState::carriedOut>("carriedOut")
	    .Field<&openblack::creature_desires::DesireState::sources>("sources");
	Reflect<openblack::creature_desires::Desires>(context, ValueOnly {})
	    .Field<&openblack::creature_desires::Desires::desires>("desires", {k_CreatureDesiresDesireNames})
	    .Field<&openblack::creature_desires::Desires::sum>("sum")
	    .Field<&openblack::creature_desires::Desires::written>("written");
	Reflect<openblack::creature_desires::Source>(context, ValueOnly {})
	    .Field<&openblack::creature_desires::Source::type>("type")
	    .Field<&openblack::creature_desires::Source::value>("value")
	    .Field<&openblack::creature_desires::Source::threshold>("threshold")
	    .Field<&openblack::creature_desires::Source::multiplier>("multiplier")
	    .Field<&openblack::creature_desires::Source::drive>("drive")
	    .Field<&openblack::creature_desires::Source::clearedWhenSatisfied>("clearedWhenSatisfied");
}
