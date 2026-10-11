/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{
struct LivingAction;
}

/// What happens to a villager each turn before its state does its part: it loses the life its state costs; in a state
/// that looks at its needs it dies with no life left, and every few turns its age, its wounds and its belly are looked
/// at. A villager a script controls is left alone.
namespace openblack::ecs::villager_checks
{

void CheckEveryTime(components::LivingAction& action);

} // namespace openblack::ecs::villager_checks
