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
	const auto join = RoundTrip(Join {.ticket = "v4.public.abc", .gamePassword = "pw"});
	EXPECT_EQ(join.ticket, "v4.public.abc");
	EXPECT_EQ(join.gamePassword, "pw");

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
	const auto start = RoundTrip(Start {.land = "Land1.txt", .testbed = false, .seed = 7, .turn = 0});
	EXPECT_EQ(start.land, "Land1.txt");
	EXPECT_EQ(start.seed, 7u);
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
