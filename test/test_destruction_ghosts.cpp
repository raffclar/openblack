/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Components/DestructionGhost.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"

using namespace openblack::ecs;

// The ghost of a thing that has gone is drawn only: it is in the list of the explosions, not in the world
TEST(DestructionGhosts, AGhostRunsDownByTheFramesOnceDrawnAndGoes)
{
	systems::ExplosionSystem explosions;
	explosions.AddGhost(components::DestructionGhost {.mesh = 7});
	ASSERT_EQ(explosions.Ghosts().size(), 1u);
	// The first frame draws it and takes nothing of its time
	explosions.Update(100.0f);
	ASSERT_EQ(explosions.Ghosts().size(), 1u);
	EXPECT_EQ(explosions.Ghosts()[0].millisecondsLeft, 500);
	explosions.Update(300.0f);
	ASSERT_EQ(explosions.Ghosts().size(), 1u);
	EXPECT_EQ(explosions.Ghosts()[0].millisecondsLeft, 200);
	explosions.Update(300.0f);
	EXPECT_TRUE(explosions.Ghosts().empty());
}

TEST(DestructionGhosts, ANewLandHasNoGhosts)
{
	systems::ExplosionSystem explosions;
	explosions.AddGhost(components::DestructionGhost {.mesh = 7});
	explosions.Reset();
	EXPECT_TRUE(explosions.Ghosts().empty());
}
