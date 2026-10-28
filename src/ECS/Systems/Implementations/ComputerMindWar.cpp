/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerMindRuleTable.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void computer_world::RegisterWarRules(RuleTable& /*table*/)
{
	// The rules of the attacks on other players are not made yet. Add each rule with AddRule and MindDomain::War.
}
