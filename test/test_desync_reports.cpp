/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <Common/DesyncReports.h>
#include <gtest/gtest.h>

using namespace openblack::error_reports;

namespace
{

constexpr uint8_t k_Codec = 34;

DesyncFacts FactsAt(uint32_t turn)
{
	return {.turn = turn, .ownChecksum = 0x1111, .otherChecksum = 0x2222, .slot = std::nullopt};
}

/// One desync of the client at a time, with the names of its parts at once
void Desync(DesyncReportRule& rule, uint32_t turn, std::vector<std::string> parts, int64_t now)
{
	rule.Detected(0, FactsAt(turn), now);
	rule.Parted(0, parts, now);
}

std::optional<std::string> FieldOf(const std::vector<std::pair<std::string, std::string>>& fields, std::string_view key)
{
	const auto found = std::ranges::find(fields, key, &std::pair<std::string, std::string>::first);
	return found != fields.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace

TEST(DesyncReports, TheKeyIsTheFirstPartThatDiffers)
{
	const std::vector<std::string> parts = {"Transform", "VillagerPose"};
	EXPECT_EQ(DesyncKey(parts), "Transform");
	EXPECT_EQ(DesyncKey({}), k_UnknownDesyncPart);
	// A name from another computer keeps only the characters of the names of the code
	const std::vector<std::string> odd = {"Bad\nname\"x"};
	EXPECT_EQ(DesyncKey(odd), "Bad?name?x");
}

TEST(DesyncReports, TheFirstDesyncOfAKeyGoesOnceAndALoopOfTheSameKeyOnlyCounts)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	Desync(rule, 100, {"Transform"}, 0);
	auto reports = rule.Update(0, budget);
	ASSERT_EQ(reports.size(), 1u);
	EXPECT_EQ(reports[0].kind, DesyncReport::Kind::Desync);
	EXPECT_EQ(reports[0].key, "Transform");
	EXPECT_EQ(reports[0].facts.turn, 100u);
	EXPECT_EQ(reports[0].desyncsInGame, 1u);
	EXPECT_EQ(reports[0].game, "game1");
	EXPECT_EQ(reports[0].turnCodec, k_Codec);
	// The resync, then the same fault again and again: no more events, long after the gap too
	for (uint32_t i = 1; i <= 50; ++i)
	{
		Desync(rule, 100 + i * 10, {"Transform"}, i * 1000);
		EXPECT_TRUE(rule.Update(i * 1000, budget).empty());
	}
	EXPECT_TRUE(rule.Update(10 * k_DesyncEventGapMilliseconds, budget).empty());
	EXPECT_EQ(rule.CountOf("Transform"), 51u);
	EXPECT_EQ(rule.EventsSent(), 1u);
	// The summary at the end carries the count
	const auto summary = rule.Finish(budget);
	ASSERT_TRUE(summary.has_value());
	EXPECT_EQ(summary->kind, DesyncReport::Kind::Summary);
	EXPECT_EQ(summary->desyncsInGame, 51u);
	EXPECT_EQ(summary->notSent, 50u);
	ASSERT_EQ(summary->counts.size(), 1u);
	EXPECT_EQ(summary->counts[0], std::make_pair(std::string("Transform"), 51u));
	// Only one summary, and nothing after it
	EXPECT_FALSE(rule.Finish(budget).has_value());
	Desync(rule, 900, {"Other"}, 999'999);
	EXPECT_TRUE(rule.Update(999'999, budget).empty());
}

TEST(DesyncReports, AGameWithEachDesyncSentHasNoSummary)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	EXPECT_FALSE(rule.Finish(budget).has_value());
	DesyncReportRule once("game2", k_Codec);
	Desync(once, 10, {"Transform"}, 0);
	EXPECT_EQ(once.Update(0, budget).size(), 1u);
	EXPECT_FALSE(once.Finish(budget).has_value());
}

TEST(DesyncReports, ANewKeyInTheGapWaitsForTheEndOfTheGap)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	Desync(rule, 10, {"Transform"}, 0);
	EXPECT_EQ(rule.Update(0, budget).size(), 1u);
	Desync(rule, 20, {"Flock"}, 1000);
	EXPECT_TRUE(rule.Update(1000, budget).empty());
	EXPECT_TRUE(rule.Update(k_DesyncEventGapMilliseconds - 1, budget).empty());
	const auto reports = rule.Update(k_DesyncEventGapMilliseconds, budget);
	ASSERT_EQ(reports.size(), 1u);
	EXPECT_EQ(reports[0].key, "Flock");
	// The waiting event keeps the facts of its first desync
	EXPECT_EQ(reports[0].facts.turn, 20u);
	EXPECT_EQ(reports[0].desyncsInGame, 2u);
}

TEST(DesyncReports, AGameSendsAtMostItsCapAndCountsTheRestInTheSummary)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	int64_t now = 0;
	for (uint32_t i = 0; i < k_MostDesyncEventsInGame + 3; ++i)
	{
		Desync(rule, i, {"Part" + std::to_string(i)}, now);
		std::ignore = rule.Update(now, budget);
		now += k_DesyncEventGapMilliseconds;
	}
	EXPECT_EQ(rule.EventsSent(), k_MostDesyncEventsInGame);
	EXPECT_TRUE(rule.Update(now * 2, budget).empty());
	const auto summary = rule.Finish(budget);
	ASSERT_TRUE(summary.has_value());
	EXPECT_EQ(summary->notSent, 3u);
	EXPECT_EQ(summary->counts.size(), k_MostDesyncEventsInGame + 3);
}

TEST(DesyncReports, TheRunSendsAtMostItsCapForAllGames)
{
	DesyncRunBudget budget;
	uint32_t sent = 0;
	for (uint32_t game = 0; game < k_MostDesyncEventsInRun; ++game)
	{
		DesyncReportRule rule("game" + std::to_string(game), k_Codec);
		Desync(rule, 1, {"Transform"}, 0);
		Desync(rule, 2, {"Transform"}, 0);
		sent += static_cast<uint32_t>(rule.Update(0, budget).size());
		sent += rule.Finish(budget).has_value() ? 1 : 0;
	}
	EXPECT_EQ(sent, k_MostDesyncEventsInRun);
	EXPECT_EQ(budget.Sent(), k_MostDesyncEventsInRun);
	DesyncReportRule late("late", k_Codec);
	Desync(late, 1, {"Transform"}, 0);
	EXPECT_TRUE(late.Update(0, budget).empty());
	EXPECT_FALSE(late.Finish(budget).has_value());
}

TEST(DesyncReports, ADesyncWithNoPartsTakesTheUnknownKeyAfterTheWait)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	rule.Detected(0, FactsAt(10), 0);
	EXPECT_TRUE(rule.Update(k_DesyncPartsWaitMilliseconds - 1, budget).empty());
	const auto reports = rule.Update(k_DesyncPartsWaitMilliseconds, budget);
	ASSERT_EQ(reports.size(), 1u);
	EXPECT_EQ(reports[0].key, k_UnknownDesyncPart);
	// Parts that come after the wait change nothing
	rule.Parted(0, std::vector<std::string> {"Transform"}, k_DesyncPartsWaitMilliseconds + 1);
	EXPECT_EQ(rule.CountOf("Transform"), 0u);
	EXPECT_EQ(rule.CountOf(k_UnknownDesyncPart), 1u);
}

TEST(DesyncReports, EachSlotOfTheServerWaitsForItsOwnParts)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	rule.Detected(0, {.turn = 5, .ownChecksum = 1, .otherChecksum = 2, .slot = 0}, 0);
	rule.Detected(1, {.turn = 6, .ownChecksum = 1, .otherChecksum = 3, .slot = 1}, 0);
	rule.Parted(1, std::vector<std::string> {"Flock"}, 0);
	rule.Parted(0, std::vector<std::string> {"Transform"}, 0);
	auto reports = rule.Update(0, budget);
	ASSERT_EQ(reports.size(), 1u);
	EXPECT_EQ(reports[0].key, "Flock");
	EXPECT_EQ(reports[0].facts.slot, uint8_t {1});
	reports = rule.Update(k_DesyncEventGapMilliseconds, budget);
	ASSERT_EQ(reports.size(), 1u);
	EXPECT_EQ(reports[0].key, "Transform");
	EXPECT_EQ(reports[0].facts.slot, uint8_t {0});
}

TEST(DesyncReports, AGameThatEndsWhileADesyncWaitsCountsItInTheSummary)
{
	DesyncRunBudget budget;
	DesyncReportRule rule("game1", k_Codec);
	rule.Detected(0, FactsAt(10), 0);
	const auto summary = rule.Finish(budget);
	ASSERT_TRUE(summary.has_value());
	EXPECT_EQ(summary->notSent, 1u);
	EXPECT_EQ(rule.CountOf(k_UnknownDesyncPart), 1u);
}

TEST(DesyncReports, TheEventOfADesyncHasTheKeyInItsFingerprintAndNoPersonalData)
{
	const DesyncReport report {
	    .kind = DesyncReport::Kind::Desync,
	    .game = "0123456789abcdef",
	    .turnCodec = k_Codec,
	    .key = "Transform",
	    .facts = {.turn = 300, .ownChecksum = 0xabc, .otherChecksum = 0xdef, .slot = 1},
	    .parts = {"Transform", "VillagerPose"},
	    .desyncsInGame = 2,
	    .counts = {},
	    .notSent = 0,
	};
	const auto fields = DesyncEventFieldsOf(report);
	EXPECT_EQ(fields.level, "warning");
	EXPECT_EQ(fields.message, "Desync: Transform");
	EXPECT_EQ(fields.fingerprint, (std::vector<std::string> {"desync", "Transform"}));
	EXPECT_EQ(FieldOf(fields.tags, "game"), "0123456789abcdef");
	EXPECT_EQ(FieldOf(fields.tags, "turn_codec"), "34");
	EXPECT_EQ(FieldOf(fields.tags, "desync_key"), "Transform");
	EXPECT_EQ(FieldOf(fields.tags, "slot"), "1");
	EXPECT_EQ(FieldOf(fields.extras, "turn"), "300");
	EXPECT_EQ(FieldOf(fields.extras, "own_checksum"), "0000000000000abc");
	EXPECT_EQ(FieldOf(fields.extras, "other_checksum"), "0000000000000def");
	EXPECT_EQ(FieldOf(fields.extras, "parts"), "Transform, VillagerPose");
	EXPECT_EQ(FieldOf(fields.extras, "desyncs_in_game"), "2");
	// The fields are only these: no name, account, address or chat
	std::vector<std::string> keys;
	for (const auto& [key, value] : fields.tags)
	{
		keys.push_back(key);
	}
	for (const auto& [key, value] : fields.extras)
	{
		keys.push_back(key);
	}
	std::ranges::sort(keys);
	EXPECT_EQ(keys, (std::vector<std::string> {"desync_key", "desyncs_in_game", "game", "other_checksum", "own_checksum",
	                                           "parts", "slot", "turn", "turn_codec"}));
}

TEST(DesyncReports, TheEventOfASummaryHasTheCountOfEachKey)
{
	const DesyncReport report {
	    .kind = DesyncReport::Kind::Summary,
	    .game = "g",
	    .turnCodec = k_Codec,
	    .key = {},
	    .facts = {},
	    .parts = {},
	    .desyncsInGame = 7,
	    .counts = {{"Transform", 5}, {"Flock", 2}},
	    .notSent = 5,
	};
	const auto fields = DesyncEventFieldsOf(report);
	EXPECT_EQ(fields.level, "info");
	EXPECT_EQ(fields.fingerprint, (std::vector<std::string> {"desync-summary"}));
	EXPECT_EQ(FieldOf(fields.extras, "count.Transform"), "5");
	EXPECT_EQ(FieldOf(fields.extras, "count.Flock"), "2");
	EXPECT_EQ(FieldOf(fields.extras, "not_sent"), "5");
	EXPECT_EQ(FieldOf(fields.extras, "desyncs_in_game"), "7");
	EXPECT_FALSE(FieldOf(fields.tags, "desync_key").has_value());
}

TEST(DesyncReports, TheGameIdOfAClientHasSixteenDigits)
{
	EXPECT_EQ(ClientGameId(0x1234), "0000000000001234");
}
