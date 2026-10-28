/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ComputerMindRuleTable.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// The rules of the worship part that use the workshops and their scaffolds: the god joins two scaffolds, takes a
/// scaffold to a town that wants its building, and fills a workshop with wood
namespace openblack::ecs::systems::computer_world
{

/// The worship part calls this to put the rules of the scaffolds in the table
void RegisterScaffoldRules(RuleTable& table);

} // namespace openblack::ecs::systems::computer_world
