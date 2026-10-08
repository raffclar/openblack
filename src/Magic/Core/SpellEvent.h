/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Effects/EffectValues.h"
#include "Particles/SpellLink.h"

// What a spell does when its effect reports an event.

namespace openblack::magic::spell_event
{
/// Types 1 and 11 are ignored (1), the rest ApplyDefaultSpellEffect
int SpellEvent(entt::entity spell, const psys::SpellEventInfo& event);

/// While the spell is open, the spell moves to the event, pays one event, and
/// the magic effect's EffectValues x strength x tribal power x event strength go to the target (type 5), to the other
/// spell (type 4) or to the map around the position (ApplyEffectToMapPos). Then the reaction, and the spell's movement
/// takes the event's velocity.
int ApplyDefaultSpellEffect(entt::entity spell, const psys::SpellEventInfo& event);

/// The spell's EffectValues scaled by PayForOneEvent's strength; false when that is 0
bool GetPaidEffectValues(entt::entity spell, ecs::effects::EffectValues& values);

/// Spell hitting spell: the other spell pays this one's strength x costPerShieldCollide (forced), this
/// one pays an event; true when the other is destroyed by it (or had no strength)
bool SpellHitSpell(entt::entity spell, entt::entity other);
} // namespace openblack::magic::spell_event
