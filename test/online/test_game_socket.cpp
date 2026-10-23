/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <variant>
#include <vector>

#include <GameSocket.h>
#include <gtest/gtest.h>

using namespace openblack::game_socket;

namespace
{

template <typename Kind>
Kind RoundTrip(const Kind& value)
{
	const auto text = ToJson(value);
	const auto parsed = Parse(text);
	EXPECT_TRUE(parsed.has_value()) << text;
	EXPECT_TRUE(parsed.has_value() && std::holds_alternative<Kind>(*parsed)) << text;
	return parsed.has_value() && std::holds_alternative<Kind>(*parsed) ? std::get<Kind>(*parsed) : Kind {};
}

} // namespace

TEST(GameSocket, EachFrameGoesToJsonAndBack)
{
	const auto join = RoundTrip(Join {.ticket = "v4.public.abc", .gamePassword = "pw", .name = {}});
	EXPECT_EQ(join.ticket, "v4.public.abc");
	EXPECT_EQ(join.gamePassword, "pw");
	EXPECT_EQ(RoundTrip(Join {.ticket = {}, .gamePassword = {}, .name = "lan"}).name, "lan");

	const auto resume = RoundTrip(Resume {.session = "secret", .turn = 4321});
	EXPECT_EQ(resume.session, "secret");
	EXPECT_EQ(resume.turn, 4321u);

	EXPECT_FALSE(RoundTrip(Ready {.ready = false}).ready);
	RoundTrip(Leave {});

	const auto god = RoundTrip(Joined {.role = Role::God, .slot = 3, .session = "s", .name = "Alice"});
	EXPECT_EQ(god.role, Role::God);
	EXPECT_EQ(god.slot, 3);
	EXPECT_EQ(god.name, "Alice");
	const auto observer = RoundTrip(Joined {.role = Role::Observer, .slot = std::nullopt, .session = "o", .name = {}});
	EXPECT_EQ(observer.role, Role::Observer);
	EXPECT_FALSE(observer.slot.has_value());

	EXPECT_EQ(RoundTrip(Refused {.reason = "full"}).reason, "full");
	const auto start = RoundTrip(Start {.land = "Land1.txt", .testbed = false, .seed = 7, .turn = 0, .opening = "cow"});
	EXPECT_EQ(start.land, "Land1.txt");
	EXPECT_EQ(start.seed, 7u);
	EXPECT_EQ(start.opening, "cow");
	// The testbed has no opening, and the frame leaves it out
	EXPECT_EQ(ToJson(Start {.land = {}, .testbed = true, .seed = 1, .turn = 0, .opening = {}}),
	          R"({"land":"","seed":1,"testbed":true,"turn":0,"type":"game.start"})");
	EXPECT_EQ(RoundTrip(Resumed {.turn = 12}).turn, 12u);
	const auto state = RoundTrip(PlayerState {.slot = 5, .state = PlayerState::State::Left});
	EXPECT_EQ(state.slot, 5);
	EXPECT_EQ(state.state, PlayerState::State::Left);
}

TEST(GameSocket, TheJoinOfTheFrontEndReads)
{
	// The front end's join frame (lobby_api::RoomJoinFrame) has the same members
	const auto parsed = Parse(R"({"type":"room.join","ticket":"t","game_password":"p","name":"lan"})");
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ(std::get<Join>(*parsed).ticket, "t");
	EXPECT_EQ(std::get<Join>(*parsed).name, "lan");
}

TEST(GameSocket, BadFramesDoNotRead)
{
	EXPECT_FALSE(Parse("").has_value());
	EXPECT_FALSE(Parse("[]").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.unknown"})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.resume","session":"s"})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.resume","session":"s","turn":-1})").has_value());
	// A god always has a slot, and an observer never has one
	EXPECT_FALSE(Parse(R"({"type":"room.joined","role":"god","session":"s","slot":null})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.joined","role":"observer","session":"s","slot":1})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.joined","role":"king","session":"s","slot":1})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.joined","role":"god","session":"s","slot":32})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"player.state","slot":1,"state":"gone"})").has_value());
}

TEST(GameSocket, TheFramesOfTheStartGoToJsonAndBack)
{
	const auto query =
	    RoundTrip(StartQuery {.land = "Playgrounds/TwoGods.txt", .testbed = false, .checksum = 0xFFFFFFFF00000001ull});
	EXPECT_EQ(query.land, "Playgrounds/TwoGods.txt");
	EXPECT_EQ(query.checksum, 0xFFFFFFFF00000001ull);
	const auto reply = RoundTrip(StartReply {.have = true, .checksum = 123});
	EXPECT_TRUE(reply.have);
	EXPECT_EQ(reply.checksum, 123u);
	RoundTrip(StartCancel {});
	RoundTrip(StartStop {});
	RoundTrip(StartTransfer {});
	const auto fromClient = RoundTrip(StartCreature {.slot = std::nullopt, .file = {0, 1, 2, 255}});
	EXPECT_FALSE(fromClient.slot.has_value());
	EXPECT_EQ(fromClient.file, (std::vector<uint8_t> {0, 1, 2, 255}));
	const auto toClient = RoundTrip(StartCreature {.slot = 31, .file = {}});
	EXPECT_EQ(toClient.slot, 31);
	EXPECT_TRUE(toClient.file.empty());
}

TEST(GameSocket, ACreatureFileThatIsTooLargeDoesNotRead)
{
	const std::vector<uint8_t> largest(k_MostCreatureFileBytes, 7);
	EXPECT_TRUE(Parse(ToJson(StartCreature {.slot = std::nullopt, .file = largest})).has_value());
	const std::vector<uint8_t> tooLarge(k_MostCreatureFileBytes + 1, 7);
	EXPECT_FALSE(Parse(ToJson(StartCreature {.slot = std::nullopt, .file = tooLarge})).has_value());
	EXPECT_FALSE(Parse(R"({"type":"start.creature","file":"!!"})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"start.creature","file":"","slot":32})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"start.reply"})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"start.query","land":"Land1.txt"})").has_value());
}
