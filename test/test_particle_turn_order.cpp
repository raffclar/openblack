/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Particles/ParticleTurnOrder.h"

using namespace openblack::particles::turn_order;

TEST(ParticleTurnOrder, TheSpotVisualsStepLastAndTheNewestFirst)
{
	// In the order of their start
	constexpr std::array k_Started {
	    Entry {.id = 1, .spotVisual = true},  Entry {.id = 2, .spotVisual = false}, Entry {.id = 3, .spotVisual = true},
	    Entry {.id = 4, .spotVisual = false}, Entry {.id = 5, .spotVisual = true},
	};
	EXPECT_EQ(Steps(k_Started), (std::vector<uint32_t> {2, 4, 5, 3, 1}));
}

TEST(ParticleTurnOrder, NoEffectsGiveNoSteps)
{
	EXPECT_TRUE(Steps({}).empty());
}

TEST(ParticleTurnOrder, EffectsThatAreNotSpotVisualsKeepTheOrderOfTheirStart)
{
	constexpr std::array k_Started {
	    Entry {.id = 7, .spotVisual = false},
	    Entry {.id = 3, .spotVisual = false},
	    Entry {.id = 9, .spotVisual = false},
	};
	EXPECT_EQ(Steps(k_Started), (std::vector<uint32_t> {7, 3, 9}));
}
