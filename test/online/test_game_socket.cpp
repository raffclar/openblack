/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <initializer_list>
#include <optional>
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

TEST(GameSocket, TheStartNamesTheGods)
{
	Start start {.land = {}, .testbed = true, .seed = 1, .turn = 0, .opening = {}, .godsRingMap = false};
	start.gods = {{.slot = 0, .name = "Ann"}, {.slot = 3, .name = "Bob"}};
	EXPECT_EQ(RoundTrip(start).gods, start.gods);
	EXPECT_EQ(ToJson(start), R"({"gods":[{"name":"Ann","slot":0},{"name":"Bob","slot":3}],"land":"","seed":1,)"
	                         R"("testbed":true,"turn":0,"type":"game.start"})");
	// A list that is not a list, or a god with no name or a slot past 255, does not read
	EXPECT_FALSE(Parse(R"({"type":"game.start","seed":1,"gods":3})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"game.start","seed":1,"gods":[{"slot":1}]})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"game.start","seed":1,"gods":[{"slot":256,"name":"a"}]})").has_value());
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

TEST(GameSocket, TheToolModesOfTheSoakGoInTheStartAndAreLeftOutWhenNotSet)
{
	Start tools {.land = {}, .testbed = true, .seed = 1, .turn = 0, .opening = {}, .godsRingMap = false};
	tools.allComputerGods = true;
	tools.aiSeed = 4000000000u;
	const auto read = RoundTrip(tools);
	EXPECT_TRUE(read.allComputerGods);
	ASSERT_TRUE(read.aiSeed.has_value());
	EXPECT_EQ(*read.aiSeed, 4000000000u);
	const auto plain = RoundTrip(Start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"});
	EXPECT_FALSE(plain.allComputerGods);
	EXPECT_FALSE(plain.aiSeed.has_value());
	// A seed of 0 is a seed, not a missing seed
	tools.aiSeed = 0u;
	ASSERT_TRUE(RoundTrip(tools).aiSeed.has_value());
	// A seed that is not a number does not read
	EXPECT_FALSE(Parse(R"({"type":"game.start","land":"","testbed":true,"seed":1,"turn":0,"ai_seed":"x"})").has_value());
	EXPECT_FALSE(
	    Parse(R"({"type":"game.start","land":"","testbed":true,"seed":1,"turn":0,"all_computer_gods":3})").has_value());
}

TEST(GameSocket, TheScenarioOfTheToolsGoesInTheStartAndIsLeftOutWhenNotSet)
{
	Start tools {.land = {}, .testbed = true, .seed = 1, .turn = 0, .opening = {}, .godsRingMap = false};
	tools.scenario = "mind.computer_god_rivals";
	const auto read = RoundTrip(tools);
	ASSERT_TRUE(read.scenario.has_value());
	EXPECT_EQ(*read.scenario, "mind.computer_god_rivals");
	EXPECT_FALSE(
	    RoundTrip(Start {.land = "Land1.txt", .testbed = false, .seed = 1, .turn = 0, .opening = "cow"}).scenario.has_value());
	// A scenario that is not a word, or is empty, does not read
	EXPECT_FALSE(Parse(R"({"type":"game.start","land":"","testbed":true,"seed":1,"turn":0,"scenario":3})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"game.start","land":"","testbed":true,"seed":1,"turn":0,"scenario":""})").has_value());
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
	EXPECT_EQ(toClient.alignment, 0.0f);
}

// The alignment of the start reads back as the same float, and only from -1 to 1
TEST(GameSocket, TheAlignmentOfTheStartReadsBackAsTheSameFloat)
{
	for (const float alignment : {0.7f, -0.5f, 0.002555664f, 1.0f, -1.0f, -0.0f, 1.0e-30f})
	{
		const auto creature = RoundTrip(StartCreature {.slot = 2, .file = {1}, .alignment = alignment});
		EXPECT_EQ(std::bit_cast<uint32_t>(creature.alignment), std::bit_cast<uint32_t>(alignment));
	}
	// A frame with no alignment is a god at 0
	const auto none = Parse(R"({"type":"start.creature","file":""})");
	ASSERT_TRUE(none.has_value());
	EXPECT_EQ(std::get<StartCreature>(*none).alignment, 0.0f);
	EXPECT_FALSE(Parse(R"({"type":"start.creature","file":"","alignment":1.5})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"start.creature","file":"","alignment":-1.0001})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"start.creature","file":"","alignment":"0.5"})").has_value());
	EXPECT_TRUE(Parse(R"({"type":"start.creature","file":"","alignment":-1})").has_value());
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

// The start names the players of the gods by slot, and the server tells the clients that the turns of the game stopped
TEST(GameSocket, TheNamesOfTheGodsAndTheStopOfTheGameGoToJsonAndBack)
{
	const auto start = RoundTrip(Start {.land = "Online Maps/mpm_2p_1.map",
	                                    .testbed = false,
	                                    .seed = 3,
	                                    .turn = 0,
	                                    .opening = {},
	                                    .godsRingMap = false,
	                                    .names = {"Alice", "", "Bob"}});
	EXPECT_EQ(start.names, (std::vector<std::string> {"Alice", "", "Bob"}));
	RoundTrip(StopRequest {});
	EXPECT_EQ(RoundTrip(GameStop {.turn = 1254}).turn, 1254u);
	EXPECT_FALSE(Parse(R"({"type":"game.stop"})").has_value());
	EXPECT_FALSE(Parse(R"({"type":"game.start","seed":1,"names":[1]})").has_value());
}
