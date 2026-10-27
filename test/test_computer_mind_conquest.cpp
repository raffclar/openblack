/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindConquest, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionTakeOverTown), MindDomain::Conquest);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastSpellToImpress), MindDomain::Conquest);
	EXPECT_EQ(DomainOf(NodeKind::ObjectGetCreaturetoHelp), MindDomain::Conquest);
}
