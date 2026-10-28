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

void computer_world::RegisterCreaturePlayRules(RuleTable& /*table*/)
{
	// The rules of the play with the god's creature and its lessons are not made yet. Each rule goes in with AddRule and
	// MindDomain::CreaturePlay.
}
