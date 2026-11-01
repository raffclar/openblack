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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Online/LanDiscovery.h"
#include "Server/LanRoom.h"

using namespace openblack;
namespace lan_discovery = online::lan_discovery;

namespace
{

lobby_api::LanAnswer Answer()
{
	lobby_api::LanAnswer answer;
	answer.hostname = "Office game";
	answer.land = "Online Maps/mpm_2p_1.map";
	answer.gamemode = "openstaging";
	answer.numplayers = 1;
	answer.maxplayers = 2;
	answer.gamever = "0.1.0";
	answer.port = 4433;
	answer.players = {{.account = {.id = 0, .name = "Alice"}, .role = "god", .clan = std::nullopt}};
	return answer;
}

lobby_api::ServerEntry Entry(std::string id)
{
	lobby_api::ServerEntry entry;
	entry.id = std::move(id);
	return entry;
}

} // namespace

TEST(LanDiscovery, TheSearchIsKnownAndNothingElseIs)
{
	EXPECT_TRUE(lan_discovery::IsSearch(lan_discovery::Search()));
	const auto answer = lan_discovery::AnswerBytes(Answer());
	EXPECT_FALSE(lan_discovery::IsSearch(answer));
	const std::array<uint8_t, 3> other {'a', 'b', 'c'};
	EXPECT_FALSE(lan_discovery::IsSearch(other));
	EXPECT_FALSE(lan_discovery::ParseAnswer(lan_discovery::Search()).has_value());
}

TEST(LanDiscovery, TheAnswerGivesTheRoomBack)
{
	const auto parsed = lan_discovery::ParseAnswer(lan_discovery::AnswerBytes(Answer()));
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(parsed->hostname, "Office game");
	EXPECT_EQ(parsed->port, 4433);
	EXPECT_EQ(parsed->maxplayers, 2);
	ASSERT_EQ(parsed->players.size(), 1u);
	EXPECT_EQ(parsed->players[0].account.name, "Alice");
	// An answer with no port of a game socket is not an answer
	auto noPort = Answer();
	noPort.port = 0;
	EXPECT_FALSE(lan_discovery::ParseAnswer(lan_discovery::AnswerBytes(noPort)).has_value());
}

TEST(LanDiscovery, TheLineOfARoomHasTheAddressOfItsGameSocket)
{
	const auto entry = lan_discovery::EntryOf(Answer(), "192.168.1.20");
	EXPECT_EQ(entry.id, "lan-192.168.1.20:4433");
	EXPECT_EQ(entry.url, "wss://192.168.1.20:4433/game");
	EXPECT_EQ(entry.hostname, "Office game");
	EXPECT_EQ(entry.numplayers, 1);
	EXPECT_EQ(entry.gamever, "0.1.0");
	// A server with a computer name for its certificate gives that name in the address
	auto named = Answer();
	named.host = "game.office.lan";
	EXPECT_EQ(lan_discovery::EntryOf(named, "192.168.1.20").url, "wss://game.office.lan:4433/game");
}

TEST(LanDiscovery, TheListKeepsTheOrderAndForgetsSilentRooms)
{
	lan_discovery::ServerList list;
	list.Heard(Entry("a"), 0);
	list.Heard(Entry("b"), 1'000'000);
	auto changed = Entry("a");
	changed.hostname = "new name";
	list.Heard(changed, 2'000'000);
	ASSERT_EQ(list.Servers().size(), 2u);
	EXPECT_EQ(list.Servers()[0].hostname, "new name");
	EXPECT_EQ(list.Servers()[1].id, "b");
	list.Forget(2'000'000 + lan_discovery::k_ForgetAfterMicroseconds);
	ASSERT_EQ(list.Servers().size(), 1u);
	EXPECT_EQ(list.Servers()[0].id, "a");
	list.Clear();
	EXPECT_TRUE(list.Servers().empty());
}

TEST(LanRoom, TheAnswerListsTheGodsByTheNamesOfTheirPlayers)
{
	const server::lobby_status::Room room {.hostname = "Office game", .land = "Land1.txt", .maxGods = 2, .version = "0.1.0"};
	std::vector<server::LockstepHost::Seat> seats(3);
	seats[0].state = server::LockstepHost::SeatState::Taken;
	seats[0].name = "Alice";
	seats[1].state = server::LockstepHost::SeatState::Closed;
	seats[1].name = "Gone";
	seats[2].state = server::LockstepHost::SeatState::Away;
	seats[2].name = "Bob";
	const auto answer = server::lan_room::AnswerOf(room, false, seats, 4433, "");
	EXPECT_EQ(answer.gamemode, "openstaging");
	EXPECT_EQ(answer.numplayers, 2);
	EXPECT_EQ(answer.maxplayers, 2);
	EXPECT_EQ(answer.port, 4433);
	ASSERT_EQ(answer.players.size(), 2u);
	EXPECT_EQ(answer.players[0].account.name, "Alice");
	EXPECT_EQ(answer.players[1].account.name, "Bob");
	EXPECT_EQ(server::lan_room::AnswerOf(room, true, seats, 4433, "").gamemode, "closedplaying");
}
