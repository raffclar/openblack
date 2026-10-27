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

#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureStomp.h"
#include "Creature/CreatureSwap.h"
#include "Creature/CreatureSway.h"
#include "ECS/Reflection/ComponentReflection.h"

namespace openblack::ecs::reflection
{
void RegisterFields_Values_creatures(entt::meta_ctx& context);
}

namespace
{

// The names of the enumerations indexing lists, which the lists' elements are read and set by
constexpr std::array<std::string_view, 16> k_CreatureSpellsSpellNames {
    "Freeze", "Small", "Big",    "Weak",       "Strong", "Fat", "Thin",    "Invisible",
    "Nice",   "Nasty", "Hungry", "Frightened", "Tired",  "Ill", "Thirsty", "Itchy"};
static_assert(static_cast<size_t>(openblack::creature_spells::Spell::_Count) == k_CreatureSpellsSpellNames.size());

} // namespace

void openblack::ecs::reflection::RegisterFields_Values_creatures(entt::meta_ctx& context)
{
	Reflect<openblack::creature_script_play::Request>(context, ValueOnly {})
	    .Field<&openblack::creature_script_play::Request::animation>("animation")
	    .Field<&openblack::creature_script_play::Request::plays>("plays");
	Reflect<openblack::creature_spell_mind::Cheat>(context, ValueOnly {})
	    .Field<&openblack::creature_spell_mind::Cheat::desire>("desire")
	    .Field<&openblack::creature_spell_mind::Cheat::turns>("turns")
	    .Field<&openblack::creature_spell_mind::Cheat::seconds>("seconds");
	Reflect<openblack::creature_spells::Slot>(context, ValueOnly {})
	    .Field<&openblack::creature_spells::Slot::phase>("phase")
	    .Field<&openblack::creature_spells::Slot::turnsLeft>("turnsLeft")
	    .Field<&openblack::creature_spells::Slot::holdTurns>("holdTurns")
	    .Field<&openblack::creature_spells::Slot::miracle>("miracle")
	    .Field<&openblack::creature_spells::Slot::before>("before");
	Reflect<openblack::creature_spells::Spells>(context, ValueOnly {})
	    .Field<&openblack::creature_spells::Spells::slots>("slots", {k_CreatureSpellsSpellNames})
	    .Field<&openblack::creature_spells::Spells::waiting>("waiting")
	    .Field<&openblack::creature_spells::Spells::startDelayTurns>("startDelayTurns")
	    .Field<&openblack::creature_spells::Spells::reversion>("reversion");
	Reflect<openblack::creature_spells::Waiting>(context, ValueOnly {})
	    .Field<&openblack::creature_spells::Waiting::spell>("spell")
	    .Field<&openblack::creature_spells::Waiting::holdTurns>("holdTurns")
	    .Field<&openblack::creature_spells::Waiting::miracle>("miracle")
	    .Field<&openblack::creature_spells::Waiting::seconds>("seconds");
	Reflect<openblack::creature_stomp::BodyStart>(context, ValueOnly {})
	    .Field<&openblack::creature_stomp::BodyStart::stands>("stands")
	    .Field<&openblack::creature_stomp::BodyStart::animation>("animation")
	    .Field<&openblack::creature_stomp::BodyStart::phase>("phase");
	Reflect<openblack::creature_swap::Shape>(context, ValueOnly {})
	    .Field<&openblack::creature_swap::Shape::size>("size")
	    .Field<&openblack::creature_swap::Shape::alignment>("alignment");
	Reflect<openblack::creature_sway::Sway>(context, ValueOnly {})
	    .Field<&openblack::creature_sway::Sway::lowerVelocity>("lowerVelocity")
	    .Field<&openblack::creature_sway::Sway::upperVelocity>("upperVelocity")
	    .Field<&openblack::creature_sway::Sway::lowerOffset>("lowerOffset")
	    .Field<&openblack::creature_sway::Sway::upperOffset>("upperOffset")
	    .Field<&openblack::creature_sway::Sway::active>("active")
	    .Field<&openblack::creature_sway::Sway::turnSeconds>("turnSeconds")
	    .Field<&openblack::creature_sway::Sway::heading>("heading");
}
