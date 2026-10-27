/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureAnimationLog.h"

using namespace openblack::creature_animation_log;

namespace
{
Clip BodyClip(size_t animation, float timeMs, bool looping = false)
{
	return {.layer = Layer::Body, .animation = animation, .timeMs = timeMs, .looping = looping, .durationMs = 1000};
}

Cause CauseBy(const char* by)
{
	return {.by = by};
}
} // namespace

TEST(CreatureAnimationLog, AClipThatPlaysOnOverTurnsIsOneEntry)
{
	Log log;
	for (uint32_t turn = 10; turn < 14; ++turn)
	{
		const std::array clips {BodyClip(96, static_cast<float>(turn - 10) * 100.0f)};
		log.Posed(turn, clips, CauseBy("object action"));
	}
	ASSERT_EQ(log.Current().size(), 1);
	EXPECT_EQ(log.Current()[0].startTurn, 10u);
	EXPECT_EQ(log.Current()[0].turns, 4u);
	EXPECT_FLOAT_EQ(log.Current()[0].lastAdvanceMs, 100.0f);
	EXPECT_EQ(log.Current()[0].cause.by, "object action");
	EXPECT_TRUE(log.History().empty());
}

TEST(CreatureAnimationLog, AClipThatStopsClosesAtTheTurnOfTheNextClip)
{
	Log log;
	const std::array eat {BodyClip(96, 0.0f)};
	const std::array stand {BodyClip(0, 0.0f, true)};
	log.Posed(1, eat, CauseBy("object action"));
	log.Posed(2, eat, CauseBy("object action"));
	log.Posed(3, stand, CauseBy("body"));
	ASSERT_EQ(log.History().size(), 1);
	EXPECT_EQ(log.History()[0].first.animation, 96u);
	EXPECT_EQ(log.History()[0].startTurn, 1u);
	EXPECT_EQ(log.History()[0].endTurn, 3u);
	ASSERT_EQ(log.Current().size(), 1);
	EXPECT_EQ(log.Current()[0].first.animation, 0u);
	// The cause of an entry is the cause of the turn that it started
	EXPECT_EQ(log.Current()[0].cause.by, "body");
}

TEST(CreatureAnimationLog, AOneShotThatStartsAgainIsANewEntryButALoopIsNot)
{
	Log log;
	log.Posed(1, std::array {BodyClip(55, 900.0f)}, CauseBy("body"));
	log.Posed(2, std::array {BodyClip(55, 50.0f)}, CauseBy("body"));
	EXPECT_EQ(log.History().size(), 1);
	EXPECT_EQ(log.Current()[0].startTurn, 2u);

	Log loops;
	loops.Posed(1, std::array {BodyClip(1, 950.0f, true)}, CauseBy("walk"));
	loops.Posed(2, std::array {BodyClip(1, 50.0f, true)}, CauseBy("walk"));
	EXPECT_TRUE(loops.History().empty());
	// The advance of a loop counts through the end of the clip
	EXPECT_FLOAT_EQ(loops.Current()[0].lastAdvanceMs, 100.0f);
}

TEST(CreatureAnimationLog, ClipsOfOtherLayersAndSidesAreOtherEntries)
{
	Log log;
	auto face = BodyClip(16, 0.0f);
	face.layer = Layer::Face;
	auto mirrored = BodyClip(96, 0.0f);
	mirrored.mirrored = true;
	log.Posed(1, std::array {BodyClip(96, 0.0f), face}, CauseBy("body"));
	log.Posed(2, std::array {mirrored, face}, CauseBy("body"));
	ASSERT_EQ(log.Current().size(), 2);
	EXPECT_EQ(log.History().size(), 1);
	EXPECT_EQ(log.Current()[0].startTurn, 2u);
	EXPECT_EQ(log.Current()[1].startTurn, 1u);
}

TEST(CreatureAnimationLog, AClipThatTheSpeciesLacksIsAMissingProblemOnce)
{
	Log log;
	auto missing = BodyClip(140, 0.0f);
	missing.layer = Layer::Blend;
	missing.found = false;
	for (uint32_t turn = 1; turn <= 3; ++turn)
	{
		missing.timeMs = static_cast<float>(turn) * 100.0f;
		log.Posed(turn, std::array {missing}, CauseBy("object action"));
	}
	ASSERT_EQ(log.Problems().size(), 1);
	EXPECT_EQ(log.Problems()[0].problem, Problem::Missing);
	EXPECT_EQ(log.Problems()[0].animation, 140u);
	EXPECT_EQ(log.Problems()[0].by, "object action");
}

TEST(CreatureAnimationLog, AProblemInTheNextTurnsCountsAsARepeat)
{
	Log log;
	const auto refused = [](uint32_t turn) {
		return ProblemEntry {.problem = Problem::Refused, .animation = 54, .turn = turn, .by = "mind"};
	};
	log.Note(refused(5));
	log.Note(refused(5));
	log.Note(refused(6));
	log.Note(refused(7));
	log.Note(refused(20));
	ASSERT_EQ(log.Problems().size(), 2);
	EXPECT_EQ(log.Problems()[0].repeats, 2u);
	EXPECT_EQ(log.Problems()[1].turn, 20u);
}

TEST(CreatureAnimationLog, TheLogKeepsOnlyTheNewestEntries)
{
	Log log(3);
	for (uint32_t turn = 0; turn < 10; ++turn)
	{
		log.Posed(turn, std::array {BodyClip(52 + turn, 0.0f)}, CauseBy("body"));
	}
	ASSERT_EQ(log.History().size(), 3);
	EXPECT_EQ(log.History().front().first.animation, 58u);
	EXPECT_EQ(log.History().back().first.animation, 60u);
	log.Clear();
	EXPECT_TRUE(log.History().empty());
	EXPECT_TRUE(log.Current().empty());
	EXPECT_FALSE(log.LastTurn().has_value());
}
