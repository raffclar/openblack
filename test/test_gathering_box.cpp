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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/ChatRules.h"
#include "ECS/GatheringBoxRules.h"
#include "ECS/Systems/Implementations/GatheringBoxSystem.h"
#include "Online/PingServerInterface.h"

using namespace openblack;
using namespace openblack::gathering_box;
using openblack::chat::people::List;
using openblack::chat::people::Person;

namespace
{

constexpr UserId k_Ann = 1001;
constexpr UserId k_Bob = 1002;
constexpr UserId k_Cid = 1003;
constexpr UserId k_Own = 1999;

/// What the fake ping server got, and the messages that it gives
struct ServerLog
{
	bool connected {true};
	int pings {0};
	std::vector<std::vector<UserId>> asked;
	std::vector<std::pair<UserId, std::u16string>> lines;
	std::vector<std::vector<uint8_t>> reports;
	std::vector<online::PingServerInterface::Message> messages;
};

class FakePingServer final: public online::PingServerInterface
{
public:
	explicit FakePingServer(ServerLog& log)
	    : _log(log)
	{
	}
	[[nodiscard]] bool IsConnected() const override { return _log.connected; }
	void Ping() override { ++_log.pings; }
	void AskOnline(std::span<const UserId> friends) override { _log.asked.emplace_back(friends.begin(), friends.end()); }
	void SendLine(UserId user, std::u16string_view text) override { _log.lines.emplace_back(user, std::u16string(text)); }
	void SendHostReport(std::span<const uint8_t> report) override { _log.reports.emplace_back(report.begin(), report.end()); }
	[[nodiscard]] std::vector<Message> TakeMessages() override { return std::exchange(_log.messages, {}); }

private:
	ServerLog& _log;
};

Person Friend(UserId user, std::u16string name)
{
	return {.user = user, .name = std::move(name), .isFriend = true};
}

struct Fixture
{
	List people;
	ServerLog log;
	int opened {0};
	int changed {0};
	bool plain {false};
	std::optional<host_report::Facts> facts;
	std::vector<std::u16string> shown;
	std::unique_ptr<ecs::systems::GatheringBoxSystem> system;

	Fixture()
	{
		ecs::systems::GatheringBoxSystem::Services services;
		services.ownUser = []() { return std::optional<UserId>(k_Own); };
		services.showLine = [this](std::u16string_view name, std::u16string_view text) {
			shown.push_back(chat::NamedLine(name, text));
		};
		services.plainChat = [this]() { return plain; };
		services.hostFacts = [this]() { return facts; };
		system = std::make_unique<ecs::systems::GatheringBoxSystem>(std::move(services));
		system->SetBox(
		    {.people = [this]() { return &people; }, .peopleChanged = [this]() { ++changed; }, .open = [this]() { ++opened; }});
		system->SetPingServer(std::make_unique<FakePingServer>(log));
	}
};

} // namespace

TEST(GatheringBoxRules, PingClockIsDueAtOnceAndThenEach20001Milliseconds)
{
	PingClock clock;
	EXPECT_TRUE(clock.Due(5000));
	EXPECT_FALSE(clock.Due(5000 + 20000));
	EXPECT_TRUE(clock.Due(5000 + 20001));
	EXPECT_FALSE(clock.Due(5000 + 20001 + 1));
	// The time of the computer wraps
	PingClock wrapping;
	EXPECT_TRUE(wrapping.Due(0xFFFFFF00u));
	EXPECT_FALSE(wrapping.Due(0x100u));
	EXPECT_TRUE(wrapping.Due(0xFFFFFF00u + 20001u));
}

TEST(GatheringBoxRules, QuestionNeedsTheSecretWordAndOneMoreCharacter)
{
	EXPECT_FALSE(question::Parse(u"Staff", u"!SaMuElGR", k_Ann, false).has_value());
	EXPECT_FALSE(question::Parse(u"Staff", u"!samuelgrX", k_Ann, false).has_value());
	EXPECT_FALSE(question::Parse(u"Staff", u"Hello there", k_Ann, false).has_value());
	const auto yesNo = question::Parse(u"Staff", u"!SaMuElGRDo you like it?", k_Ann, false);
	ASSERT_TRUE(yesNo.has_value());
	EXPECT_EQ(yesNo->kind, question::Kind::YesNo);
	EXPECT_EQ(yesNo->text, u"Staff Do you like it?");
	EXPECT_EQ(yesNo->sender, k_Ann);
	const auto line = question::Parse(u"Staff", u"!SaMuElGR!Your town?", k_Ann, false);
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(line->kind, question::Kind::Line);
	EXPECT_EQ(line->text, u"Staff Your town?");
	// The setting makes the questions plain lines
	EXPECT_FALSE(question::Parse(u"Staff", u"!SaMuElGRDo you like it?", k_Ann, true).has_value());
}

TEST(GatheringBoxRules, QuestionKeysAndAnswers)
{
	using question::Key;
	using question::Kind;
	using question::Push;
	EXPECT_EQ(question::PushOf(Key::Return, true), Push::FocusFirstButton);
	EXPECT_EQ(question::PushOf(Key::Return, false), Push::First);
	EXPECT_EQ(question::PushOf(Key::Escape, true), Push::No);
	EXPECT_EQ(question::AnswerOf(Kind::YesNo, Push::First, u"typed"), u"YES");
	EXPECT_EQ(question::AnswerOf(Kind::YesNo, Push::No, u"typed"), u"NO");
	// A question that asks for a line sends the line from either button, Escape too
	EXPECT_EQ(question::AnswerOf(Kind::Line, Push::No, u"typed"), u"typed");
	EXPECT_EQ(question::AnswerOf(Kind::Line, Push::First, std::u16string(150, u'a')).size(), 100u);
	EXPECT_EQ(question::FirstButtonText(Kind::YesNo), 2593u);
	EXPECT_EQ(question::FirstButtonText(Kind::Line), 2596u);
}

TEST(GatheringBoxRules, ClippedLineKeeps254Characters)
{
	const std::u16string longLine(300, u'x');
	EXPECT_EQ(ClippedLine(longLine).size(), 254u);
	EXPECT_EQ(ClippedLine(u"short"), u"short");
}

TEST(GatheringBoxRules, SeePersonUpdatesEachGroupAndAddsAnOther)
{
	List list;
	list.players.people.push_back({.slot = PlayerNames::PLAYER_TWO, .user = k_Ann, .name = u"Ann"});
	list.friends.people.push_back(Friend(k_Bob, u"Bob"));
	EXPECT_FALSE(SeePerson(list, u"Annie", k_Ann, k_Own, 500000));
	EXPECT_EQ(list.players.people[0].name, u"Annie");
	// A player is not a friend, so the player goes to the others too
	ASSERT_EQ(list.others.people.size(), 1u);
	EXPECT_EQ(list.others.people[0].user, k_Ann);

	EXPECT_FALSE(SeePerson(list, u"Bobby", k_Bob, k_Own, 500000));
	EXPECT_EQ(list.friends.people[0].name, u"Bobby");
	EXPECT_TRUE(list.friends.people[0].online);
	EXPECT_EQ(list.friends.people[0].lastSeen, 500000u);
	EXPECT_EQ(list.others.people.size(), 1u);

	EXPECT_FALSE(SeePerson(list, u"Ann2", k_Ann, k_Own, 500000));
	EXPECT_EQ(list.others.people[0].name, u"Ann2");
	EXPECT_EQ(list.others.people.size(), 1u);

	// The own number does not go to the others
	EXPECT_FALSE(SeePerson(list, u"Me", k_Own, k_Own, 500000));
	EXPECT_EQ(list.others.people.size(), 1u);

	// A long name keeps 47 characters
	SeePerson(list, std::u16string(60, u'n'), k_Cid, k_Own, 500000);
	EXPECT_EQ(list.others.people[0].name.size(), 47u);
}

TEST(GatheringBoxRules, AdminExpandsTheOthersInTheStaffColour)
{
	List list;
	list.others.expanded = false;
	EXPECT_TRUE(SeePerson(list, u"Admin", k_AdminUser, k_Own, 500000));
	EXPECT_TRUE(list.others.expanded);
	ASSERT_EQ(list.others.people.size(), 1u);
	EXPECT_EQ(list.others.people[0].colour, chat::people::k_StaffColour);
	// An announcement comes as staff names, each at the top of the others
	SeePerson(list, u"Line one", 10000000, k_Own, 500000);
	SeePerson(list, u"Line two", 10000001, k_Own, 500000);
	EXPECT_EQ(list.others.people[0].name, u"Line two");
	EXPECT_EQ(list.others.people[1].colour, chat::people::k_StaffColour);
}

TEST(GatheringBoxRules, ShowsLineOnlyFromAGroupWithItsMessagesOn)
{
	List list;
	list.friends.people.push_back(Friend(k_Bob, u"Bob"));
	EXPECT_TRUE(ShowsLine(list, k_Bob));
	EXPECT_FALSE(ShowsLine(list, k_Cid));
	list.friends.messagesOn = false;
	EXPECT_FALSE(ShowsLine(list, k_Bob));
}

TEST(GatheringBoxRules, PickedFriendsAndOthersNeedAPick)
{
	List list;
	list.players.people.push_back({.slot = PlayerNames::PLAYER_TWO, .user = k_Ann, .name = u"Ann"});
	list.friends.people.push_back(Friend(k_Bob, u"Bob"));
	list.others.people.push_back({.user = k_Cid, .name = u"Cid"});
	EXPECT_TRUE(PickedFriendsAndOthers(list).empty());
	list.players.people[0].selected = true;
	EXPECT_TRUE(PickedFriendsAndOthers(list).empty());
	list.others.people[0].selected = true;
	list.friends.people[0].selected = true;
	EXPECT_EQ(PickedFriendsAndOthers(list), (std::vector<UserId> {k_Bob, k_Cid}));
}

TEST(GatheringBoxRules, HostReportWritesOverTheDecidedMark)
{
	host_report::Facts facts;
	facts.game[0] = 0xAB;
	facts.decided = true;
	facts.valueIsOne = true;
	host_report::Condition condition {.aim = 7};
	condition.players[0] = {.current = 3, .met = false};
	condition.players[1] = {.current = 9, .met = true};
	facts.conditions[2] = condition;
	const auto bytes = host_report::Encode(facts);
	ASSERT_EQ(bytes.size(), 64u + 4u + 4u + 4u + 16u);
	EXPECT_EQ(bytes[0], 0xAB);
	// The mark is 1 from the value of the game, not 2 for the decided game
	EXPECT_EQ(bytes[64], 1);
	EXPECT_EQ(bytes[68], 1u << 2);
	EXPECT_EQ(bytes[72], 7);
	EXPECT_EQ(bytes[76], 3);
	EXPECT_EQ(bytes[80], 7);
	EXPECT_EQ(bytes[84], 0);
}

TEST(GatheringBoxSystem, NothingWithNoPingServer)
{
	Fixture fixture;
	fixture.system->SetPingServer(nullptr);
	fixture.system->Update(1000);
	EXPECT_FALSE(fixture.system->HasPingServer());
	EXPECT_EQ(fixture.changed, 0);
}

TEST(GatheringBoxSystem, PingsAndAsksForTheFriends)
{
	Fixture fixture;
	fixture.system->Update(1000);
	EXPECT_EQ(fixture.log.pings, 1);
	EXPECT_TRUE(fixture.log.asked.empty());
	fixture.people.friends.people.push_back(Friend(k_Bob, u"Bob"));
	fixture.system->Update(1000 + 20000);
	EXPECT_EQ(fixture.log.pings, 1);
	fixture.system->Update(1000 + 20001);
	EXPECT_EQ(fixture.log.pings, 2);
	ASSERT_EQ(fixture.log.asked.size(), 1u);
	EXPECT_EQ(fixture.log.asked[0], std::vector<UserId> {k_Bob});
}

TEST(GatheringBoxSystem, NoConnectionStillAgesTheOnlineMarks)
{
	Fixture fixture;
	fixture.log.connected = false;
	auto bob = Friend(k_Bob, u"Bob");
	bob.online = true;
	bob.lastSeen = 0;
	fixture.people.friends.people.push_back(bob);
	fixture.system->Update(1000);
	EXPECT_EQ(fixture.log.pings, 0);
	// The online clock starts at 450000, so a friend seen at 0 is not online
	EXPECT_FALSE(fixture.people.friends.people[0].online);
}

TEST(GatheringBoxSystem, OnlineAnswerMarksTheFriends)
{
	Fixture fixture;
	fixture.people.friends.people.push_back(Friend(k_Bob, u"Bob"));
	fixture.log.messages.emplace_back(online::PingServerInterface::Online {.users = {k_Bob}});
	fixture.system->Update(1000);
	EXPECT_TRUE(fixture.people.friends.people[0].online);
	// After 450 seconds with no news, the friend is no longer online
	fixture.system->Update(1000 + 450000);
	EXPECT_FALSE(fixture.people.friends.people[0].online);
}

TEST(GatheringBoxSystem, LineShowsAndTheAdminOpensTheBox)
{
	Fixture fixture;
	fixture.log.messages.emplace_back(online::PingServerInterface::Line {.user = k_Cid, .name = u"Cid", .text = u"Hi"});
	fixture.log.messages.emplace_back(
	    online::PingServerInterface::Line {.user = k_AdminUser, .name = u"Admin", .text = u"Server news"});
	fixture.system->Update(1000);
	EXPECT_EQ(fixture.shown, (std::vector<std::u16string> {u"Cid: Hi", u"Admin: Server news"}));
	EXPECT_EQ(fixture.opened, 1);
	EXPECT_EQ(fixture.people.others.people.size(), 2u);
	// A line from a group with its messages off does not show
	fixture.people.others.messagesOn = false;
	fixture.log.messages.emplace_back(online::PingServerInterface::Line {.user = k_Cid, .name = u"Cid", .text = u"Again"});
	fixture.system->Update(1001);
	EXPECT_EQ(fixture.shown.size(), 2u);
}

TEST(GatheringBoxSystem, HereAddsWithNoLine)
{
	Fixture fixture;
	fixture.log.messages.emplace_back(online::PingServerInterface::Here {.user = k_Cid, .name = u"Cid"});
	fixture.system->Update(1000);
	EXPECT_TRUE(fixture.shown.empty());
	ASSERT_EQ(fixture.people.others.people.size(), 1u);
	EXPECT_EQ(fixture.people.others.people[0].name, u"Cid");
}

TEST(GatheringBoxSystem, QuestionGoesToTheBoxAndTheAnswerBack)
{
	Fixture fixture;
	fixture.log.messages.emplace_back(
	    online::PingServerInterface::Line {.user = k_AdminUser, .name = u"Admin", .text = u"!SaMuElGRHappy?"});
	fixture.system->Update(1000);
	// A question is not a line: it does not show, and the sender does not go to the others
	EXPECT_TRUE(fixture.shown.empty());
	EXPECT_TRUE(fixture.people.others.people.empty());
	const auto asked = fixture.system->TakeQuestion();
	ASSERT_TRUE(asked.has_value());
	EXPECT_EQ(asked->text, u"Admin Happy?");
	EXPECT_FALSE(fixture.system->TakeQuestion().has_value());
	fixture.system->Answer(*asked, question::AnswerOf(asked->kind, question::Push::First, {}));
	ASSERT_EQ(fixture.log.lines.size(), 1u);
	EXPECT_EQ(fixture.log.lines[0], std::make_pair(k_AdminUser, std::u16string(u"YES")));

	// With the setting, the question is a plain line
	fixture.plain = true;
	fixture.log.messages.emplace_back(
	    online::PingServerInterface::Line {.user = k_AdminUser, .name = u"Admin", .text = u"!SaMuElGRHappy?"});
	fixture.system->Update(1001);
	EXPECT_EQ(fixture.shown, std::vector<std::u16string> {u"Admin: !SaMuElGRHappy?"});
}

TEST(GatheringBoxSystem, LinesGoToThePickedFriendsAndOthers)
{
	Fixture fixture;
	fixture.people.friends.people.push_back(Friend(k_Bob, u"Bob"));
	fixture.people.others.people.push_back({.user = k_Cid, .name = u"Cid"});
	fixture.system->SendToPicked(u"Hello");
	EXPECT_TRUE(fixture.log.lines.empty());
	fixture.people.others.people[0].selected = true;
	fixture.system->SendToPicked(u"Hello");
	ASSERT_EQ(fixture.log.lines.size(), 1u);
	EXPECT_EQ(fixture.log.lines[0], std::make_pair(k_Cid, std::u16string(u"Hello")));
}

TEST(GatheringBoxSystem, HostReportGoesWithEachPing)
{
	Fixture fixture;
	fixture.system->Update(1000);
	EXPECT_TRUE(fixture.log.reports.empty());
	fixture.facts = host_report::Facts {};
	fixture.system->Update(1000 + 20001);
	ASSERT_EQ(fixture.log.reports.size(), 1u);
	EXPECT_EQ(fixture.log.reports[0].size(), 72u);
	fixture.system->SendHostReport();
	EXPECT_EQ(fixture.log.reports.size(), 2u);
}
