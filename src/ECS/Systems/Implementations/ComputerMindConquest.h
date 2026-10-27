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

#include <functional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/ComputerMind.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The parts of the conquest rules that other parts of the mind of a computer god use too
namespace openblack::ecs::systems::computer_world
{

struct RuleContext;

/// The town of the player above the node that the god most wants to win is the node's object. How much the god wants
/// it, not more than 1.
float TakeOverTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node);
/// The god's creature is the node's object. How much the god wants it to go to the storage pit of the town above the
/// node.
float CreatureToTown(RuleContext& context, computer_mind::Mind& mind, uint16_t node);

/// The score of a thing for a search. 0 or less skips the thing.
using ThingScore = std::function<float(entt::entity thing)>;
/// The thing in the cells around the place, in the influence of the god and its allies, whose score against its
/// distance is the best. Cells go along x, then along z, and in each cell the things that stay put come first. None
/// when no thing scores.
[[nodiscard]] entt::entity BestThingNear(const RuleContext& context, const map_coords::MapCoords& centre, float radius,
                                         const ThingScore& score);
/// How much the thing impresses when the god throws it. 0 for a thing the god cannot throw or that knocks down
/// buildings.
[[nodiscard]] float ThrowScoreOf(entt::entity thing);

/// The share of the miracles of the kind that the creature casts in fights and knows. 1 when there are none of them.
[[nodiscard]] float KnownFightMiracleShare(entt::entity creature, CreatureDesires kind);

} // namespace openblack::ecs::systems::computer_world
