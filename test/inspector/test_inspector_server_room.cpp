/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Inspector.h>
#include <gtest/gtest.h>

#include "Inspector/ServerProviders.h"
#include "Server/ServerAdmin.h"

using namespace openblack;
using inspector::Inspector;
using inspector::Json;
using inspector::QueryResult;
using inspector::Request;
using server::LockstepHost;

namespace
{

/// A god slot with a player, a secret of the session and a hash of the secret
LockstepHost::Seat TakenSeat()
{
	LockstepHost::Seat seat;
	seat.state = LockstepHost::SeatState::Taken;
	seat.connection = 4;
	seat.session = "secret-of-alice";
	seat.sessionHash = server::SessionHash {};
	seat.sessionHash->salt.fill(0xAB);
	seat.sessionHash->digest.fill(0xCD);
	seat.account = "1234";
	seat.name = "alice";
	seat.ready = true;
	seat.sessionNumber = 2;
	seat.resumes = 1;
	seat.commands = {{3, 5}, {9, 2}};
	seat.blocksAgain = 1;
	seat.compares = {.compared = 40, .same = 39, .firstDifferent = 38u, .differs = true};
	return seat;
}

server::admin::RoomReport Room()
{
	return {
	    .starting = false,
	    .started = true,
	    .paused = false,
	    .gameOver = false,
	    .replaying = false,
	    .turnsDone = 41,
	    .stoppedAfter = std::nullopt,
	    .observers = 1,
	    .refusedBlocks = 0,
	    .bans = {99},
	    .seats = {server::admin::SeatReportOf(0, TakenSeat()), server::admin::SeatReportOf(1, LockstepHost::Seat {})},
	};
}

/// A provider of one query that writes, as the edits of the world are
class FakeEditProvider final: public inspector::ProviderInterface
{
public:
	[[nodiscard]] std::string_view Name() const override { return "edit"; }
	[[nodiscard]] std::vector<inspector::QueryDescription> Describe() const override
	{
		return {{.name = "create",
		         .description = "Writes",
		         .parameters = {},
		         .kind = inspector::ResultKind::Object,
		         .needsNear = false,
		         .writes = true}};
	}
	[[nodiscard]] QueryResult Run(std::string_view /*query*/, const inspector::QueryContext& /*context*/) override
	{
		return QueryResult::Value(Json {{"done", true}});
	}
};

QueryResult Ask(const Inspector& inspector, std::string query)
{
	return inspector.Answer(Request {.id = 1, .query = std::move(query)});
}

} // namespace

TEST(InspectorServerRoom, TheReportOfASeatCopiesTheStateAndSumsTheCommands)
{
	const auto seat = server::admin::SeatReportOf(3, TakenSeat());
	EXPECT_EQ(seat.seat, 3);
	EXPECT_EQ(seat.state, "taken");
	EXPECT_EQ(seat.account, "1234");
	EXPECT_EQ(seat.name, "alice");
	EXPECT_TRUE(seat.connected);
	EXPECT_EQ(seat.commands, 7u);
	EXPECT_EQ(seat.checksumsCompared, 40u);
	EXPECT_EQ(seat.firstDifferentTurn, 38u);
	EXPECT_TRUE(seat.differs);
	EXPECT_FALSE(server::admin::SeatReportOf(0, LockstepHost::Seat {}).connected);
	EXPECT_EQ(server::admin::StateName(LockstepHost::SeatState::Away), "away");
	EXPECT_EQ(server::admin::StateName(LockstepHost::SeatState::Closed), "closed");
}

TEST(InspectorServerRoom, TheRoomHasNoSecretOfASession)
{
	const auto text = inspector::ToJson(Room()).dump();
	EXPECT_EQ(text.find("secret-of-alice"), std::string::npos);
	EXPECT_EQ(text.find("session\""), std::string::npos);
	EXPECT_EQ(text.find("salt"), std::string::npos);
	EXPECT_EQ(text.find("digest"), std::string::npos);
	EXPECT_EQ(text.find("abab"), std::string::npos);
}

TEST(InspectorServerRoom, TheQueryOfTheRoomReadsInALiveNetworkGame)
{
	Inspector inspector;
	inspector.SetNetworkGameLive([] { return true; });
	inspector.Add(std::make_unique<FakeEditProvider>());
	inspector.Add(inspector::MakeServerProvider(Room));

	const auto room = Ask(inspector, "server.room");
	ASSERT_TRUE(room.Ok()) << room.error;
	EXPECT_EQ(room.value.at("turns_done"), 41);
	EXPECT_EQ(room.value.at("stopped_after"), nullptr);
	EXPECT_EQ(room.value.at("bans"), Json::array({99}));
	ASSERT_EQ(room.value.at("seats").size(), 2u);
	EXPECT_EQ(room.value.at("seats").at(0).at("state"), "taken");
	EXPECT_EQ(room.value.at("seats").at(0).at("first_different_turn"), 38);
	EXPECT_EQ(room.value.at("seats").at(1).at("first_different_turn"), nullptr);
	// The server hosts the network game, so a query that changes the world does not run
	EXPECT_FALSE(Ask(inspector, "edit.create").Ok());
}

TEST(InspectorServerRoom, TheServerRemovesTheQueriesOfTheRoomBeforeTheHostStops)
{
	Inspector inspector;
	inspector.Add(inspector::MakeServerProvider(Room));
	ASSERT_NE(inspector.Find(inspector::k_ServerProviderName), nullptr);
	inspector.Remove(inspector::k_ServerProviderName);
	EXPECT_EQ(inspector.Find(inspector::k_ServerProviderName), nullptr);
	EXPECT_FALSE(Ask(inspector, "server.room").Ok());
	// A name with no provider changes nothing
	inspector.Remove("none");
	EXPECT_EQ(inspector.Count(), 0u);
}
