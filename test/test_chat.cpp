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
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS

#include "Commands/PlayerCommands.h"
#include "ECS/ChatRules.h"
#include "ECS/Systems/Implementations/ChatSystem.h"

using namespace openblack;
using namespace openblack::chat;
using openblack::ecs::systems::ChatSystem;
using P = PlayerNames;

TEST(ChatRules, TheStaffCodeTestDropsLettersOfAPlainLine)
{
	EXPECT_EQ(WithoutStaffCode(u"Hello"), u"Hello");
	EXPECT_EQ(WithoutStaffCode(u"Sorry"), u"orry");
	EXPECT_EQ(WithoutStaffCode(u"SGRBOhi"), u"hi");
	// The letters go only in their order
	EXPECT_EQ(WithoutStaffCode(u"GS"), u"S");
	// An 'N' takes the characters to the '#', and the '#'
	EXPECT_EQ(WithoutStaffCode(u"!N123#rest"), u"rest");
	// An 'N' takes at most 19 characters after it
	EXPECT_EQ(WithoutStaffCode(u"N12345678901234567890X"), u"0X");
	EXPECT_EQ(WithoutStaffCode(u"Nice to see you"), u"");
	EXPECT_EQ(WithoutStaffCode(u"#x"), u"x");
	EXPECT_EQ(WithoutStaffCode(u""), u"");
}

TEST(ChatRules, ATauntPastTheLastIsLost)
{
	EXPECT_EQ(TauntText(0), 2137u);
	EXPECT_EQ(TauntText(30), 2167u);
	EXPECT_EQ(TauntText(50), 2187u);
	EXPECT_FALSE(TauntText(51).has_value());
}

TEST(ChatRules, NoSelectionSendsToAll)
{
	const std::vector<ListedPlayer> none {{.slot = P::PLAYER_TWO, .selected = false},
	                                      {.slot = P::PLAYER_THREE, .selected = false}};
	EXPECT_EQ(Recipients(none), (std::vector {P::PLAYER_TWO, P::PLAYER_THREE}));
	const std::vector<ListedPlayer> one {{.slot = P::PLAYER_TWO, .selected = false},
	                                     {.slot = P::PLAYER_THREE, .selected = true}};
	EXPECT_EQ(Recipients(one), (std::vector {P::PLAYER_THREE}));
	EXPECT_TRUE(Recipients({}).empty());
}

TEST(ChatRules, TheLineShowsOnlyToItsPlayers)
{
	const std::vector recipients {P::PLAYER_TWO};
	EXPECT_FALSE(ShowsHere(P::PLAYER_ONE, P::PLAYER_ONE, recipients));
	EXPECT_TRUE(ShowsHere(P::PLAYER_ONE, P::PLAYER_TWO, recipients));
	EXPECT_FALSE(ShowsHere(P::PLAYER_ONE, P::PLAYER_THREE, recipients));
	EXPECT_TRUE(ShowsHere(P::PLAYER_ONE, P::PLAYER_THREE, {}));
	EXPECT_FALSE(ShowsHere(P::PLAYER_ONE, std::nullopt, recipients));
	EXPECT_TRUE(ShowsHere(P::PLAYER_ONE, std::nullopt, {}));
	EXPECT_TRUE(CreatureSays(P::PLAYER_TWO, P::PLAYER_ONE, recipients));
	EXPECT_FALSE(CreatureSays(P::PLAYER_THREE, P::PLAYER_ONE, recipients));
	EXPECT_FALSE(CreatureSays(P::PLAYER_ONE, P::PLAYER_ONE, {}));
}

TEST(ChatRules, TheBubbleTimesOfALine)
{
	speech_bubble::Bubble bubble;
	speech_bubble::Say(bubble, {.text = u"a", .sender = P::PLAYER_ONE}, true);
	EXPECT_FLOAT_EQ(bubble.displayTime, 6.0f);
	speech_bubble::Say(bubble, {.text = u"b", .sender = P::PLAYER_ONE}, true);
	EXPECT_FLOAT_EQ(bubble.displayTime, 12.0f);
	ASSERT_EQ(bubble.lines.size(), 2);
	EXPECT_EQ(bubble.lines[0].text, u"b");

	speech_bubble::Bubble other;
	speech_bubble::Say(other, {.text = u"a", .sender = P::PLAYER_ONE}, false);
	EXPECT_FLOAT_EQ(other.displayTime, 6.0f);
	other.displayTime = 4.0f;
	speech_bubble::Say(other, {.text = u"b", .sender = P::PLAYER_ONE}, false);
	EXPECT_FLOAT_EQ(other.displayTime, 7.0f);

	// A frame of 100 game milliseconds takes 0.02 off the time
	speech_bubble::Update(other, 100.0f);
	EXPECT_FLOAT_EQ(other.displayTime, 6.98f);
	speech_bubble::Update(other, 1.0e6f);
	EXPECT_EQ(other.displayTime, 0.0f);
}

TEST(ChatRules, TheColourAndFontOfTheBubble)
{
	EXPECT_EQ(speech_bubble::LineColour(std::nullopt), 0xFFFFFFFFu);
	EXPECT_EQ(speech_bubble::LineColour(0xFFFF0000u), 0xFFFF8080u);
	// The box has half of the colour of the god, or half of the default sky blue
	EXPECT_EQ(speech_bubble::BoxColour(std::nullopt), 0xFF1D5E76u);
	EXPECT_EQ(speech_bubble::BoxColour(0xFFFF4646u), 0xFF7F2323u);
	EXPECT_EQ(speech_bubble::FontOf(0.5f), speech_bubble::Font::Good);
	EXPECT_EQ(speech_bubble::FontOf(0.333f), speech_bubble::Font::Neutral);
	EXPECT_EQ(speech_bubble::FontOf(-0.5f), speech_bubble::Font::Evil);
}

TEST(ChatRules, TheLogLineUsesTheWesternCodePage)
{
	EXPECT_EQ(LogLine(u"Ann", u"café € 中"), "Ann: caf\xE9 \x80 ?\n");
}

TEST(ChatRules, TheListOfPlayersSelects)
{
	using namespace people;
	List list;
	list.players = MakePlayers({{.slot = P::PLAYER_THREE, .name = u"C"},
	                            {.slot = P::PLAYER_TWO, .name = u"B"},
	                            {.slot = P::PLAYER_FOUR, .name = u"D"}});
	auto& players = list.players.people;
	ASSERT_EQ(players.size(), 3);
	EXPECT_EQ(players[0].slot, P::PLAYER_TWO);
	EXPECT_TRUE(std::ranges::all_of(players, &Person::selected));
	EXPECT_EQ(RowCount(list), 4);

	// A plain click on a selected player removes the selection of all
	Apply(list, {.row = 2});
	EXPECT_TRUE(std::ranges::none_of(players, &Person::selected));
	// A plain click on a player that is not selected selects only it
	Apply(list, {.row = 1});
	EXPECT_TRUE(players[0].selected);
	EXPECT_FALSE(players[1].selected);
	// Ctrl keeps the others
	Apply(list, {.row = 3, .ctrl = true});
	EXPECT_TRUE(players[0].selected);
	EXPECT_TRUE(players[2].selected);
	Apply(list, {.row = 1, .ctrl = true});
	EXPECT_FALSE(players[0].selected);
	EXPECT_TRUE(players[2].selected);
	// Shift selects from the last clicked player
	Apply(list, {.row = 3});
	Apply(list, {.row = 1});
	Apply(list, {.row = 3, .shift = true});
	EXPECT_TRUE(std::ranges::all_of(players, &Person::selected));
	EXPECT_EQ(list.lastSelected, 1u);

	// The heading collapses the list and removes the selection
	Apply(list, {.row = 0});
	EXPECT_FALSE(list.players.expanded);
	EXPECT_EQ(RowCount(list), 1);
	EXPECT_TRUE(std::ranges::none_of(players, &Person::selected));
	Apply(list, {.row = 0, .onSpeechButton = true});
	EXPECT_FALSE(list.players.messagesOn);
	EXPECT_FALSE(list.players.expanded);
	Apply(list, {.row = 0, .onSpeechButton = true});
	EXPECT_TRUE(list.players.messagesOn);
	EXPECT_TRUE(list.players.expanded);
}

namespace
{
people::List PlayersAndFriends()
{
	using namespace people;
	List list;
	list.players =
	    MakePlayers({{.slot = P::PLAYER_TWO, .user = 20u, .name = u"A"}, {.slot = P::PLAYER_THREE, .user = 30u, .name = u"B"}});
	const std::vector<FriendRecord> friends {{.user = 30, .name = u"B"}, {.user = 40, .name = u"F"}};
	LoadFriends(list, friends);
	return list;
}
} // namespace

TEST(ChatRules, TheGroupsShowInTheOrderPlayersFriendsOthers)
{
	using namespace people;
	auto list = PlayersAndFriends();
	AddOther(list, u"O", 50, std::nullopt);
	using G = Group;
	EXPECT_EQ(Rows(list), (std::vector<Row> {{G::Players, std::nullopt},
	                                         {G::Players, 0},
	                                         {G::Players, 1},
	                                         {G::Friends, std::nullopt},
	                                         {G::Friends, 0},
	                                         {G::Friends, 1},
	                                         {G::Others, std::nullopt},
	                                         {G::Others, 0}}));
	// A group with no people has no heading
	list.others.people.clear();
	EXPECT_EQ(RowCount(list), 6);
	// A friend marks the player with its number
	EXPECT_FALSE(list.players.people[0].isFriend);
	EXPECT_TRUE(list.players.people[1].isFriend);
	EXPECT_TRUE(std::ranges::all_of(list.friends.people, &Person::isFriend));
}

TEST(ChatRules, AClickOnAHeadingClearsTheSelectionOfAllGroups)
{
	using namespace people;
	auto list = PlayersAndFriends();
	// A click with Ctrl on a friend keeps the selected players
	Apply(list, {.row = 4, .ctrl = true});
	EXPECT_TRUE(list.friends.people[0].selected);
	EXPECT_TRUE(list.players.people[0].selected);
	// Shift from the friend to the first player selects the people between them, over the heading of the friends
	Apply(list, {.row = 1, .shift = true});
	EXPECT_TRUE(std::ranges::all_of(list.players.people, &Person::selected));
	EXPECT_TRUE(list.friends.people[0].selected);
	EXPECT_FALSE(list.friends.people[1].selected);
	Apply(list, {.row = 3});
	EXPECT_TRUE(std::ranges::none_of(list.players.people, &Person::selected));
	EXPECT_TRUE(std::ranges::none_of(list.friends.people, &Person::selected));
	EXPECT_FALSE(list.friends.expanded);
	EXPECT_TRUE(list.players.expanded);
}

TEST(ChatRules, TheFriendButtonNeedsPeopleOfOneKind)
{
	using namespace people;
	auto list = PlayersAndFriends();
	// Both players are selected: one is a friend and one is not
	EXPECT_EQ(FriendButtonOf(list), (FriendButton {.usable = false, .removes = false}));
	list.players.people[0].selected = false;
	EXPECT_EQ(FriendButtonOf(list), (FriendButton {.usable = true, .removes = true}));
	list.players.people[1].selected = false;
	EXPECT_EQ(FriendButtonOf(list), (FriendButton {.usable = false, .removes = false}));
	list.friends.people[1].selected = true;
	EXPECT_EQ(FriendButtonOf(list), (FriendButton {.usable = true, .removes = true}));
}

TEST(ChatRules, MakeFriendsAddsAPlayerAtTheTopAndKeepsItSelected)
{
	using namespace people;
	auto list = PlayersAndFriends();
	list.players.people[1].selected = false;
	MakeFriends(list);
	EXPECT_EQ(KeptFriends(list),
	          (std::vector<FriendRecord> {{.user = 20, .name = u"A"}, {.user = 30, .name = u"B"}, {.user = 40, .name = u"F"}}));
	EXPECT_TRUE(list.players.people[0].selected);
	EXPECT_TRUE(list.players.people[0].isFriend);
	EXPECT_FALSE(list.friends.people[0].selected);
}

TEST(ChatRules, MakeNotFriendsRemovesTheFriendOfASelectedPlayer)
{
	using namespace people;
	auto list = PlayersAndFriends();
	list.players.people[0].selected = false;
	MakeFriends(list);
	EXPECT_EQ(KeptFriends(list), (std::vector<FriendRecord> {{.user = 40, .name = u"F"}}));
	EXPECT_FALSE(list.players.people[1].selected);
	EXPECT_FALSE(list.players.people[1].isFriend);
}

TEST(ChatRules, MakeFriendsMovesAnOtherToTheFriends)
{
	using namespace people;
	auto list = PlayersAndFriends();
	for (auto& player : list.players.people)
	{
		player.selected = false;
	}
	AddOther(list, u"O", 50, std::nullopt);
	AddOther(list, u"Q", 60, std::nullopt);
	// Each new other goes to the top of the others
	EXPECT_EQ(list.others.people[0].name, u"Q");
	list.others.people[1].selected = true;
	MakeFriends(list);
	ASSERT_EQ(list.others.people.size(), 1);
	EXPECT_EQ(list.others.people[0].name, u"Q");
	EXPECT_EQ(list.friends.people[0].name, u"O");
	EXPECT_FALSE(list.friends.people[0].selected);
	EXPECT_TRUE(list.friends.people[0].isFriend);
}

TEST(ChatRules, AnOtherKeepsItsFriendMarkAfterMakeNotFriends)
{
	using namespace people;
	List list;
	const std::vector<FriendRecord> friends {{.user = 50, .name = u"O"}};
	LoadFriends(list, friends);
	list.others.people.push_back({.user = 50u, .name = u"O"});
	MarkFriends(list);
	list.friends.people[0].selected = true;
	MakeFriends(list);
	EXPECT_TRUE(list.friends.people.empty());
	EXPECT_TRUE(list.others.people[0].isFriend);
}

TEST(ChatRules, TheOthersTakeNoFriendAndNotThePlayerHimself)
{
	using namespace people;
	auto list = PlayersAndFriends();
	AddOther(list, u"F", 40, std::nullopt);
	AddOther(list, u"Me", 70, 70u);
	EXPECT_TRUE(list.others.people.empty());
	AddOther(list, u"Staff", 9999999, 70u);
	AddOther(list, u"Past", 10000015, 70u);
	ASSERT_EQ(list.others.people.size(), 2);
	EXPECT_EQ(list.others.people[1].colour, k_StaffColour);
	EXPECT_EQ(list.others.people[0].colour, k_NoGodColour);
}

TEST(ChatRules, TheProfileKeepsTheFirst25Friends)
{
	using namespace people;
	std::vector<FriendRecord> records;
	for (UserId i = 0; i < 30; ++i)
	{
		records.push_back({.user = i, .name = u"N"});
	}
	List list;
	LoadFriends(list, records);
	ASSERT_EQ(list.friends.people.size(), 25);
	EXPECT_EQ(list.friends.people.front().user, 0u);
	EXPECT_EQ(list.friends.people.back().user, 24u);
	// A friend made in the game goes over the 25, and the profile keeps the first 25 from the top
	list.others.people.push_back({.user = 99u, .name = u"New", .selected = true});
	MakeFriends(list);
	EXPECT_EQ(FriendNumbers(list).size(), 26);
	const auto kept = KeptFriends(list);
	ASSERT_EQ(kept.size(), 25);
	EXPECT_EQ(kept.front().user, 99u);
	EXPECT_EQ(kept.back().user, 23u);
}

TEST(ChatRules, AFriendIsOnlineFor450SecondsAfterItWasSeen)
{
	using namespace people;
	auto list = PlayersAndFriends();
	auto& away = list.friends.people[1];
	// A friend that was never seen is not online when the clock starts
	UpdateOnline(list, {}, k_OnlineClockStart);
	EXPECT_FALSE(away.online);
	// A friend that is a player of the game is online
	EXPECT_TRUE(list.friends.people[0].online);
	const std::vector<UserId> seen {40};
	UpdateOnline(list, seen, 500000);
	EXPECT_TRUE(away.online);
	EXPECT_EQ(away.lastSeen, 500000u);
	UpdateOnline(list, {}, 500000 + k_OnlineTime - 1);
	EXPECT_TRUE(away.online);
	UpdateOnline(list, {}, 500000 + k_OnlineTime);
	EXPECT_FALSE(away.online);
	// A time later than the clock goes back to 0
	UpdateOnline(list, seen, 900000);
	UpdateOnline(list, {}, 600000);
	EXPECT_EQ(away.lastSeen, 0u);
	EXPECT_FALSE(away.online);
}

TEST(ChatRules, TheEditGrowsWithItsText)
{
	EXPECT_EQ(EditWidth(10.0f, 800), 90);
	EXPECT_EQ(EditWidth(100.0f, 800), 120);
	EXPECT_EQ(EditWidth(1000.0f, 800), 400);
}

namespace
{

/// A game of three gods with a creature each, as the chat sees it
struct FakeGame
{
	std::vector<player_commands::Action> submitted;
	std::optional<P> here {P::PLAYER_TWO};
	std::vector<std::u16string> incoming;
	std::vector<uint32_t> said;
	int yacking {0};
	std::vector<std::string> log;

	[[nodiscard]] ChatSystem::Services Services()
	{
		return {
		    .submit = [this](player_commands::Action action) { submitted.push_back(std::move(action)); },
		    .localPlayer = [this]() { return here; },
		    .creatureOf = [](P player) -> std::optional<entt::entity> {
			    if (static_cast<int>(player) < 3)
			    {
				    return static_cast<entt::entity>(100 + static_cast<int>(player));
			    }
			    return std::nullopt;
		    },
		    .players = []() { return std::vector {P::PLAYER_ONE, P::PLAYER_TWO, P::PLAYER_THREE, P::PLAYER_FOUR}; },
		    .incoming = [this](std::u16string_view line) { incoming.emplace_back(line); },
		    .tauntText = [](uint32_t text) { return text == 2140 ? std::u16string(u"Ha!") : std::u16string(u"?"); },
		    .advisorsSay = [this](uint32_t text) { said.push_back(text); },
		    .yackingRemark = [this]() { ++yacking; },
		    .log = [this](std::string_view line) { log.emplace_back(line); },
		    .names = []() { return std::map<P, std::u16string> {{P::PLAYER_ONE, u"Ann"}, {P::PLAYER_TWO, u"Bob"}}; },
		    .turn = []() { return 7u; },
		};
	}
};

entt::entity CreatureOf(P player)
{
	return static_cast<entt::entity>(100 + static_cast<int>(player));
}

} // namespace

TEST(ChatSystem, ALineGoesAsACommandAndTheAdvisorsSpeakAtTheTurn)
{
	FakeGame game;
	ChatSystem chat(game.Services());
	const std::vector recipients {P::PLAYER_ONE};
	chat.SendLine(u"", recipients);
	EXPECT_TRUE(game.submitted.empty());
	chat.SendLine(u"Shello", recipients);
	ASSERT_EQ(game.submitted.size(), 1);
	const auto& speech = std::get<player_commands::ChatSpeech>(game.submitted[0]);
	EXPECT_EQ(speech.text, u"hello");
	EXPECT_EQ(speech.recipients, recipients);
	// The one-off remark waits for the turn
	EXPECT_EQ(game.yacking, 0);
	chat.ProcessTurn();
	EXPECT_EQ(game.yacking, 1);
	chat.ProcessTurn();
	EXPECT_EQ(game.yacking, 1);

	chat.SendTaunt(3, recipients);
	ASSERT_EQ(game.submitted.size(), 2);
	EXPECT_EQ(std::get<player_commands::ChatTaunt>(game.submitted[1]).taunt, 3u);
	EXPECT_TRUE(game.said.empty());
	chat.ProcessTurn();
	EXPECT_EQ(game.said, (std::vector {2140u}));

	// With no player to get it, nothing goes, but the advisors still speak
	chat.SendLine(u"x", {});
	EXPECT_EQ(game.submitted.size(), 2);
}

TEST(ChatSystem, ALineComesInForItsPlayers)
{
	FakeGame game;
	ChatSystem chat(game.Services());
	chat.Receive(P::PLAYER_ONE, player_commands::ChatSpeech {.recipients = {P::PLAYER_TWO}, .text = u"hi"});
	EXPECT_EQ(game.incoming, (std::vector<std::u16string> {u"Ann: hi"}));
	// The sender's creature says the text alone, the creature of the player of the list says the name too
	ASSERT_NE(chat.GetBubble(CreatureOf(P::PLAYER_ONE)), nullptr);
	EXPECT_EQ(chat.GetBubble(CreatureOf(P::PLAYER_ONE))->lines[0].text, u"hi");
	EXPECT_EQ(chat.GetBubble(CreatureOf(P::PLAYER_TWO))->lines[0].text, u"Ann: hi");
	EXPECT_EQ(chat.GetBubble(CreatureOf(P::PLAYER_THREE)), nullptr);
	EXPECT_EQ(game.log, (std::vector<std::string> {"Ann: hi\n", "Ann: hi\n"}));
	ASSERT_EQ(chat.GetReceived().size(), 1);
	EXPECT_TRUE(chat.GetReceived()[0].shown);
	EXPECT_EQ(chat.GetReceived()[0].turn, 7u);

	// A line to another player does not show here
	game.incoming.clear();
	chat.Receive(P::PLAYER_ONE, player_commands::ChatSpeech {.recipients = {P::PLAYER_THREE}, .text = u"psst"});
	EXPECT_TRUE(game.incoming.empty());
	EXPECT_EQ(chat.GetBubble(CreatureOf(P::PLAYER_THREE))->lines[0].text, u"Ann: psst");

	// The sender's own line does not show on the sender's computer
	game.here = P::PLAYER_ONE;
	chat.Receive(P::PLAYER_ONE, player_commands::ChatSpeech {.recipients = {}, .text = u"all"});
	EXPECT_TRUE(game.incoming.empty());
}

TEST(ChatSystem, ATauntSpeaksOnEveryComputer)
{
	FakeGame game;
	game.here = P::PLAYER_THREE;
	ChatSystem chat(game.Services());
	chat.Receive(P::PLAYER_ONE, player_commands::ChatTaunt {.recipients = {P::PLAYER_TWO}, .taunt = 3});
	EXPECT_EQ(game.said, (std::vector {2140u}));
	EXPECT_TRUE(game.incoming.empty());
	EXPECT_EQ(chat.GetBubble(CreatureOf(P::PLAYER_TWO))->lines[0].text, u"Ann: Ha!");
	// A taunt past the last is lost
	chat.Receive(P::PLAYER_ONE, player_commands::ChatTaunt {.recipients = {}, .taunt = 51});
	EXPECT_EQ(game.said.size(), 1);
	EXPECT_EQ(chat.GetReceived().size(), 1);
}

TEST(ChatSystem, TheBubblesRunDownAndTheTestbedNamesWin)
{
	FakeGame game;
	ChatSystem chat(game.Services());
	chat.Receive(P::PLAYER_ONE, player_commands::ChatSpeech {.recipients = {}, .text = u"a"});
	auto* bubble = chat.GetBubble(CreatureOf(P::PLAYER_ONE));
	ASSERT_NE(bubble, nullptr);
	chat.UpdateFrame(5000.0f);
	EXPECT_FLOAT_EQ(bubble->displayTime, 5.0f);

	chat.SetNames({{P::PLAYER_ONE, u"Zed"}});
	EXPECT_EQ(chat.GetNames().at(P::PLAYER_ONE), u"Zed");
	chat.Reset();
	EXPECT_TRUE(chat.GetBubbles().empty());
}
