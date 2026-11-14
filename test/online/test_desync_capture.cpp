/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Server/DesyncBundle.h"
#include "Server/DesyncCapture.h"

using namespace openblack;
using namespace openblack::server;

namespace
{

game_socket::SyncFacts ServerFacts()
{
	return {.turn = 0,
	        .build = "0.1.0+abc/codec35",
	        .land = "Online Maps/mpm_2p_1.txt",
	        .landSha256 = std::string(64, 'a'),
	        .map = {},
	        .seed = 7,
	        .data = 0x1234,
	        .patch = "1.20",
	        .language = "0809"};
}

std::vector<synced_state::EntityHash> Entities(uint64_t change)
{
	std::vector<synced_state::EntityHash> entities;
	for (uint32_t i = 0; i < 300; ++i)
	{
		entities.push_back({.entity = i, .hash = i == 270 ? change : i + 1, .kind = "Villager", .position = {}});
	}
	return entities;
}

/// A server with one capture, and the frames that it sends
struct Harness
{
	std::vector<std::pair<ConnectionId, game_socket::Frame>> sent;
	std::vector<CaptureRecord> finished;
	DesyncCapture capture {
	    ServerFacts(),
	    {
	        .send = [this](ConnectionId connection, const game_socket::Frame& frame) { sent.emplace_back(connection, frame); },
	        .entities =
	            [](std::span<const std::string> parts) {
		            std::vector<synced_state::PartEntities> result;
		            for (const auto& part : parts)
		            {
			            result.push_back({.part = part, .entities = Entities(5)});
		            }
		            return result;
	            },
	        .world = []() { return std::vector<uint8_t> {1, 2, 3}; },
	        .finished = [this](CaptureRecord record) { finished.push_back(std::move(record)); },
	    }};
};

game_socket::SyncBlocks PlayerBlocks(uint32_t turn, uint64_t change)
{
	const auto entities = Entities(change);
	return {.turn = turn,
	        .parts = {{.part = "Transform",
	                   .entities = static_cast<uint32_t>(entities.size()),
	                   .blocks = synced_state::BlocksOf(entities)}}};
}

game_socket::SyncEntities PlayerEntities(uint32_t turn, const game_socket::SyncAsk& ask, uint64_t change)
{
	game_socket::SyncEntities frame {.turn = turn, .parts = {}};
	const auto all = Entities(change);
	for (const auto& part : ask.parts)
	{
		game_socket::PartEntityHashes hashes {.part = part.part, .entities = {}};
		for (const auto& entity : synced_state::EntitiesOfBlocks(all, part.blocks))
		{
			hashes.entities.push_back(
			    {.entity = entity.entity, .hash = entity.hash, .kind = entity.kind, .position = entity.position});
		}
		frame.parts.push_back(std::move(hashes));
	}
	return frame;
}

} // namespace

TEST(DesyncCaptureTest, ACaptureFindsTheEntityAndTheFactThatDiffer)
{
	Harness harness;
	const std::vector<std::string> parts {"Transform"};
	auto playerFacts = ServerFacts();
	playerFacts.data = 0x9999;
	harness.capture.Facts(1, playerFacts);
	harness.capture.Parted(1, 42, 10, 12, parts, 0);
	ASSERT_EQ(harness.sent.size(), 1U);
	const auto* ask = std::get_if<game_socket::SyncCapture>(&harness.sent[0].second);
	ASSERT_NE(ask, nullptr);
	EXPECT_EQ(harness.sent[0].first, 42U);
	const auto turn = 12 + k_CaptureLeadTurns;
	EXPECT_EQ(ask->turn, turn);
	EXPECT_EQ(ask->parts, parts);

	// The player sends its blocks before the server gets to the turn
	harness.capture.Blocks(1, PlayerBlocks(turn, 6));
	EXPECT_EQ(harness.sent.size(), 1U);
	harness.capture.TurnDone(turn);
	ASSERT_EQ(harness.sent.size(), 2U);
	const auto* blocks = std::get_if<game_socket::SyncAsk>(&harness.sent[1].second);
	ASSERT_NE(blocks, nullptr);
	ASSERT_EQ(blocks->parts.size(), 1U);
	EXPECT_EQ(blocks->parts[0].blocks, std::vector<uint32_t> {1});

	harness.capture.Entities(1, PlayerEntities(turn, *blocks, 6));
	EXPECT_TRUE(harness.finished.empty());
	harness.capture.World(1, {.turn = turn, .index = 0, .count = 2, .size = 4, .bytes = {9, 8}});
	harness.capture.World(1, {.turn = turn, .index = 1, .count = 2, .size = 4, .bytes = {7, 6}});
	ASSERT_EQ(harness.finished.size(), 1U);
	const auto& record = harness.finished[0];
	EXPECT_EQ(record.seat, 1U);
	EXPECT_EQ(record.partTurn, 10U);
	EXPECT_TRUE(record.missing.empty());
	EXPECT_EQ(record.serverWorld, (std::vector<uint8_t> {1, 2, 3}));
	EXPECT_EQ(record.playerWorld, (std::vector<uint8_t> {9, 8, 7, 6}));
	ASSERT_TRUE(record.fact.has_value());
	EXPECT_EQ(record.fact->fact, "data");
	ASSERT_EQ(record.parts.size(), 1U);
	EXPECT_TRUE(record.parts[0].compared);
	ASSERT_EQ(record.parts[0].differences.size(), 1U);
	EXPECT_EQ(record.parts[0].differences[0].entity, 270U);
	EXPECT_FALSE(harness.capture.Capturing(1));
}

TEST(DesyncCaptureTest, AGodGetsOneCaptureAndTheGameGetsAFew)
{
	Harness harness;
	const std::vector<std::string> parts {"Transform"};
	harness.capture.Parted(0, 1, 5, 5, parts, 0);
	harness.capture.Parted(0, 1, 6, 6, parts, 0);
	EXPECT_EQ(harness.capture.Captures(), 1U);
	for (uint8_t seat = 1; seat < 10; ++seat)
	{
		harness.capture.Parted(seat, seat, 5, 5, parts, 0);
	}
	EXPECT_EQ(harness.capture.Captures(), k_MostCapturesInGame);
}

TEST(DesyncCaptureTest, ACaptureWithNoAnswerEndsWithTheMissingParts)
{
	Harness harness;
	const std::vector<std::string> parts {"Transform"};
	harness.capture.Parted(2, 1, 5, 5, parts, 0);
	harness.capture.Update(k_CaptureWaitMicroseconds);
	EXPECT_TRUE(harness.finished.empty());
	harness.capture.Update(k_CaptureWaitMicroseconds + 1);
	ASSERT_EQ(harness.finished.size(), 1U);
	EXPECT_EQ(harness.finished[0].missing, (std::vector<std::string> {"server", "blocks", "world"}));
	EXPECT_FALSE(harness.finished[0].fact.has_value());
}

TEST(DesyncCaptureTest, AWorldInTheWrongOrderIsNotKept)
{
	Harness harness;
	const std::vector<std::string> parts {"Transform"};
	harness.capture.Parted(0, 1, 5, 5, parts, 0);
	const auto turn = 5 + k_CaptureLeadTurns;
	harness.capture.TurnDone(turn);
	harness.capture.Blocks(0, PlayerBlocks(turn, 5));
	harness.capture.World(0, {.turn = turn, .index = 1, .count = 2, .size = 4, .bytes = {7, 6}});
	ASSERT_EQ(harness.finished.size(), 1U);
	EXPECT_TRUE(harness.finished[0].playerWorld.empty());
	EXPECT_EQ(harness.finished[0].missing, std::vector<std::string> {"world"});
	// Blocks that are the same as the server's need no entities
	EXPECT_TRUE(harness.finished[0].parts[0].compared);
	EXPECT_TRUE(harness.finished[0].parts[0].differences.empty());
}

TEST(DesyncCaptureTest, AGodThatGoesEndsItsCapture)
{
	Harness harness;
	const std::vector<std::string> parts {"Transform"};
	harness.capture.Parted(3, 1, 5, 5, parts, 0);
	harness.capture.Gone(4);
	EXPECT_TRUE(harness.finished.empty());
	harness.capture.Gone(3);
	EXPECT_EQ(harness.finished.size(), 1U);
}

TEST(DesyncBundleTest, TheNameHasTheTimeTheGameTheSlotAndTheTurn)
{
	const auto at = std::chrono::sys_days {std::chrono::year {2026} / 11 / 14} + std::chrono::hours {3} +
	                std::chrono::minutes {45} + std::chrono::seconds {7};
	EXPECT_EQ(desync_bundle::NameOf("game1", 1, 32, at), "20261114T034507Z_game1_slot1_turn32");
}

TEST(DesyncBundleTest, OldRecordsGoAfterTheDaysAndPastTheMostRecords)
{
	using Clock = std::chrono::system_clock;
	const auto now = Clock::time_point {} + std::chrono::days {100};
	const desync_bundle::Settings settings {.folder = "x", .keepDays = 14, .mostRecords = 2};
	const std::vector<std::pair<std::filesystem::path, Clock::time_point>> records {
	    {"old", now - std::chrono::days {15}},
	    {"a", now - std::chrono::days {3}},
	    {"b", now - std::chrono::days {2}},
	    {"c", now - std::chrono::days {1}},
	};
	const auto old = desync_bundle::OldRecords(records, now, settings);
	EXPECT_EQ(old, (std::vector<std::filesystem::path> {"old", "a"}));
}

TEST(DesyncBundleTest, TheDetailNamesTheFirstFactAndTheRecord)
{
	CaptureRecord record;
	record.seat = 1;
	record.captureTurn = 32;
	record.server = ServerFacts();
	record.player = ServerFacts();
	record.player->data = 0x9999;
	record.fact = online::desync_facts::FirstDifferent(record.server, *record.player);
	record.parts.push_back(
	    {.part = "Transform", .serverEntities = 3, .playerEntities = 3, .differences = {}, .compared = true});
	const auto detail = desync_bundle::DetailOf(record, "game1", "record1");
	EXPECT_EQ(detail.fact, "data");
	EXPECT_EQ(detail.attachmentName, "facts.json");
	EXPECT_NE(detail.attachment.find("\"record\": \"record1\""), std::string::npos);
	const auto found = std::ranges::find(detail.extras, std::string("record"), &std::pair<std::string, std::string>::first);
	ASSERT_NE(found, detail.extras.end());
	EXPECT_EQ(found->second, "record1");
	const auto lines = desync_bundle::Lines(record);
	ASSERT_FALSE(lines.empty());
	EXPECT_NE(lines[0].find("data"), std::string::npos);
}

TEST(DesyncBundleTest, TheServerWritesTheRecordAndKeepsOnlyTheNewest)
{
	std::error_code error;
	const auto folder = std::filesystem::temp_directory_path() / "openblack_test_desync_bundle";
	std::filesystem::remove_all(folder, error);
	const desync_bundle::Settings settings {.folder = folder, .keepDays = 14, .mostRecords = 1};
	CaptureRecord record;
	record.server = ServerFacts();
	record.serverWorld = {1, 2, 3};
	const auto now = std::chrono::system_clock::now();
	const auto first = desync_bundle::Write(settings, record, "game1", now - std::chrono::seconds {10});
	ASSERT_TRUE(first.has_value());
	EXPECT_TRUE(std::filesystem::exists(*first / "facts.json"));
	EXPECT_EQ(std::filesystem::file_size(*first / "server.obsav"), 3U);
	EXPECT_FALSE(std::filesystem::exists(*first / "player.obsav"));
	// A file that is not a record stays
	std::filesystem::create_directories(folder / "keep me", error);
	record.seat = 1;
	const auto second = desync_bundle::Write(settings, record, "game1", now);
	ASSERT_TRUE(second.has_value());
	EXPECT_FALSE(std::filesystem::exists(*first));
	EXPECT_TRUE(std::filesystem::exists(*second));
	EXPECT_TRUE(std::filesystem::exists(folder / "keep me"));
	std::filesystem::remove_all(folder, error);
}
