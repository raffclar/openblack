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

#include "Common/VirtualInfluence.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/VillagerDeathRules.h"
#include "Enums.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_v(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 10> k_DeathReasonNames {
    "None",      "Starving",   "Spell", "Animal", "Chant", "PlayerInteraction", "PlayerInteractionDrown",
    "Sacrifice", "Exhaustion", "OldAge"};
static_assert(static_cast<size_t>(openblack::DeathReason::_COUNT) == k_DeathReasonNames.size());
constexpr std::array<std::string_view, 8> k_PlayerNamesNames {"PLAYER_ONE",  "PLAYER_TWO", "PLAYER_THREE", "PLAYER_FOUR",
                                                              "PLAYER_FIVE", "PLAYER_SIX", "PLAYER_SEVEN", "NEUTRAL"};
static_assert(static_cast<size_t>(openblack::PlayerNames::_COUNT) == k_PlayerNamesNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Values_v(entt::meta_ctx& context)
{
	Reflect<openblack::villager_death::DeathCounts>(context, ValueOnly {})
	    .Field<&openblack::villager_death::DeathCounts::lastDeathTurn>("lastDeathTurn")
	    .Field<&openblack::villager_death::DeathCounts::total>("total")
	    .Field<&openblack::villager_death::DeathCounts::byPlayer>("byPlayer", {k_PlayerNamesNames})
	    .Field<&openblack::villager_death::DeathCounts::byReason>("byReason", {k_DeathReasonNames});
	Reflect<openblack::virtual_influence::Shown>(context, ValueOnly {})
	    .Field<&openblack::virtual_influence::Shown::soundFraction>("soundFraction")
	    .Field<&openblack::virtual_influence::Shown::soundStarted>("soundStarted")
	    .Field<&openblack::virtual_influence::Shown::manaPathStart>("manaPathStart")
	    .Field<&openblack::virtual_influence::Shown::manaPathEmission>("manaPathEmission");
	Reflect<openblack::virtual_influence::State>(context, ValueOnly {})
	    .Field<&openblack::virtual_influence::State::anchor>("anchor")
	    .Field<&openblack::virtual_influence::State::turnHand>("turnHand")
	    .Field<&openblack::virtual_influence::State::lastInsideTurn>("lastInsideTurn")
	    .Field<&openblack::virtual_influence::State::fraction>("fraction")
	    .Field<&openblack::virtual_influence::State::disabled>("disabled");
}
