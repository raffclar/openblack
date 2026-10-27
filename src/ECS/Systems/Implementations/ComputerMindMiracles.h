/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ComputerMindRuleTable.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindRules.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The miracles of a computer god that the other parts of its mind use: how it chooses a miracle for an aim, how much it
/// wants one, and the steps of the hand that cast it.
namespace openblack::ecs::systems::computer_world
{

/// How much the god wants to cast the miracle, from its charge, the time to charge the rest and the pull of the place.
/// Each look counts requests for the miracle. The icon of the miracle comes back in icon. With no place, the pull is 1.
float SpellWish(RuleContext& context, MagicType type, const std::optional<map_coords::MapCoords>& place, entt::entity& icon);

/// The best miracle of the kind for the place: the magic type whose wish times its suit is the largest, the first of the
/// largest. Its icon comes back in icon. 0 when no miracle suits.
float BestSpellOfKind(RuleContext& context, computer_mind_rules::SpellKind kind,
                      const std::optional<map_coords::MapCoords>& place, entt::entity& icon);

/// The rules of nodes that choose a miracle for an aim. The node's object becomes the icon of the miracle.
/// - An angry miracle at the creature above the node, but not when it is too near the god's own creature.
float AggressiveSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node);
/// - A kind miracle, at no place.
float CompassionateSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node);
/// - A miracle that impresses the town above the node, at the place where the most people see it.
float ImpressiveSpell(RuleContext& context, computer_mind::Mind& mind, uint16_t node);

/// The god has a miracle of the magic type of its own at the place now
[[nodiscard]] bool OwnCastAt(const RuleContext& context, const map_coords::MapCoords& place, MagicType type);

/// The hand goes to the icon or draws its gesture, and the worshippers chant for its miracle
void ReachAndChant(RuleContext& context, entt::entity icon);
/// The steps that cast the miracle of the icon at the thing. A shield gets the radius.
void CastAtThing(RuleContext& context, entt::entity target, entt::entity icon, float radius);
/// The steps that cast the miracle of the icon at the place, from the side of the hand at the radius
void CastAtPlace(RuleContext& context, const map_coords::MapCoords& place, entt::entity icon, float radius);

/// The hand carries the thing of the node's chosen leaf near to the thing above the node, and puts it down there
bool CarryNear(RuleContext& context, computer_mind::Mind& mind, uint16_t node);

} // namespace openblack::ecs::systems::computer_world
