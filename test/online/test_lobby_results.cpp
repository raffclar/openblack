/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <JoinTicket.h>
#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "Server/Ed25519.h"
#include "Server/LobbyResults.h"

using namespace openblack;
using namespace openblack::server;
using namespace openblack::server::lobby_results;

namespace
{

LockstepHost::Seat SeatOf(LockstepHost::SeatState state, std::string account)
{
	LockstepHost::Seat seat;
	seat.state = state;
	seat.account = std::move(account);
	return seat;
}

const Game k_Game {.id = "game-1", .land = "Scripts/Playgrounds/TwoGods.txt", .turn = 1234};

/// The private key of test 1 of RFC 8032 in PEM text, a key of the test only
constexpr std::string_view k_TestKeyPem = "-----BEGIN PRIVATE KEY-----\n"
                                          "MC4CAQAwBQYDK2VwBCIEIJ1hsZ3v/VpguoRK9JLsLMREScVpezJpGXA7rAMcrn9g\n"
                                          "-----END PRIVATE KEY-----\n";
constexpr std::array<uint8_t, 32> k_TestSeed {0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60, 0xba, 0x84, 0x4a,
                                              0xf4, 0x92, 0xec, 0x2c, 0xc4, 0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32,
                                              0x69, 0x19, 0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60};

} // namespace

TEST(LobbyResults, TheLastPlaceIsTheWinner)
{
	// Two gods: the first god out has place 1, the winner place 2
	EXPECT_EQ(PositionOf(2, 2), 1);
	EXPECT_EQ(PositionOf(1, 2), 2);
	// Four gods
	EXPECT_EQ(PositionOf(4, 4), 1);
	EXPECT_EQ(PositionOf(3, 4), 2);
	EXPECT_EQ(PositionOf(1, 4), 4);
	// A god with no place shares the position after the gods with a place
	EXPECT_EQ(PositionOf(std::nullopt, 4), 5);
	EXPECT_EQ(PositionOf(std::nullopt, 0), 1);
}

TEST(LobbyResults, ReportHasEachGodOfASeatWithAnAccount)
{
	const std::vector seats {
	    SeatOf(LockstepHost::SeatState::Taken, "7"), SeatOf(LockstepHost::SeatState::Closed, "9"),
	    SeatOf(LockstepHost::SeatState::Open, "13"), SeatOf(LockstepHost::SeatState::Away, "bad"),
	    SeatOf(LockstepHost::SeatState::Away, "11"),
	};
	// The god of slot 4 is player 5
	const std::vector<GodFigures> gods {
	    {.god = PlayerNames::PLAYER_ONE, .place = 3, .godsDefeated = 2},
	    {.god = PlayerNames::PLAYER_TWO, .place = 1, .godsDefeated = 0},
	    {.god = PlayerNames::PLAYER_THREE, .place = std::nullopt, .godsDefeated = 0},
	    {.god = PlayerNames::PLAYER_FOUR, .place = 4, .godsDefeated = 1},
	    {.god = PlayerNames::PLAYER_FIVE, .place = 2, .godsDefeated = 1},
	};
	const std::vector<lobby_api::ServerStatusClan> clans {{.account = 11, .clan = 5}};
	const auto report = MakeReport(k_Game, gods, seats, clans);
	ASSERT_TRUE(report.has_value());
	EXPECT_EQ(report->game, "game-1");
	EXPECT_EQ(report->land, k_Game.land);
	ASSERT_EQ(report->gods.size(), 3U);
	EXPECT_EQ(report->gods[0].account, 7);
	EXPECT_EQ(report->gods[0].position, 2);
	EXPECT_EQ(report->gods[0].godsDefeated, 2);
	EXPECT_EQ(report->gods[0].timePlayed, 123);
	EXPECT_FALSE(report->gods[0].clan.has_value());
	EXPECT_EQ(report->gods[1].account, 9);
	EXPECT_EQ(report->gods[1].position, 4);
	EXPECT_EQ(report->gods[2].account, 11);
	EXPECT_EQ(report->gods[2].position, 3);
	EXPECT_EQ(report->gods[2].clan, 5);
}

TEST(LobbyResults, NoReportWithFewerThanTwoGods)
{
	const std::vector seats {SeatOf(LockstepHost::SeatState::Taken, "7"), SeatOf(LockstepHost::SeatState::Taken, "x")};
	const std::vector<GodFigures> gods {{.god = PlayerNames::PLAYER_ONE, .place = 2, .godsDefeated = 1},
	                                    {.god = PlayerNames::PLAYER_TWO, .place = 1, .godsDefeated = 0}};
	EXPECT_FALSE(MakeReport(k_Game, gods, seats, {}).has_value());
	// A seat whose god is not a god of the game is not in the report
	const std::vector two {SeatOf(LockstepHost::SeatState::Taken, "7"), SeatOf(LockstepHost::SeatState::Taken, "8")};
	EXPECT_FALSE(MakeReport(k_Game, std::span(gods).first(1), two, {}).has_value());
	EXPECT_TRUE(MakeReport(k_Game, gods, two, {}).has_value());
}

TEST(LobbyResults, GameIdKeepsTheLimitOfTheLobby)
{
	const Game game {.id = std::string(100, 'a'), .land = "", .turn = 0};
	const std::vector seats {SeatOf(LockstepHost::SeatState::Taken, "7"), SeatOf(LockstepHost::SeatState::Taken, "8")};
	const std::vector<GodFigures> gods {{.god = PlayerNames::PLAYER_ONE, .place = 2, .godsDefeated = 1},
	                                    {.god = PlayerNames::PLAYER_TWO, .place = 1, .godsDefeated = 0}};
	const auto report = MakeReport(game, gods, seats, {});
	ASSERT_TRUE(report.has_value());
	EXPECT_EQ(report->game.size(), k_MostGameIdCharacters);
	EXPECT_EQ(report->gods[0].timePlayed, 0);
}

TEST(LobbyResults, BodyHasTheFieldsOfTheLobby)
{
	const lobby_api::ResultReport report {
	    .game = "g",
	    .land = "L",
	    .gods = {{.account = 7, .clan = 5, .position = 1, .godsDefeated = 1, .timePlayed = 60},
	             {.account = 8, .clan = std::nullopt, .position = 2, .godsDefeated = std::nullopt, .timePlayed = std::nullopt}},
	};
	EXPECT_EQ(lobby_api::ToJson(report), R"({"game":"g","gods":[{"account":7,"clan":5,"gods_defeated":1,"position":1,)"
	                                     R"("time_played":60},{"account":8,"clan":null,"position":2}],"land":"L"})");
}

TEST(LobbyResults, AnswerGivesTheResult)
{
	const auto answer = lobby_api::ParseResultAnswer(R"({"result": 12, "gods": []})");
	ASSERT_TRUE(answer.has_value());
	EXPECT_EQ(answer->result, 12);
	EXPECT_FALSE(lobby_api::ParseResultAnswer(R"({"gods": []})").has_value());
}

TEST(LobbyResults, SignatureCoversThePrefixAndTheBody)
{
	const std::string body = R"({"game":"g"})";
	const auto bytes = SignedBytes(body);
	EXPECT_EQ(std::string(bytes.begin(), bytes.end()), "bwgame-result-v1\n" + body);
	const auto seed = ed25519::SeedOfPem(k_TestKeyPem);
	ASSERT_TRUE(seed.has_value());
	EXPECT_EQ(*seed, k_TestSeed);
	const auto signature = ed25519::Sign(*seed, bytes);
	const auto key = ed25519::PublicKeyOf(*seed);
	ASSERT_TRUE(signature.has_value());
	ASSERT_TRUE(key.has_value());
	EXPECT_TRUE(ed25519::Verify(*key, bytes, *signature));
	EXPECT_FALSE(ed25519::Verify(*key, SignedBytes(R"({"game":"h"})"), *signature));
	EXPECT_EQ(join_ticket::EncodeBase64Url(*signature).size(), 86U);
}

TEST(LobbyResults, KeyMustBeAnEd25519PrivateKey)
{
	EXPECT_FALSE(ed25519::SeedOfPem("").has_value());
	EXPECT_FALSE(ed25519::SeedOfPem("not a key").has_value());
	// The public key of the test key
	EXPECT_FALSE(ed25519::SeedOfPem("-----BEGIN PUBLIC KEY-----\n"
	                                "MCowBQYDK2VwAyEA11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=\n"
	                                "-----END PUBLIC KEY-----\n")
	                 .has_value());
}

TEST(LobbyResults, AddressOfTheResults)
{
	EXPECT_EQ(ResultsAddress("https://srv.example.org/api/v1/", "abc"), "https://srv.example.org/api/v1/servers/abc/results");
	EXPECT_EQ(ResultsAddress("https://srv.example.org/api/v1", "abc"), "https://srv.example.org/api/v1/servers/abc/results");
}

TEST(LobbyResults, ServerSendsAgainOnlyWhenTheLobbyDidNotAnswer)
{
	EXPECT_EQ(NextAfter(201), Next::Done);
	EXPECT_EQ(NextAfter(200), Next::Done);
	EXPECT_EQ(NextAfter(0), Next::SendAgain);
	EXPECT_EQ(NextAfter(429), Next::SendAgain);
	EXPECT_EQ(NextAfter(502), Next::SendAgain);
	EXPECT_EQ(NextAfter(401), Next::Stop);
	EXPECT_EQ(NextAfter(409), Next::Stop);
	EXPECT_EQ(NextAfter(422), Next::Stop);
}

TEST(LobbyResults, GameIdStaysInTheFolderOfTheSnapshots)
{
	const std::array<uint8_t, 16> random {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
	const auto id = GameIdOf(random);
	EXPECT_EQ(id.size(), 22U);
	const auto folder = std::filesystem::temp_directory_path() / ("openblack_test_lobby_results_" + id);
	std::error_code error;
	std::filesystem::remove_all(folder, error);
	EXPECT_FALSE(ReadGameId(folder).has_value());
	ASSERT_TRUE(WriteGameId(folder, id));
	EXPECT_EQ(ReadGameId(folder), id);
	{
		std::ofstream file(folder / k_GameIdFile, std::ios::trunc);
		file << "bad id\n";
	}
	EXPECT_FALSE(ReadGameId(folder).has_value());
	std::filesystem::remove_all(folder, error);
}
