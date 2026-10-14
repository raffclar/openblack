/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "Creature/FightTranscript.h"

using namespace openblack::fight_transcript;

namespace
{
constexpr uint32_t k_Tiger = 7;
constexpr uint32_t k_Lion = 9;
constexpr uint32_t k_Bystander = 11;

Stamp At(uint64_t frame)
{
	return {.frame = frame, .turn = static_cast<uint32_t>(frame / 3), .gameMs = static_cast<double>(frame) * 33.0};
}
} // namespace

TEST(FightTranscript, AFightStartsWithItsStartRecord)
{
	Recorder recorder;
	const auto fight = recorder.Begin(k_Tiger, k_Lion, At(10), {{"reason", "script"}});
	ASSERT_EQ(recorder.Fights().size(), 1u);
	const auto* found = recorder.Find(fight);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->first, k_Tiger);
	EXPECT_EQ(found->second, k_Lion);
	ASSERT_EQ(found->entries.size(), 1u);
	EXPECT_EQ(found->entries.front().kind, "fight_start");
	EXPECT_EQ(found->entries.front().index, 0u);
	EXPECT_EQ(found->entries.front().stamp, At(10));
}

TEST(FightTranscript, RecordsGoToTheLastFightOfTheCreature)
{
	Recorder recorder;
	const auto first = recorder.Begin(k_Tiger, k_Lion, At(1), {});
	EXPECT_EQ(recorder.Add(k_Lion, "blow", At(2), {{"attacker", int64_t {k_Lion}}}), 1u);
	// A creature that never fought has no fight to keep its records
	EXPECT_FALSE(recorder.Add(k_Bystander, "blow", At(2), {}).has_value());

	const auto second = recorder.Begin(k_Tiger, k_Bystander, At(5), {});
	EXPECT_NE(first, second);
	EXPECT_EQ(recorder.FightOf(k_Tiger), second);
	// The lion was not in the second fight: its records still go to the first
	EXPECT_EQ(recorder.FightOf(k_Lion), first);
	EXPECT_EQ(recorder.Add(k_Tiger, "faint", At(6), {}), 1u);
	EXPECT_EQ(recorder.Find(second)->entries.back().kind, "faint");
	EXPECT_EQ(recorder.Find(first)->entries.size(), 2u);
}

TEST(FightTranscript, ACauseMustBeAnEarlierRecord)
{
	Recorder recorder;
	recorder.Begin(k_Tiger, k_Lion, At(1), {});
	const auto blow = recorder.Add(k_Tiger, "blow", At(2), {});
	const auto level = recorder.Add(k_Lion, "level", At(2), {}, blow);
	const auto* fight = recorder.Find(*recorder.FightOf(k_Tiger));
	EXPECT_EQ(fight->entries.at(*level).cause, blow);
	// A cause at or after the record itself is dropped
	const auto wrong = recorder.Add(k_Lion, "level", At(3), {}, uint64_t {99});
	EXPECT_FALSE(fight->entries.at(*wrong).cause.has_value());
}

TEST(FightTranscript, TheLastRecordOfAKindIsFoundByItsCreature)
{
	Recorder recorder;
	recorder.Begin(k_Tiger, k_Lion, At(1), {});
	const auto tiger = recorder.Add(k_Tiger, "action", At(2), {{"creature", int64_t {k_Tiger}}});
	const auto lion = recorder.Add(k_Lion, "action", At(3), {{"creature", int64_t {k_Lion}}});
	recorder.Add(k_Tiger, "blow", At(4), {{"attacker", int64_t {k_Tiger}}});
	EXPECT_EQ(recorder.LastOf(k_Tiger, "action"), tiger);
	EXPECT_EQ(recorder.LastOf(k_Lion, "action"), lion);
	EXPECT_EQ(recorder.LastOf(k_Tiger, "blow"), 3u);
	EXPECT_FALSE(recorder.LastOf(k_Lion, "blow").has_value());
}

TEST(FightTranscript, AnEndIsKeptOnceAndRecordsAfterItStillCount)
{
	Recorder recorder;
	const auto fight = recorder.Begin(k_Tiger, k_Lion, At(1), {});
	recorder.End(fight, At(20));
	recorder.End(fight, At(30));
	EXPECT_EQ(recorder.Find(fight)->ended, At(20));
	EXPECT_TRUE(recorder.Add(k_Lion, "taken_home", At(40), {}).has_value());
}

TEST(FightTranscript, AClearKeepsTheNumbersGoingUp)
{
	Recorder recorder;
	const auto first = recorder.Begin(k_Tiger, k_Lion, At(1), {});
	recorder.Clear();
	EXPECT_TRUE(recorder.Fights().empty());
	EXPECT_FALSE(recorder.FightOf(k_Tiger).has_value());
	EXPECT_GT(recorder.Begin(k_Tiger, k_Lion, At(2), {}), first);
}

TEST(FightTranscript, ARecordIsOneLineOfJsonWithStableNames)
{
	Recorder recorder;
	const auto fight = recorder.Begin(k_Tiger, k_Lion, At(3), {});
	const auto blow = recorder.Add(k_Tiger, "blow", At(4),
	                               {{"attacker", int64_t {k_Tiger}},
	                                {"outcome", "hit"},
	                                {"damage", 0.125f},
	                                {"blocked", false},
	                                {"note", std::string("a \"quoted\"\nline")}});
	const auto* found = recorder.Find(fight);
	const auto line = ToJsonLine(*found, found->entries.at(*blow));
	EXPECT_EQ(line.find('\n'), std::string::npos);
	// The texts that the records must give, kept out of the test macros
	const std::string expected =
	    R"({"v":1,"fight":1,"i":1,"frame":4,"turn":1,"game_ms":132,"kind":"blow","cause":null,)"
	    R"("fields":{"attacker":7,"outcome":"hit","damage":0.125,"blocked":false,"note":"a \"quoted\"\nline"}})";
	EXPECT_EQ(line, expected);

	const std::string summary =
	    R"({"v":1,"fight":1,"first":7,"second":9,"started":{"frame":3,"turn":1,"game_ms":99},"ended":null,)"
	    R"("records":2})";
	EXPECT_EQ(SummaryJson(*found), summary);
}

TEST(FightTranscript, ANumberThatIsNotFiniteIsWrittenAsNull)
{
	Recorder recorder;
	const auto fight = recorder.Begin(k_Tiger, k_Lion, At(1), {});
	const auto index = recorder.Add(k_Tiger, "level", At(2), {{"change", std::numeric_limits<double>::infinity()}}, 0);
	const auto* found = recorder.Find(fight);
	const std::string nullChange = R"("cause":0,"fields":{"change":null})";
	EXPECT_NE(ToJsonLine(*found, found->entries.at(*index)).find(nullChange), std::string::npos);
}
