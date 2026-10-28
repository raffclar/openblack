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

#include "3D/MapCoords.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The parts of the war rules of a computer god that other parts of its mind use too
namespace openblack::ecs::systems::computer_world
{

struct RuleContext;

/// The building or the field of the town that the god most wants to destroy, from how much of it stands, its life, how
/// much the game wants computer gods to attack it and how near it is to the hand. None when the town has nothing to
/// destroy.
[[nodiscard]] entt::entity TownTarget(entt::entity town, const map_coords::MapCoords& hand);

} // namespace openblack::ecs::systems::computer_world
