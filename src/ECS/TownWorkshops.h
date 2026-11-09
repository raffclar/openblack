/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/fwd.hpp>

/// The list of workshops that each town keeps (components::Town::workshops). The rule of the list is
/// workshop::StartsWorkingInTown (WorkshopRules.h).
namespace openblack::town_workshops
{

/// A building of a town starts to work: it stands built when it is made, its builders finish it, or they repair it.
/// A workshop goes first in the list of its town, unless the list holds it already.
void StartsWorking(entt::entity building);

/// The workshops of the town that are still in the world, the newest to start to work first
[[nodiscard]] std::vector<entt::entity> Of(entt::entity town);

} // namespace openblack::town_workshops
