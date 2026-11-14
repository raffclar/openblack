/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApiJson.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Gui/MainLobbyChatRules.h"
#include "Gui/TextWrap.h"
#include "Online/MainLobbyRoom.h"

using namespace openblack;
using openblack::online::main_room::Line;
using openblack::online::main_room::MainLobbyRoom;

namespace
{

lobby_api::PushEvent Event(std::string_view type, std::string data, std::optional<int64_t> seq = std::nullopt)
{
	return {.seq = seq, .type = std::string(type), .time = {}, .data = std::move(data)};
}

lobby_api::PushEvent Subscribed(std::string history)
{
	return Event("subscribed", R"({"channel":"lobby","members":[{"account":{"id":7,"name":"zed"},"status":"online"},)"
	                           R"({"account":{"id":3,"name":"Ann"},"status":"online"}],"history":[)" +
	                               history + "]}");
}

lobby_api::PushEvent ChatLine(int64_t seq, int64_t from, std::string_view name, std::string_view text,
                              std::string_view channel = "lobby", bool staff = false)
{
	return Event("chat.line",
	             fmt::format(R"({{"channel":"{}","from":{{"id":{},"name":"{}"}},"text":"{}"{}}})", channel, from, name, text,
	                         staff ? R"(,"staff":true)" : ""),
	             seq);
}

std::vector<std::string> Texts(const MainLobbyRoom& room)
{
	std::vector<std::string> texts;
	for (const auto& line : room.Lines())
	{
		texts.push_back(line.name + "|" + line.text);
	}
	return texts;
}

} // namespace

TEST(MainLobbyRoom, SubscribedGivesThePeopleByNameAndTheLastLines)
{
	MainLobbyRoom room;
	ASSERT_TRUE(room.Apply(Subscribed(
	    R"({"seq":4,"time":"2026-11-14T10:00:00.000Z","from":{"id":7,"name":"zed"},"text":"hi"},)"
	    R"({"seq":5,"time":"2026-11-14T10:00:01.000Z","from":{"id":9,"name":"Boss"},"text":"Welcome","staff":true})")));
	ASSERT_EQ(room.People().size(), 2U);
	EXPECT_EQ(room.People()[0].name, "Ann");
	EXPECT_EQ(room.People()[1].name, "zed");
	EXPECT_EQ(Texts(room), (std::vector<std::string> {"zed|hi", "Boss|Welcome"}));
	EXPECT_TRUE(room.Lines().back().staff);
}

TEST(MainLobbyRoom, ALineThatTheHistoryGaveDoesNotComeAgain)
{
	MainLobbyRoom room;
	room.Apply(Subscribed(R"({"seq":5,"time":"2026-11-14T10:00:00.000Z","from":{"id":7,"name":"zed"},"text":"hi"})"));
	EXPECT_FALSE(room.Apply(ChatLine(5, 7, "zed", "hi")));
	EXPECT_TRUE(room.Apply(ChatLine(6, 3, "Ann", "hello")));
	EXPECT_EQ(Texts(room), (std::vector<std::string> {"zed|hi", "Ann|hello"}));
}

TEST(MainLobbyRoom, ANewSubscribeReplacesTheLines)
{
	MainLobbyRoom room;
	room.Apply(ChatLine(1, 3, "Ann", "old"));
	room.Apply(Subscribed(R"({"seq":8,"time":"2026-11-14T10:00:00.000Z","from":{"id":7,"name":"zed"},"text":"new"})"));
	EXPECT_EQ(Texts(room), (std::vector<std::string> {"zed|new"}));
}

TEST(MainLobbyRoom, PeopleComeAndGo)
{
	MainLobbyRoom room;
	room.Apply(Subscribed(""));
	EXPECT_TRUE(room.Apply(Event("chat.joined", R"({"channel":"lobby","account":{"id":5,"name":"bob"}})", 10)));
	EXPECT_FALSE(room.Apply(Event("chat.joined", R"({"channel":"lobby","account":{"id":5,"name":"bob"}})", 11)));
	ASSERT_EQ(room.People().size(), 3U);
	EXPECT_EQ(room.People()[1].name, "bob");
	EXPECT_TRUE(room.Apply(Event("chat.left", R"({"channel":"lobby","account":{"id":7,"name":"zed"},"reason":"x"})", 12)));
	ASSERT_EQ(room.People().size(), 2U);
	EXPECT_EQ(room.People()[1].name, "bob");
}

TEST(MainLobbyRoom, OtherChannelsAndEventsChangeNothing)
{
	MainLobbyRoom room;
	const auto version = room.Version();
	EXPECT_FALSE(room.Apply(ChatLine(1, 3, "Ann", "in a session", "lobby-abc")));
	EXPECT_FALSE(room.Apply(Event("chat.joined", R"({"channel":"server-1","account":{"id":5,"name":"bob"}})", 2)));
	EXPECT_FALSE(room.Apply(Event("subscribed", R"({"channel":"lobbies","members":[]})")));
	EXPECT_FALSE(room.Apply(Event("presence.changed", R"({"account":{"id":5,"name":"bob"},"status":"online"})", 3)));
	EXPECT_TRUE(room.Lines().empty());
	EXPECT_EQ(room.Version(), version);
}

TEST(MainLobbyRoom, ItKeepsTheNewest100Lines)
{
	MainLobbyRoom room;
	for (int i = 0; i < 105; ++i)
	{
		room.Apply(ChatLine(i + 1, 3, "Ann", std::to_string(i)));
	}
	ASSERT_EQ(room.Lines().size(), online::main_room::k_MostLines);
	EXPECT_EQ(room.Lines().front().text, "5");
	EXPECT_EQ(room.Lines().back().text, "104");
}

TEST(MainLobbyRoom, ClearForgetsTheLinesAndThePeople)
{
	MainLobbyRoom room;
	room.Apply(Subscribed(R"({"seq":5,"time":"2026-11-14T10:00:00.000Z","from":{"id":7,"name":"zed"},"text":"hi"})"));
	room.AddSystemLine("refused");
	EXPECT_TRUE(room.Lines().back().system);
	room.Clear();
	EXPECT_TRUE(room.Lines().empty());
	EXPECT_TRUE(room.People().empty());
	// After a clear, the old lines can come again
	EXPECT_TRUE(room.Apply(ChatLine(5, 7, "zed", "hi")));
}

TEST(MainLobbyChat, LinesHaveTheFormsAndColoursOfTheGameRoom)
{
	using namespace openblack::gui;
	const auto own = main_chat::LineItem({.account = 3, .name = "Ann", .text = "hi", .staff = false, .system = false}, 3);
	EXPECT_EQ(own.text, u"Ann> hi");
	EXPECT_EQ(own.colour, multiplayer::OwnLineColour());
	const auto other = main_chat::LineItem({.account = 7, .name = "zed", .text = "yo", .staff = false, .system = false}, 3);
	EXPECT_EQ(other.text, u"zed: yo");
	EXPECT_EQ(other.colour, glm::vec4(1.0f));
	const auto staff =
	    main_chat::LineItem({.account = 9, .name = "Boss", .text = "Welcome", .staff = true, .system = false}, 3);
	EXPECT_EQ(staff.text, u"Boss: Welcome");
	EXPECT_EQ(staff.colour, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
	const auto system = main_chat::LineItem({.account = 0, .name = {}, .text = "Not sent.", .staff = false, .system = true}, 3);
	EXPECT_EQ(system.text, u"Not sent.");
	EXPECT_EQ(system.colour, main_chat::ColourOf(main_chat::k_SystemColourBits));
}

TEST(MainLobbyChat, ALineHasAtMost254Characters)
{
	using namespace openblack::gui;
	const auto item =
	    main_chat::LineItem({.account = 7, .name = "zed", .text = std::string(300, 'x'), .staff = false, .system = false}, 3);
	EXPECT_EQ(item.text.size(), std::u16string(u"zed: ").size() + 254);
}

TEST(MainLobbyChat, ALongLineWrapsInThePanel)
{
	using namespace openblack::gui;
	// A fake font: each character is 10 units wide, and the panel's list is about 225 units wide
	const auto advance = [](char16_t /*c*/) { return 10.0f; };
	const auto item =
	    main_chat::LineItem({.account = 7, .name = "zed", .text = std::string(254, 'x'), .staff = false, .system = false}, 3);
	const auto lines = text_wrap::BreakLines(item.text, 225.0f, 1.0f, advance);
	// The name goes on a line of its own, because the word after it does not fit: then 254 characters in lines of 22
	EXPECT_EQ(lines.size(), 13U);
	for (const auto line : lines)
	{
		EXPECT_LE(line.size(), 22U);
	}
}

TEST(MainLobbyChat, PeopleHeadingAndRefusals)
{
	using namespace openblack::gui;
	EXPECT_EQ(main_chat::PeopleHeading(3), u"In the room (3)");
	EXPECT_EQ(
	    main_chat::RefusalLine({.kind = ecs::systems::LobbyErrorKind::Refused, .status = 403, .detail = {}, .code = "muted"}),
	    "An admin muted your account. The line was not sent.");
	ecs::systems::LobbyError throttled {.kind = ecs::systems::LobbyErrorKind::RateLimited, .status = 429, .code = "throttled"};
	throttled.retryAfterSeconds = 2;
	EXPECT_EQ(main_chat::RefusalLine(throttled), "Too many lines. Wait 2 seconds and send the line again.");
	EXPECT_EQ(main_chat::RefusalLine({}), "The lobby did not get the line. Send it again.");
}

TEST(MainLobbyChat, TheJsonOfTheRoom)
{
	const auto subscribed = lobby_api::ParseChannelSubscribed(
	    R"({"channel":"lobby","members":[{"account":{"id":3,"name":"Ann"},"status":"online","server":null}],)"
	    R"("history":[{"seq":4,"time":"2026-11-14T10:00:00.000Z","from":{"id":3,"name":"Ann"},"text":"hi","staff":true}]})");
	ASSERT_TRUE(subscribed.has_value());
	EXPECT_EQ(subscribed->members.size(), 1U);
	ASSERT_EQ(subscribed->history.size(), 1U);
	EXPECT_EQ(subscribed->history[0].seq, 4);
	EXPECT_TRUE(subscribed->history[0].staff);
	EXPECT_GT(subscribed->history[0].time, 0);
	// A channel with no history gives none
	const auto plain = lobby_api::ParseChannelSubscribed(R"({"channel":"lobby-abc","members":[]})");
	ASSERT_TRUE(plain.has_value());
	EXPECT_TRUE(plain->history.empty());
	const auto line = lobby_api::ParseChatLine(R"({"channel":"lobby","from":{"id":3,"name":"Ann"},"text":"hi","staff":true})");
	ASSERT_TRUE(line.has_value());
	EXPECT_TRUE(line->staff);
	const auto presence = lobby_api::ParseChatPresence(R"({"channel":"lobby","account":{"id":3,"name":"Ann"}})");
	ASSERT_TRUE(presence.has_value());
	EXPECT_EQ(presence->account.name, "Ann");
}
