/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>

#include <Audio/AudioRing.h>
#include <gtest/gtest.h>

using namespace openblack::audio;

TEST(AudioRing, TheFilledAndFreeFramesComeFromTheTwoCounts)
{
	constexpr AudioRing ring {.capacity = 8192};
	EXPECT_EQ(AudioRing::Filled(3000, 1000), 2000u);
	EXPECT_EQ(ring.Free(3000, 1000), 6192u);
	EXPECT_EQ(ring.Free(9192, 1000), 0u);
}

TEST(AudioRing, TheCountsCanWrapRound)
{
	constexpr AudioRing ring {.capacity = 8192};
	constexpr uint32_t k_Read = std::numeric_limits<uint32_t>::max() - 99;
	constexpr uint32_t k_Written = k_Read + 300; // The count of the writer wrapped round
	EXPECT_EQ(AudioRing::Filled(k_Written, k_Read), 300u);
	EXPECT_EQ(ring.Free(k_Written, k_Read), 7892u);
	// The capacity is a power of two, so the place stays in step across the wrap
	EXPECT_EQ(ring.Place(k_Read + 100), 0u);
	EXPECT_EQ(ring.Place(k_Read + 99), 8191u);
}

TEST(AudioRing, TheWriterRendersWholeBlocksUpToTheTarget)
{
	constexpr AudioRing ring {.capacity = 8192};
	// 100 frames are in the ring: 2780 more reach the target of 2880, which is 11 blocks of 256
	EXPECT_EQ(ring.ToRender(1100, 1000, 2880, 256), 2816u);
	// At the target or above, the writer renders nothing
	EXPECT_EQ(ring.ToRender(3880, 1000, 2880, 256), 0u);
	EXPECT_EQ(ring.ToRender(4000, 1000, 2880, 256), 0u);
	// No block size gives nothing
	EXPECT_EQ(ring.ToRender(1000, 1000, 2880, 0), 0u);
}

TEST(AudioRing, TheWriterNeverRendersMoreThanTheFreeSpace)
{
	constexpr AudioRing ring {.capacity = 1024};
	// The target is above the capacity: only the free whole blocks
	EXPECT_EQ(ring.ToRender(1100, 1000, 4096, 256), 768u);
	EXPECT_EQ(ring.ToRender(1000, 1000, 4096, 256), 1024u);
}

TEST(AudioRing, ARunOfFramesTakesTheEndOfTheRingThenItsStart)
{
	constexpr AudioRing ring {.capacity = 8192};
	EXPECT_EQ(PartsOf(ring, 100, 256), (RingParts {.first = 100, .firstCount = 256, .secondCount = 0}));
	EXPECT_EQ(PartsOf(ring, 8000, 512), (RingParts {.first = 8000, .firstCount = 192, .secondCount = 320}));
	EXPECT_EQ(PartsOf(ring, 8192 + 8000, 192), (RingParts {.first = 8000, .firstCount = 192, .secondCount = 0}));
}
