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

void computer_world::RegisterCreatureCareRules(RuleTable& /*table*/)
{
	// The rules of the care of the god's creature are not made yet. Each rule goes in with AddRule and
	// MindDomain::CreatureCare.
}
