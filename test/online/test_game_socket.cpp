/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <variant>

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

	const auto god = RoundTrip(Joined {.role = Role::God, .slot = 3, .session = "s", .name = "Alice", .start = std::nullopt});
	EXPECT_EQ(god.role, Role::God);
	EXPECT_EQ(god.slot, 3);
	EXPECT_EQ(god.name, "Alice");
	const auto observer =
	    RoundTrip(Joined {.role = Role::Observer, .slot = std::nullopt, .session = "o", .name = {}, .start = std::nullopt});
	EXPECT_EQ(observer.role, Role::Observer);
	EXPECT_FALSE(observer.slot.has_value());

	EXPECT_EQ(RoundTrip(Refused {.reason = "full"}).reason, "full");
	const auto start =
	    RoundTrip(Start {.land = "Land1.txt", .testbed = false, .seed = 7, .turn = 0, .opening = "cow", .godsRingMap = false});
	EXPECT_EQ(start.land, "Land1.txt");
	EXPECT_EQ(start.seed, 7u);
	EXPECT_EQ(start.opening, "cow");
	// The testbed has no opening, and the frame leaves it out
	EXPECT_EQ(ToJson(Start {.land = {}, .testbed = true, .seed = 1, .turn = 0, .opening = {}, .godsRingMap = false}),
	          R"({"land":"","seed":1,"testbed":true,"turn":0,"type":"game.start"})");
	EXPECT_EQ(RoundTrip(Resumed {.turn = 12}).turn, 12u);
	const auto state = RoundTrip(PlayerState {.slot = 5, .state = PlayerState::State::Left});
	EXPECT_EQ(state.slot, 5);
	EXPECT_EQ(state.state, PlayerState::State::Left);
}

TEST(GameSocket, TheReplyToAJoinGivesTheServersWorld)
{
	const auto joined = RoundTrip(Joined {.role = Role::God,
	                                      .slot = 0,
	                                      .session = "s",
	                                      .name = {},
	                                      .start = Start {.land = "Playgrounds/construct.txt",
	                                                      .testbed = false,
	                                                      .seed = 9,
	                                                      .turn = 0,
	                                                      .opening = "normal",
	                                                      .godsRingMap = false}});
	ASSERT_TRUE(joined.start.has_value());
	EXPECT_EQ(joined.start->land, "Playgrounds/construct.txt");
	EXPECT_EQ(joined.start->seed, 9u);
	EXPECT_EQ(joined.start->opening, "normal");
	EXPECT_FALSE(joined.start->godsRingMap);
	// The map for 32 gods goes as a member of its own, and an old reply with no start still reads
	const auto map = RoundTrip(Start {.land = {}, .testbed = false, .seed = 2, .turn = 5, .opening = {}, .godsRingMap = true});
	EXPECT_TRUE(map.godsRingMap);
	EXPECT_EQ(map.turn, 5u);
	const auto old = Parse(R"({"type":"room.joined","role":"god","slot":1,"session":"x"})");
	ASSERT_TRUE(old.has_value());
	EXPECT_FALSE(std::get<Joined>(*old).start.has_value());
	// A start that is not an object, or has no seed, does not read
	EXPECT_FALSE(Parse(R"({"type":"room.joined","role":"god","slot":1,"session":"x","start":3})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"room.joined","role":"god","slot":1,"session":"x","start":{"land":"a"}})").has_value());
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

TEST(GameSocket, AResumeCanAskForTheWorldOfTheServer)
{
	const auto resume = RoundTrip(Resume {.session = "secret", .turn = 9, .world = true});
	EXPECT_TRUE(resume.world);
	EXPECT_FALSE(RoundTrip(Resume {.session = "secret", .turn = 9}).world);
	const auto resumed = RoundTrip(Resumed {.turn = 30, .world = true});
	EXPECT_EQ(resumed.turn, 30u);
	EXPECT_TRUE(resumed.world);
	// A resume of an older client has no world member
	EXPECT_EQ(ToJson(Resumed {.turn = 30}).find("world"), std::string::npos);
	const auto parsed = Parse(R"({"type":"room.resumed","turn":3})");
	ASSERT_TRUE(parsed.has_value());
	EXPECT_FALSE(std::get<Resumed>(*parsed).world);
}
