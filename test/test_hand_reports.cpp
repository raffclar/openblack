/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <MindFile.h>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Commands/PlayerCommands.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureLayers.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/GodHand.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandCreaturePress.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/HandLockedCreature.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerHands.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ShownCreatureFace.h"
#include "ECS/Components/Transform.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Simulation/CommandLog.h"

// The test builds its own locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/PlayerCommandSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"

using namespace openblack;
using namespace openblack::player_commands;

namespace
{
entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

HandReport ReportAt(glm::vec3 hand)
{
	return HandReport {.collidePoint = hand + glm::vec3(0.0f, -1.0f, 0.0f),
	                   .handPoint = hand,
	                   .camera = glm::vec3(0.0f, 100.0f, 0.0f),
	                   .focus = glm::vec3(0.0f)};
}
} // namespace

TEST(CommandLog, ALineGivesBackTheTurnAndEachCommandWithItsPlayer)
{
	const std::vector<Command> commands {
	    {.player = PlayerNames::PLAYER_ONE, .action = HandTake {.object = Entity(5)}},
	    {.player = PlayerNames::PLAYER_ONE, .action = ReportAt({1.0f, 2.0f, 3.0f})},
	    {.player = PlayerNames::PLAYER_TWO, .action = HandTap {.object = Entity(6)}},
	};
	const auto line = command_log::LineOf(42, commands);
	EXPECT_EQ(line.rfind("{\"turn\":42,\"frame\":\"", 0), 0u);
	const auto turn = command_log::ReadLine(line);
	ASSERT_TRUE(turn.has_value());
	EXPECT_EQ(turn->turn, 42u);
	ASSERT_EQ(turn->commands.size(), 3u);
	EXPECT_EQ(turn->commands[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(std::get<HandTake>(turn->commands[0].action).object, Entity(5));
	EXPECT_EQ(std::get<HandReport>(turn->commands[1].action), ReportAt({1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(turn->commands[2].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(std::get<HandTap>(turn->commands[2].action).object, Entity(6));
}

TEST(CommandLog, ALineThatIsNotATurnDoesNotRead)
{
	EXPECT_FALSE(command_log::ReadLine("").has_value());
	EXPECT_FALSE(command_log::ReadLine("{\"turn\":1,\"frame\":\"0g\"}").has_value());
	EXPECT_FALSE(command_log::ReadLine("{\"turn\":1,\"frame\":\"010\"}").has_value());
	// The frame of a checksum is not a turn
	EXPECT_FALSE(command_log::ReadLine("{\"turn\":1,\"frame\":\"0203010000000200000000000000\"}").has_value());
}

class CommandsOfATurn: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	ecs::systems::PlayerCommandSystem _commands;
	std::vector<std::pair<uint32_t, std::vector<Command>>> _heard;

	void Listen()
	{
		_commands.SetTurnListener([this](uint32_t turn, std::span<const Command> commands) {
			_heard.emplace_back(turn, std::vector<Command>(commands.begin(), commands.end()));
		});
	}
};

TEST_F(CommandsOfATurn, TheListenerHearsTheCommandsThatTheTurnDoes)
{
	Listen();
	_commands.Submit(HandTap {.object = Entity(3)});
	_commands.SendOpenBlock();
	_commands.RunSent(7);
	_commands.RunSent(8);
	ASSERT_EQ(_heard.size(), 2u);
	EXPECT_EQ(_heard[0].first, 7u);
	ASSERT_EQ(_heard[0].second.size(), 1u);
	EXPECT_EQ(std::get<HandTap>(_heard[0].second[0].action).object, Entity(3));
	// A turn with no command is heard too, with none
	EXPECT_EQ(_heard[1].first, 8u);
	EXPECT_TRUE(_heard[1].second.empty());
}

TEST_F(CommandsOfATurn, TheCommandsOfAnotherComputerWaitForTheirTurnAndComeAfterTheOwn)
{
	Listen();
	_commands.Receive(5, {{.player = PlayerNames::PLAYER_TWO, .action = HandTake {.object = Entity(9)}}});
	_commands.RunSent(4);
	_commands.Submit(HandTap {.object = Entity(1)});
	_commands.SendOpenBlock();
	_commands.RunSent(5);
	ASSERT_EQ(_heard.size(), 2u);
	EXPECT_TRUE(_heard[0].second.empty());
	ASSERT_EQ(_heard[1].second.size(), 2u);
	EXPECT_EQ(_heard[1].second[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_heard[1].second[1].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(std::get<HandTake>(_heard[1].second[1].action).object, Entity(9));
}

/// A mind that only keeps the actions forced on its creatures
class ReactingMinds final: public ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity creature, size_t animation, bool mirrored, double interruptsAfter) override
	{
		forced.push_back(
		    {.creature = creature, .animation = animation, .mirrored = mirrored, .interruptsAfter = interruptsAfter});
		return true;
	}
	[[nodiscard]] bool CanForceAction(entt::entity, size_t, bool, double) const override { return bodyTakes; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void ChangeDesires(entt::entity, const std::function<void(creature_desires::Desires&)>&) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	bool ForceActivity(entt::entity, const ForcedActivity&) override { return false; }

	struct Forced
	{
		entt::entity creature;
		size_t animation;
		bool mirrored;
		double interruptsAfter;
	};
	std::vector<Forced> forced;
	/// Whether the body takes a reaction now
	bool bodyTakes {true};
};

class ReportedHands: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_hand = registry.Create();
		registry.Assign<ecs::components::Hand>(_hand, false);
		registry.Assign<ecs::components::Transform>(_hand, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::HandStillness>(_hand);
		registry.Assign<ecs::components::ReportedHand>(_hand);
		_hands.FindAfterLoad();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	void DrawHandAt(glm::vec3 at) { Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_hand).position = at; }

	ecs::systems::HandSystem _hands;
	entt::entity _hand {entt::null};
};

TEST_F(ReportedHands, TheWorldHasNoHandBeforeTheFirstReport)
{
	EXPECT_FALSE(_hands.GetPlayerHandPositions()[0].has_value());
	// The frames draw it all the same
	EXPECT_EQ(_hands.GetDrawnHandPosition(), glm::vec3(10.0f, 0.0f, 10.0f));
}

TEST_F(ReportedHands, TheWorldHasTheHandWhereTheLastReportPutIt)
{
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 1.0f, 30.0f}));
	DrawHandAt({500.0f, 0.0f, 500.0f});
	EXPECT_EQ(_hands.GetPlayerHandPositions()[0], glm::vec3(20.0f, 1.0f, 30.0f));
	const auto& reported = Locator::entitiesRegistry::value().Get<const ecs::components::ReportedHand>(_hand);
	EXPECT_EQ(reported.collidePoint, glm::vec3(20.0f, 0.0f, 30.0f));
	EXPECT_EQ(reported.camera, glm::vec3(0.0f, 100.0f, 0.0f));
}

TEST_F(ReportedHands, TheTurnCountsTheStillnessOfTheReportedHandNotOfTheDrawnHand)
{
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 0.0f, 20.0f}));
	_hands.ProcessTurn();
	// The drawn hand moves far each turn, the reported one stays
	for (int turn = 0; turn < 3; ++turn)
	{
		DrawHandAt({100.0f * static_cast<float>(turn), 0.0f, 0.0f});
		_hands.ProcessTurn();
	}
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_ONE), 3u);
	// A report far away is a move
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({60.0f, 0.0f, 20.0f}));
	_hands.ProcessTurn();
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_ONE), 0u);
}

// In a network game the turn keeps the thing under the cursor from the last pick, whatever the reports of the hand do
TEST_F(ReportedHands, TheWorldHasTheThingUnderTheCursorFromTheLastPick)
{
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_ONE), entt::entity {entt::null});
	const auto creature = registry.Create();
	_hands.ReportPick(PlayerNames::PLAYER_ONE, creature);
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_ONE), creature);
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 1.0f, 30.0f}));
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_ONE), creature);
	EXPECT_EQ(_hands.GetPlayerHandPositions()[0], glm::vec3(20.0f, 1.0f, 30.0f));
	_hands.ReportPick(PlayerNames::PLAYER_ONE, entt::null);
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_ONE), entt::entity {entt::null});
}

// Each god of a network game has its own pick and its own still hand. A player with no hands has neither.
TEST_F(ReportedHands, EachGodHasItsOwnPickAndItsOwnStillHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = registry.Create();
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_TWO), entt::entity {entt::null});
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_TWO), 0u);
	std::ignore = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	_hands.ReportPick(PlayerNames::PLAYER_TWO, creature);
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_TWO), creature);
	EXPECT_EQ(_hands.GetReportedPickOf(PlayerNames::PLAYER_ONE), entt::entity {entt::null});

	// The hand of the second god stays, and the hand of the first god moves each turn
	_hands.Report(PlayerNames::PLAYER_TWO, ReportAt({20.0f, 0.0f, 20.0f}));
	_hands.ProcessTurn();
	for (int turn = 1; turn <= 3; ++turn)
	{
		_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({10.0f * static_cast<float>(turn), 0.0f, 0.0f}));
		_hands.ProcessTurn();
	}
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_TWO), 3u);
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_ONE), 0u);
	EXPECT_EQ(_hands.GetHandStillTurnsOf(PlayerNames::PLAYER_THREE), 0u);
}

TEST_F(ReportedHands, TheTurnPlaysTheReactionThatAPlayerSent)
{
	auto& registry = Locator::entitiesRegistry::value();
	ReactingMinds minds;
	Locator::creatureMindSystem::reset(&minds, [](ecs::systems::CreatureMindSystemInterface* /*unowned*/) {});
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	registry.Assign<ecs::components::CreatureAnimation>(creature);
	ecs::systems::CreatureHandSystem creatures;
	creatures.DoReaction(PlayerNames::PLAYER_TWO,
	                     CreatureReaction {.creature = creature,
	                                       .animation = 166,
	                                       .mirrored = true,
	                                       .interruptsAfter = creature_feedback::k_SlapInterruptsAfter});
	ASSERT_EQ(minds.forced.size(), 1u);
	EXPECT_EQ(minds.forced[0].creature, creature);
	EXPECT_EQ(minds.forced[0].animation, 166u);
	EXPECT_TRUE(minds.forced[0].mirrored);
	EXPECT_EQ(minds.forced[0].interruptsAfter, creature_feedback::k_SlapInterruptsAfter);
	// The reaction of another computer's hand shows no face on this computer
	EXPECT_FALSE(registry.AllOf<ecs::components::ShownCreatureFace>(creature));
	EXPECT_EQ(registry.Get<const ecs::components::CreatureAnimation>(creature).face, creature_layers::FaceLayer {});
	// A creature that is gone by the turn plays nothing
	registry.Destroy(creature);
	creatures.DoReaction(PlayerNames::PLAYER_ONE, CreatureReaction {.creature = creature, .animation = 165});
	EXPECT_EQ(minds.forced.size(), 1u);
	Locator::creatureMindSystem::reset();
}

// The hand of this computer strokes the creature: the face shows at once on this computer only, and the world does not
// have it. A stroke that the body refuses sends nothing.
TEST_F(ReportedHands, AStrokeShowsItsFaceOnlyHereAndARefusedStrokeSendsNothing)
{
	auto& registry = Locator::entitiesRegistry::value();
	ReactingMinds minds;
	Locator::creatureMindSystem::reset(&minds, [](ecs::systems::CreatureMindSystemInterface* /*unowned*/) {});
	ecs::systems::PlayerCommandSystem commands;
	Locator::playerCommandSystem::reset(&commands, [](ecs::systems::PlayerCommandSystemInterface* /*unowned*/) {});
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	registry.Assign<ecs::components::CreatureAnimation>(creature);
	const creature_face::Request grimace {
	    .face = creature_face::Face::Grimace, .milliseconds = 2000.0f, .cue = creature_face::Cue::Grimace};
	ecs::systems::CreatureHandSystem creatures;

	minds.bodyTakes = false;
	EXPECT_FALSE(creatures.React(creature, 158, false, grimace, creature_feedback::k_StrokeInterruptsAfter));
	EXPECT_EQ(commands.PendingCount(), 0u);
	const auto* shown = registry.TryGet<const ecs::components::ShownCreatureFace>(creature);
	ASSERT_NE(shown, nullptr);
	EXPECT_EQ(shown->face.wanted, creature_face::AnimationOf(creature_face::Face::Grimace));
	EXPECT_EQ(shown->face.remainingMs, 2000.0f);
	EXPECT_EQ(registry.Get<const ecs::components::CreatureAnimation>(creature).face, creature_layers::FaceLayer {});

	// The body takes the next stroke: the command has the body action, and the face stays on this computer
	minds.bodyTakes = true;
	EXPECT_TRUE(creatures.React(creature, 158, false, grimace, creature_feedback::k_StrokeInterruptsAfter));
	EXPECT_EQ(commands.PendingCount(), 1u);
	EXPECT_EQ(registry.Get<const ecs::components::CreatureAnimation>(creature).face, creature_layers::FaceLayer {});
	EXPECT_TRUE(minds.forced.empty());
	Locator::playerCommandSystem::reset();
	Locator::creatureMindSystem::reset();
}

TEST_F(ReportedHands, TheGodOfAnotherPlayerGetsHandsOfItsOwnAndItsReportsMoveOnlyThem)
{
	// Before the command of the game server the player has no hands, and its reports do nothing
	EXPECT_FALSE(_hands.GetHandsOf(PlayerNames::PLAYER_TWO).has_value());
	_hands.Report(PlayerNames::PLAYER_TWO, ReportAt({5.0f, 0.0f, 5.0f}));
	EXPECT_FALSE(_hands.GetPlayerHandPositions()[0].has_value());

	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto hand : made)
	{
		ASSERT_TRUE(registry.Valid(hand));
		EXPECT_NE(hand, _hand);
		ASSERT_TRUE(registry.AllOf<ecs::components::GodHand>(hand));
		EXPECT_EQ(registry.Get<const ecs::components::GodHand>(hand).player, PlayerNames::PLAYER_TWO);
	}
	// A second command makes no more hands
	EXPECT_EQ(_hands.MakeHandsOf(PlayerNames::PLAYER_TWO), made);
	EXPECT_EQ(_hands.GetHandsOf(PlayerNames::PLAYER_ONE), _hands.GetPlayerHands());

	_hands.Report(PlayerNames::PLAYER_TWO, ReportAt({5.0f, 0.0f, 5.0f}));
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 0.0f, 20.0f}));
	EXPECT_EQ(registry.Get<const ecs::components::ReportedHand>(made[0]).handPoint, glm::vec3(5.0f, 0.0f, 5.0f));
	EXPECT_EQ(_hands.GetPlayerHandPositions()[0], glm::vec3(20.0f, 0.0f, 20.0f));
	// The turn counts the stillness of each god's hand
	for (int turn = 0; turn < 3; ++turn)
	{
		_hands.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const ecs::components::HandStillness>(made[0]).stillTurns,
	          registry.Get<const ecs::components::HandStillness>(_hand).stillTurns);
}

// The left hand of a god has its grab from the start, so that the order of the hands in the world is the same on every
// computer whichever computer first acts with a hand
TEST_F(ReportedHands, TheLeftHandOfAGodStartsWithItsGrabAndTheRightHandHasNone)
{
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	const auto hands = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_TRUE(
	    registry.AllOf<ecs::components::HandGrab>(hands[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)]));
	EXPECT_FALSE(
	    registry.AllOf<ecs::components::HandGrab>(hands[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)]));
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}

TEST_F(ReportedHands, ThisComputerMovesTheHandsOfItsOwnGodOnceTheyAreThere)
{
	_hands.SetOwnPlayer(PlayerNames::PLAYER_TWO);
	// Before the first turn it has the first player's hands to draw
	EXPECT_EQ(_hands.GetOwnHands(), _hands.GetPlayerHands());
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_hands.GetOwnHands(), made);
	// The turns still read the first player's hands
	EXPECT_EQ(_hands.GetPlayerHands()[0], _hand);
}

// Each god has its hands where its own reports put them. This computer has the hands of its own god, which the
// inspector and the frames read; the first player's hands stay the hands of the turn.
TEST_F(ReportedHands, EachGodHasItsHandWhereItsReportsPutItAndThisComputerHasItsOwn)
{
	EXPECT_FALSE(_hands.GetHandPositionsOf(PlayerNames::PLAYER_TWO)[0].has_value());
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 0.0f, 20.0f}));
	_hands.Report(PlayerNames::PLAYER_TWO, ReportAt({5.0f, 0.0f, 5.0f}));
	EXPECT_EQ(_hands.GetHandPositionsOf(PlayerNames::PLAYER_ONE)[0], glm::vec3(20.0f, 0.0f, 20.0f));
	EXPECT_EQ(_hands.GetHandPositionsOf(PlayerNames::PLAYER_TWO)[0], glm::vec3(5.0f, 0.0f, 5.0f));
	// A player with no hands has no place for them
	EXPECT_FALSE(_hands.GetHandPositionsOf(PlayerNames::PLAYER_THREE)[0].has_value());

	// The first computer has the first player's hands
	EXPECT_EQ(_hands.GetOwnHandPositions()[0], glm::vec3(20.0f, 0.0f, 20.0f));
	// The computer of the second god has the hands of that god, and the turn still has the first player's
	_hands.SetOwnPlayer(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_hands.GetOwnHands(), made);
	EXPECT_EQ(_hands.GetOwnHandPositions()[0], glm::vec3(5.0f, 0.0f, 5.0f));
	EXPECT_EQ(_hands.GetPlayerHandPositions()[0], glm::vec3(20.0f, 0.0f, 20.0f));
}

TEST_F(ReportedHands, AComputerWithNoPlayerOfItsOwnHasNoOwnPlayer)
{
	// A game on one computer: the first player is its own
	EXPECT_TRUE(_hands.IsOwnPlayer(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_hands.IsOwnPlayer(PlayerNames::PLAYER_TWO));
	// The game server and an observer: no god is their own, and the frames draw the first player's hands
	_hands.ClearOwnPlayer();
	EXPECT_FALSE(_hands.IsOwnPlayer(PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(_hands.IsOwnPlayer(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(_hands.GetOwnPlayer(), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_hands.GetOwnHands(), _hands.GetPlayerHands());
	// A joined slot gives the computer its god
	_hands.SetOwnPlayer(PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(_hands.IsOwnPlayer(PlayerNames::PLAYER_TWO));
	EXPECT_FALSE(_hands.IsOwnPlayer(PlayerNames::PLAYER_ONE));
}

TEST_F(ReportedHands, AfterALoadTheHandsOfEachGodAreFoundByTheirPlayer)
{
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_THREE);
	ecs::systems::HandSystem loaded;
	loaded.FindAfterLoad();
	EXPECT_EQ(loaded.GetPlayerHands()[0], _hand);
	EXPECT_EQ(loaded.GetHandsOf(PlayerNames::PLAYER_THREE), made);
	EXPECT_FALSE(loaded.GetHandsOf(PlayerNames::PLAYER_TWO).has_value());
}

namespace
{
/// A player entity of a name, as the land makes it
entt::entity MakePlayer(PlayerNames name)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Player>(entity, name);
	return entity;
}

/// The hands that the entity of a player links to
std::array<entt::entity, 2> LinkOf(entt::entity player)
{
	const auto& hands = Locator::entitiesRegistry::value().Get<const ecs::components::PlayerHands>(player);
	return {hands.left, hands.right};
}
} // namespace

// The hands of each god are a component of its player entity, whether the player or the hands come first
TEST_F(ReportedHands, ThePlayerEntityLinksToTheHandsOfItsGod)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The first player comes after its hands, as on a new land
	const auto first = MakePlayer(PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(registry.AllOf<ecs::components::PlayerHands>(first));
	_hands.LinkHandsOf(first);
	EXPECT_EQ(LinkOf(first), _hands.GetPlayerHands());
	EXPECT_EQ(LinkOf(first)[0], _hand);

	// The second god gets its hands after its player
	const auto second = MakePlayer(PlayerNames::PLAYER_TWO);
	_hands.LinkHandsOf(second);
	EXPECT_FALSE(registry.AllOf<ecs::components::PlayerHands>(second));
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(LinkOf(second), made);
	EXPECT_EQ(_hands.GetHandsOf(PlayerNames::PLAYER_TWO), made);

	// The world finds the hands from the link
	registry.Remove<ecs::components::GodHand>(made[0]);
	registry.Remove<ecs::components::GodHand>(made[1]);
	EXPECT_EQ(_hands.GetHandsOf(PlayerNames::PLAYER_TWO), made);
	EXPECT_EQ(_hands.GetPlayerHands()[0], _hand);
}

// The link is part of the saved world. A world from an older save gets the links at the load.
TEST_F(ReportedHands, TheLinkToTheHandsIsSavedAndAnOlderSaveGetsItAtTheLoad)
{
	const auto first = MakePlayer(PlayerNames::PLAYER_ONE);
	const auto second = MakePlayer(PlayerNames::PLAYER_TWO);
	const auto third = MakePlayer(PlayerNames::PLAYER_THREE);
	_hands.LinkHandsOf(first);
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	const auto firstHands = _hands.GetPlayerHands();
	entt::meta_ctx context;
	ecs::reflection::RegisterComponents(context);

	const auto load = [&context](const std::vector<uint8_t>& bytes) {
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& loaded = Locator::entitiesRegistry::value().Underlying();
		ASSERT_TRUE(ecs::reflection::LoadWorld(loaded, context, bytes, ecs::reflection::WorldLoadMode::Exact).has_value());
		ecs::systems::HandSystem hands;
		hands.FindAfterLoad();
	};

	// A save of this build keeps the links
	const auto saved = ecs::reflection::SaveWorld(Locator::entitiesRegistry::value().Underlying(), context);
	load(saved);
	EXPECT_EQ(LinkOf(first), firstHands);
	EXPECT_EQ(LinkOf(second), made);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<ecs::components::PlayerHands>(third));
	EXPECT_EQ(ecs::reflection::SaveWorld(Locator::entitiesRegistry::value().Underlying(), context), saved);

	// A save of an older build has no links. The load makes them from the hands of each player.
	auto& older = Locator::entitiesRegistry::value();
	older.Remove<ecs::components::PlayerHands>(first);
	older.Remove<ecs::components::PlayerHands>(second);
	load(ecs::reflection::SaveWorld(older.Underlying(), context));
	EXPECT_EQ(LinkOf(first), firstHands);
	EXPECT_EQ(LinkOf(second), made);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<ecs::components::PlayerHands>(third));
	EXPECT_EQ(ecs::reflection::SaveWorld(Locator::entitiesRegistry::value().Underlying(), context), saved);
}

TEST_F(ReportedHands, TheWorldHasNoCameraBeforeTheFirstReport)
{
	EXPECT_FALSE(_hands.GetReportedCamera(PlayerNames::PLAYER_ONE).has_value());
}

TEST_F(ReportedHands, TheWorldHasTheCameraWhereTheLastReportPutIt)
{
	auto report = ReportAt({20.0f, 1.0f, 30.0f});
	report.focus = {25.0f, 0.0f, 35.0f};
	_hands.Report(PlayerNames::PLAYER_ONE, report);
	const auto camera = _hands.GetReportedCamera(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->origin, glm::vec3(0.0f, 100.0f, 0.0f));
	EXPECT_EQ(camera->focus, glm::vec3(25.0f, 0.0f, 35.0f));
}

TEST_F(ReportedHands, EachPlayerHasTheCameraOfItsOwnReports)
{
	// A player with no hands has no camera
	EXPECT_FALSE(_hands.GetReportedCamera(PlayerNames::PLAYER_TWO).has_value());
	std::ignore = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	auto first = ReportAt({20.0f, 1.0f, 30.0f});
	first.focus = {25.0f, 0.0f, 35.0f};
	auto second = ReportAt({40.0f, 1.0f, 50.0f});
	second.camera = {7.0f, 80.0f, 9.0f};
	second.focus = {45.0f, 0.0f, 55.0f};
	_hands.Report(PlayerNames::PLAYER_ONE, first);
	_hands.Report(PlayerNames::PLAYER_TWO, second);
	const auto one = _hands.GetReportedCamera(PlayerNames::PLAYER_ONE);
	const auto two = _hands.GetReportedCamera(PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(one.has_value());
	ASSERT_TRUE(two.has_value());
	EXPECT_EQ(one->focus, glm::vec3(25.0f, 0.0f, 35.0f));
	EXPECT_EQ(two->origin, glm::vec3(7.0f, 80.0f, 9.0f));
	EXPECT_EQ(two->focus, glm::vec3(45.0f, 0.0f, 55.0f));
}

TEST_F(ReportedHands, TheWorldHasNoViewBeforeTheFirstViewReport)
{
	EXPECT_FALSE(_hands.GetReportedView(PlayerNames::PLAYER_ONE).has_value());
	// The report of the hand and the camera does not give a view
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 1.0f, 30.0f}));
	EXPECT_FALSE(_hands.GetReportedView(PlayerNames::PLAYER_ONE).has_value());
}

TEST_F(ReportedHands, TheWorldHasTheViewOfTheLastViewReport)
{
	const ViewReport first {.eye = {1.0f, 2.0f, 3.0f},
	                        .focus = {4.0f, 5.0f, 6.0f},
	                        .lens = 1.2f,
	                        .nearClip = 2.0f,
	                        .screenWidth = 1920,
	                        .screenHeight = 1080,
	                        .aspect = 1.7777778f,
	                        .inclusionDistance = 50.0f,
	                        .cameraOrders = 3,
	                        .cameraArrived = false,
	                        .gripsLand = true,
	                        .otherButton = false};
	auto second = first;
	second.screenWidth = 1280;
	second.screenHeight = 1024;
	second.aspect = 1.25f;
	second.inclusionDistance = 0.0f;
	second.cameraOrders = 4;
	second.cameraArrived = true;
	second.gripsLand = false;
	second.otherButton = true;
	_hands.ReportView(PlayerNames::PLAYER_ONE, first);
	_hands.ReportView(PlayerNames::PLAYER_ONE, second);
	const auto view = _hands.GetReportedView(PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(view.has_value());
	EXPECT_EQ(view->eye, second.eye);
	EXPECT_EQ(view->focus, second.focus);
	EXPECT_EQ(view->lens, second.lens);
	EXPECT_EQ(view->nearClip, second.nearClip);
	EXPECT_EQ(view->screen, glm::ivec2(1280, 1024));
	EXPECT_EQ(view->aspect, 1.25f);
	EXPECT_EQ(view->inclusionDistance, 0.0f);
	EXPECT_EQ(view->cameraOrders, 4u);
	EXPECT_TRUE(view->cameraArrived);
	EXPECT_FALSE(view->gripsLand);
	EXPECT_TRUE(view->otherButton);
	// A player with no hands has no view
	_hands.ReportView(PlayerNames::PLAYER_TWO, first);
	EXPECT_FALSE(_hands.GetReportedView(PlayerNames::PLAYER_TWO).has_value());
}

// The world holds the creature from the turn of the lock to the turn of its end, whatever the hand of the frames does
TEST_F(ReportedHands, TheWorldHoldsACreatureFromTheLockToItsEnd)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	ecs::systems::CreatureHandSystem creatures;
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	creatures.DoLock(PlayerNames::PLAYER_ONE, creature);
	EXPECT_EQ(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE), creature);
	// The frames hold the hand to no creature
	EXPECT_FALSE(creatures.GetCreature().has_value());
	// The end of the lock of another thing leaves the hold
	creatures.DoUnlock(PlayerNames::PLAYER_ONE, registry.Create());
	EXPECT_EQ(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE), creature);
	creatures.DoUnlock(PlayerNames::PLAYER_ONE, creature);
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	Locator::handSystem::reset();
}

// Each god holds its own creature with its own hand, and the creature knows that a hand holds it
TEST_F(ReportedHands, TheHandOfEachGodHoldsItsOwnCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	const auto own = registry.Create();
	registry.Assign<ecs::components::Creature>(own, ecs::components::Creature {.owner = PlayerNames::PLAYER_TWO});
	ecs::systems::CreatureHandSystem creatures;
	// A god with no hands yet holds nothing
	creatures.DoLock(PlayerNames::PLAYER_TWO, own);
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_TWO).has_value());
	EXPECT_FALSE(creatures.IsLocked(own));
	const auto hands = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	creatures.DoLock(PlayerNames::PLAYER_TWO, own);
	EXPECT_EQ(creatures.GetLockedCreature(PlayerNames::PLAYER_TWO), own);
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_TRUE(creatures.IsLocked(own));
	EXPECT_TRUE(registry.AllOf<ecs::components::HandLockedCreature>(hands[0]));
	EXPECT_FALSE(registry.AllOf<ecs::components::HandLockedCreature>(_hand));
	// The end of the lock from the first player leaves the hold of the second player
	creatures.DoUnlock(PlayerNames::PLAYER_ONE, own);
	EXPECT_TRUE(creatures.IsLocked(own));
	creatures.DoUnlock(PlayerNames::PLAYER_TWO, own);
	EXPECT_FALSE(creatures.IsLocked(own));
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}

// With the leash held loose, a press on the own creature waits for a tap. Only a press that lasts longer than the wait
// sends the hold, and the time of the press is no longer than the game turns allow.
TEST_F(ReportedHands, APressThatWaitsForATapHoldsOnlyAfterTheWait)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	ecs::systems::CreatureHandSystem creatures;
	creatures.WaitForTap(creature, 1000, 10);
	// The clock is far on, but only one turn went by
	creatures.WaitFrame(5000, 10);
	EXPECT_TRUE(registry.AllOf<ecs::components::HandCreaturePress>(_hand));
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	// 225 ms is not longer than the wait
	creatures.WaitFrame(1225, 12);
	EXPECT_FALSE(creatures.GetCreature().has_value());
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	// 226 ms is longer: the hand holds the creature that it pressed
	creatures.WaitFrame(1226, 12);
	EXPECT_EQ(creatures.GetCreature(), creature);
	EXPECT_EQ(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE), creature);
	EXPECT_FALSE(registry.AllOf<ecs::components::HandCreaturePress>(_hand));
	// The press does not wait any more, so its release is not a tap
	EXPECT_FALSE(creatures.ReleaseWait().has_value());
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}

// A press that waits for a tap and is let go before the wait ends is a tap on its creature, and it holds nothing
TEST_F(ReportedHands, APressLetGoWhileItWaitsIsATap)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	ecs::systems::CreatureHandSystem creatures;
	EXPECT_FALSE(creatures.ReleaseWait().has_value());
	creatures.WaitForTap(creature, 1000, 10);
	creatures.WaitFrame(1100, 10);
	EXPECT_EQ(creatures.ReleaseWait(), creature);
	// Later frames hold nothing
	creatures.WaitFrame(2000, 30);
	EXPECT_FALSE(creatures.GetCreature().has_value());
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}

// The turn does not ask again what the press asked. It holds a creature that changed after the press, and the
// creature of another player that the press sent.
TEST_F(ReportedHands, TheTurnHoldsTheCreatureThatThePressSent)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	ecs::systems::CreatureHandSystem creatures;
	const auto makeCreature = [&registry](PlayerNames owner, CreatureType species) {
		const auto creature = registry.Create();
		registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = owner, .species = species});
		return creature;
	};
	const auto frozen = makeCreature(PlayerNames::PLAYER_ONE, CreatureType::Tiger);
	registry.Assign<ecs::components::CreatureSpells>(frozen).spells[creature_spells::Spell::Freeze].phase =
	    creature_spells::Phase::Holding;
	const auto scripted = makeCreature(PlayerNames::PLAYER_ONE, CreatureType::Tiger);
	registry.Assign<ecs::components::ScriptControlled>(scripted);
	const auto notToPickUp = makeCreature(PlayerNames::PLAYER_ONE, CreatureType::Tiger);
	registry.Assign<ecs::components::CannotBePickedUp>(notToPickUp);
	const auto ogre = makeCreature(PlayerNames::PLAYER_ONE, CreatureType::Ogre);
	const auto theirs = makeCreature(PlayerNames::PLAYER_TWO, CreatureType::Tiger);
	const auto nobodys = makeCreature(PlayerNames::NEUTRAL, CreatureType::Tiger);
	for (const auto creature : {frozen, scripted, notToPickUp, ogre, theirs, nobodys})
	{
		creatures.DoLock(PlayerNames::PLAYER_ONE, creature);
		EXPECT_EQ(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE), creature);
		EXPECT_TRUE(creatures.IsLocked(creature));
		creatures.DoUnlock(PlayerNames::PLAYER_ONE, creature);
		EXPECT_FALSE(creatures.IsLocked(creature));
	}
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}

namespace
{
/// Magic that only notes the creatures whose cast beams stop
class BeamMagic final: public ecs::systems::MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType, PlayerNames, glm::vec3, const magic::SpellCastData&,
	                         const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType, PlayerNames, entt::entity, const magic::SpellCastData&,
	                          const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity) override {}
	[[nodiscard]] bool CanCastAt(MagicType, PlayerNames, glm::vec3) override { return false; }
	bool SpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	void PayForSpell(entt::entity, float) override {}
	entt::entity CreateDispenser(glm::vec3, MagicType, float) override { return entt::null; }
	void SetDispenserPeriod(entt::entity, float) override {}
	entt::entity CreateOneOffSeed(glm::vec3, SpellSeedType, int, float) override { return entt::null; }
	entt::entity CreateOneOffSeedFor(glm::vec3, MagicType) override { return entt::null; }
	bool Remove(entt::entity) override { return false; }
	entt::entity GiveSeedToHand(PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	entt::entity SummonSeed(PlayerNames, SpellSeedType, int) override { return entt::null; }
	entt::entity SummonSeedAtSite(entt::entity, SpellSeedType, int) override { return entt::null; }
	void DiscardHeldSeed() override {}
	void DiscardHeldSeedOf(PlayerNames) override {}
	void DeleteSeed(entt::entity) override {}
	void UpdateHand(const HandFrame&, const input_steps::Steps&) override {}
	bool TapFireBall(entt::entity) override { return false; }
	bool TapGlobe(entt::entity, PlayerNames) override { return false; }
	bool PressFireBall(uint32_t, uint32_t) override { return false; }
	void HoldFireBall(uint32_t, uint32_t) override {}
	void ReleaseFireBall(uint32_t, uint32_t) override {}
	[[nodiscard]] std::optional<entt::entity> OrbUnderHand() const override { return std::nullopt; }
	bool PressAction() override { return false; }
	void ReleaseAction() override {}
	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] bool IsHandBusyOf(PlayerNames) const override { return false; }
	[[nodiscard]] magic::PourPose GetHandPour(float) const override { return {}; }
	[[nodiscard]] HandCastState GetHandCastState() const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeedOf(PlayerNames) const override { return std::nullopt; }
	void SetIconSeedPowerUp(entt::entity, int) override {}
	entt::entity TakeIconSeed(entt::entity, PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	[[nodiscard]] HandResult GetLastHandResult() const override { return {}; }
	bool SendSpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	float ForcePayForSpell(entt::entity, float) override { return 0.0f; }
	[[nodiscard]] float SpellStrength(entt::entity) override { return 0.0f; }
	void ProcessTurn() override {}
	void Update(float) override {}
	void Reset() override {}
	void SetIgnoreInfluence(bool) override {}
	void DriveHand(std::optional<HandFrame>) override {}
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return std::nullopt; }
	[[nodiscard]] bool IsIgnoringInfluence() const override { return false; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override { return {}; }
	void RainOnFire(const glm::vec3&) override {}
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override { return {}; }
	void EndCreatureCastBeams(entt::entity creature) override { beamsEnded.push_back(creature); }

	std::vector<entt::entity> beamsEnded;
};
} // namespace

// The turn that holds a creature also stops the beams from its hands, as it casts from above. A refused hold does not.
TEST_F(ReportedHands, TheHoldStopsTheCastBeamsOfTheCreature)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	BeamMagic magic;
	Locator::magicSystem::reset(&magic, [](ecs::systems::MagicSystemInterface* /*unowned*/) {});
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	ecs::systems::CreatureHandSystem creatures;
	const auto caster = registry.Create();
	registry.Assign<ecs::components::Creature>(caster, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	creatures.DoLock(PlayerNames::PLAYER_ONE, caster);
	ASSERT_EQ(magic.beamsEnded.size(), 1u);
	EXPECT_EQ(magic.beamsEnded[0], caster);
	// The hand lets go. The beams do not come back, and the release stops nothing more.
	creatures.DoUnlock(PlayerNames::PLAYER_ONE, caster);
	EXPECT_EQ(magic.beamsEnded.size(), 1u);
	// The hand of another god holds the creature. The hold of the first god comes too late and stops nothing.
	std::ignore = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	creatures.DoLock(PlayerNames::PLAYER_TWO, caster);
	creatures.DoLock(PlayerNames::PLAYER_ONE, caster);
	EXPECT_EQ(magic.beamsEnded.size(), 2u);
	// A creature that is gone stops nothing
	const auto gone = registry.Create();
	registry.Assign<ecs::components::Creature>(gone, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	registry.Destroy(gone);
	creatures.DoLock(PlayerNames::PLAYER_ONE, gone);
	EXPECT_EQ(magic.beamsEnded.size(), 2u);
	Locator::magicSystem::reset();
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}

// The turn refuses a creature that is not in the world any more, and one that a hand already holds
TEST_F(ReportedHands, TheTurnRefusesAGoneCreatureAndOneThatAHandHolds)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	ecs::systems::CreatureHandSystem creatures;
	const auto gone = registry.Create();
	registry.Assign<ecs::components::Creature>(gone, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	registry.Destroy(gone);
	creatures.DoLock(PlayerNames::PLAYER_ONE, gone);
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	// A thing that is not a creature is not one that the hand holds
	creatures.DoLock(PlayerNames::PLAYER_ONE, registry.Create());
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	// The hand of the second god holds the creature first. The lock of the first player comes too late.
	const auto shared = registry.Create();
	registry.Assign<ecs::components::Creature>(shared, ecs::components::Creature {.owner = PlayerNames::PLAYER_TWO});
	const auto hands = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	creatures.DoLock(PlayerNames::PLAYER_TWO, shared);
	creatures.DoLock(PlayerNames::PLAYER_ONE, shared);
	EXPECT_EQ(creatures.GetLockedCreature(PlayerNames::PLAYER_TWO), shared);
	EXPECT_FALSE(creatures.GetLockedCreature(PlayerNames::PLAYER_ONE).has_value());
	EXPECT_FALSE(registry.AllOf<ecs::components::HandLockedCreature>(_hand));
	EXPECT_TRUE(registry.AllOf<ecs::components::HandLockedCreature>(hands[0]));
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}
