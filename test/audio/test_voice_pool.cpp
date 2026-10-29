/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Audio/VoicePool.h"
#include "Sound/VoiceOwner.h"

using namespace openblack::audio;
using openblack::pack::AudioBankLoop;

namespace
{
constexpr uint32_t k_Bank = 7;

VoiceRequest Request(uint32_t sample, uint16_t priority, AudioBankLoop playType, uint64_t owner = 1, uint16_t group = 0)
{
	return {.owner = owner, .bank = k_Bank, .sample = sample, .group = group, .priority = priority, .playType = playType};
}

/// Fills all the voices with samples 100 and up of the priority
void Fill(VoicePool& pool, uint16_t priority)
{
	for (size_t i = 0; i < VoicePool::k_VoiceCount; ++i)
	{
		pool.Occupy(i, Request(static_cast<uint32_t>(100 + i), priority, AudioBankLoop::Overlap));
	}
}
} // namespace

TEST(VoicePool, HasSixteenVoices)
{
	EXPECT_EQ(VoicePool::k_VoiceCount, 16u);
}

TEST(VoicePool, OverlapTakesTheFirstFreeVoice)
{
	VoicePool pool;
	const auto request = Request(1, 400, AudioBankLoop::Overlap);
	const auto first = pool.Choose(request);
	ASSERT_TRUE(first.start);
	EXPECT_EQ(first.slot, 0u);
	pool.Occupy(0, request);
	// The same sample again plays on the next voice
	const auto second = pool.Choose(request);
	EXPECT_TRUE(second.start);
	EXPECT_EQ(second.slot, 1u);
}

TEST(VoicePool, OnceHandsBackTheVoiceThatPlays)
{
	VoicePool pool;
	pool.Occupy(3, Request(1, 400, AudioBankLoop::Once));
	const auto choice = pool.Choose(Request(1, 400, AudioBankLoop::Once));
	EXPECT_FALSE(choice.start);
	EXPECT_EQ(choice.slot, 3u);
}

TEST(VoicePool, OnceMatchesTheVoiceGroup)
{
	VoicePool pool;
	pool.Occupy(2, Request(1, 400, AudioBankLoop::Once, 1, 5));
	const auto choice = pool.Choose(Request(9, 400, AudioBankLoop::Once, 1, 5));
	EXPECT_FALSE(choice.start);
	EXPECT_EQ(choice.slot, 2u);
}

TEST(VoicePool, OnceStartsForAnotherOwnerOrAfterTheSampleStops)
{
	VoicePool pool;
	pool.Occupy(0, Request(1, 400, AudioBankLoop::Once, 1));
	const auto other = pool.Choose(Request(1, 400, AudioBankLoop::Once, 2));
	EXPECT_TRUE(other.start);
	EXPECT_EQ(other.slot, 1u);
	pool.Finish(0, false);
	const auto again = pool.Choose(Request(1, 400, AudioBankLoop::Once, 1));
	EXPECT_TRUE(again.start);
	EXPECT_EQ(again.slot, 0u);
}

TEST(VoicePool, RestartUsesTheVoiceOfTheSameSample)
{
	VoicePool pool;
	pool.Occupy(0, Request(8, 400, AudioBankLoop::Overlap));
	pool.Occupy(1, Request(1, 400, AudioBankLoop::Restart));
	const auto choice = pool.Choose(Request(1, 400, AudioBankLoop::Restart));
	EXPECT_TRUE(choice.start);
	EXPECT_EQ(choice.slot, 1u);
}

TEST(VoicePool, RestartUsesTheVoiceOfTheSameSampleAfterItStops)
{
	VoicePool pool;
	pool.Occupy(4, Request(1, 400, AudioBankLoop::Restart));
	pool.Finish(4, false);
	const auto choice = pool.Choose(Request(1, 400, AudioBankLoop::Restart));
	EXPECT_EQ(choice.slot, 4u);
}

TEST(VoicePool, RestartForgetsAnMpegSampleAfterItStops)
{
	VoicePool pool;
	pool.Occupy(4, Request(1, 400, AudioBankLoop::Restart));
	pool.Finish(4, true);
	const auto choice = pool.Choose(Request(1, 400, AudioBankLoop::Restart));
	EXPECT_EQ(choice.slot, 0u);
}

TEST(VoicePool, RestartFallsBackOnTheVoiceGroupThenOnAFreeVoice)
{
	VoicePool pool;
	pool.Occupy(0, Request(2, 400, AudioBankLoop::Restart, 1, 3));
	const auto group = pool.Choose(Request(1, 400, AudioBankLoop::Restart, 1, 3));
	EXPECT_EQ(group.slot, 0u);
	const auto free = pool.Choose(Request(1, 400, AudioBankLoop::Restart, 1, 0));
	EXPECT_EQ(free.slot, 1u);
}

TEST(VoicePool, FullPoolStealsTheFirstVoiceOfLowestPriority)
{
	VoicePool pool;
	Fill(pool, 400);
	pool.Occupy(5, Request(200, 200, AudioBankLoop::Overlap));
	pool.Occupy(9, Request(201, 200, AudioBankLoop::Overlap));
	const auto choice = pool.Choose(Request(1, 300, AudioBankLoop::Overlap));
	EXPECT_TRUE(choice.start);
	EXPECT_EQ(choice.slot, 5u);
}

TEST(VoicePool, FullPoolRefusesASampleOfEqualOrLowerPriority)
{
	VoicePool pool;
	Fill(pool, 400);
	EXPECT_FALSE(pool.Choose(Request(1, 400, AudioBankLoop::Overlap)).slot.has_value());
	EXPECT_FALSE(pool.Choose(Request(1, 200, AudioBankLoop::Restart)).slot.has_value());
	EXPECT_TRUE(pool.Choose(Request(1, 401, AudioBankLoop::Once)).start);
}

TEST(VoicePool, SampleOfPriorityZeroCannotSteal)
{
	VoicePool pool;
	Fill(pool, 0);
	const auto choice = pool.Choose(Request(1, 0, AudioBankLoop::Overlap));
	EXPECT_FALSE(choice.slot.has_value());
	EXPECT_FALSE(choice.start);
}

TEST(VoicePool, VoiceOfPriorityZeroGoesFirst)
{
	VoicePool pool;
	Fill(pool, 400);
	pool.Occupy(2, Request(200, 50, AudioBankLoop::Overlap));
	pool.Occupy(7, Request(201, 0, AudioBankLoop::Overlap));
	EXPECT_EQ(pool.Choose(Request(1, 10, AudioBankLoop::Overlap)).slot, 7u);
}

TEST(VoicePool, NoneStealsAlsoWhenAVoiceIsFree)
{
	VoicePool pool;
	Fill(pool, 400);
	pool.Occupy(6, Request(300, 100, AudioBankLoop::Overlap));
	// The free voice keeps the priority of the sample that it played last
	pool.Finish(0, false);
	const auto choice = pool.Choose(Request(1, 500, AudioBankLoop::None));
	EXPECT_EQ(choice.slot, 6u);
	// A play type that looks for a free voice takes the free voice
	EXPECT_EQ(pool.Choose(Request(1, 500, AudioBankLoop::Overlap)).slot, 0u);
}

TEST(VoicePool, UnusedVoicesHavePriorityZero)
{
	VoicePool pool;
	pool.Occupy(0, Request(1, 400, AudioBankLoop::Overlap));
	EXPECT_EQ(pool.Choose(Request(2, 1, AudioBankLoop::None)).slot, 1u);
}

TEST(VoicePool, CallerPlayTypeIsRestart)
{
	EXPECT_EQ(k_CallerPlayType, AudioBankLoop::Restart);
}

TEST(VoicePool, RestartOfTheSameSampleStopsItOnlyForTheSameOwner)
{
	VoicePool pool;
	const auto first = VoiceOwnerKey(VoiceOwner::Entity, 4u, 0, 0);
	const auto second = VoiceOwnerKey(VoiceOwner::Entity, 5u, 0, 0);
	pool.Occupy(0, Request(9, 400, k_CallerPlayType, first));
	EXPECT_EQ(pool.Choose(Request(9, 400, k_CallerPlayType, first)).slot, 0u);
	EXPECT_EQ(pool.Choose(Request(9, 400, k_CallerPlayType, second)).slot, 1u);
}

TEST(VoiceOwnerKey, NoOwnerAndNumberZeroAreTheSame)
{
	EXPECT_EQ(VoiceOwnerKey(VoiceOwner::Entity, std::nullopt, 0, 0), 0u);
	EXPECT_EQ(VoiceOwnerKey(VoiceOwner::Number, std::nullopt, 0, 0), 0u);
}

TEST(VoiceOwnerKey, KindsDoNotShareKeys)
{
	const auto entity = VoiceOwnerKey(VoiceOwner::Entity, 2u, 0, 0);
	const auto number = VoiceOwnerKey(VoiceOwner::Number, std::nullopt, 3, 0);
	const auto own = VoiceOwnerKey(VoiceOwner::OwnSound, std::nullopt, 0, 2);
	EXPECT_NE(entity, number);
	EXPECT_NE(entity, own);
	EXPECT_NE(number, own);
	EXPECT_EQ(number, VoiceOwnerKey(VoiceOwner::Number, 2u, 3, 9));
}

TEST(VoiceOwnerKey, EachOwnSoundHasItsOwnKey)
{
	VoicePool pool;
	const auto first = VoiceOwnerKey(VoiceOwner::OwnSound, std::nullopt, 0, 0);
	const auto second = VoiceOwnerKey(VoiceOwner::OwnSound, std::nullopt, 0, 1);
	EXPECT_NE(first, second);
	// A sample that plays once starts again for a sound of its own
	pool.Occupy(0, Request(9, 400, AudioBankLoop::Once, first));
	const auto choice = pool.Choose(Request(9, 400, AudioBankLoop::Once, second));
	EXPECT_TRUE(choice.start);
	EXPECT_EQ(choice.slot, 1u);
}
