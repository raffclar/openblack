/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "Commands/WaitingCommandsFile.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Systems/Implementations/OwnReportSystem.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "Hand/LoadedReports.h"
#include "Hand/OwnHandReport.h"

using namespace openblack;

namespace
{
/// Keeps the commands that the reports send, in order
class SentCommands final: public ecs::systems::PlayerCommandSystemInterface
{
public:
	void Submit(player_commands::Action action) override { sent.push_back(std::move(action)); }
	void SendOpenBlock() override {}
	void RunSent(uint32_t /*turn*/) override {}
	void Reset() override {}
	void SetTurnListener(TurnListener /*listener*/) override {}
	void Receive(uint32_t /*turn*/, std::vector<player_commands::Command> /*commands*/) override {}
	void SetSender(PlayerNames /*player*/) override {}
	[[nodiscard]] PlayerNames GetSender() const override { return PlayerNames::PLAYER_ONE; }
	[[nodiscard]] size_t PendingCount() const override { return sent.size(); }
	void SetSource(player_commands::TurnCommandSourceInterface* /*source*/) override {}
	[[nodiscard]] bool TurnReady(uint32_t /*turn*/) const override { return true; }
	[[nodiscard]] bool HasSource() const override { return false; }

	std::vector<player_commands::Action> sent;
};

/// A hand and a camera far from the middle of the axes, so that each value moved from the report before the first
player_commands::HandReport Resting()
{
	return {
	    .collidePoint = glm::vec3(1500.0f, 10.0f, 2100.0f),
	    .handPoint = glm::vec3(1510.0f, 12.0f, 2110.0f),
	    .camera = glm::vec3(1490.0f, 30.0f, 2080.0f),
	    .focus = glm::vec3(1497.0f, 0.5f, 2097.0f),
	};
}

/// The values that the loaded world keeps of a hand that sent Resting and a view
ecs::components::ReportedHand Loaded()
{
	ecs::components::ReportedHand reported;
	const auto resting = Resting();
	reported.collidePoint = resting.collidePoint;
	reported.handPoint = resting.handPoint;
	reported.camera = resting.camera;
	reported.focus = resting.focus;
	reported.pickedObject = static_cast<entt::entity>(9);
	reported.viewReported = true;
	reported.viewEye = resting.camera;
	reported.viewFocus = resting.focus;
	reported.viewNearClip = 0.6f;
	reported.viewScreenWidth = 640;
	reported.viewScreenHeight = 480;
	reported.inclusionDistance = 1.0e10f;
	reported.gripsLand = true;
	return reported;
}

hand_report::OwnInputs RestingInputs(hand_report::OwnStart start)
{
	const auto resting = Resting();
	return {
	    .handPoint = start.handPoint,
	    .picked = static_cast<entt::entity>(9),
	    .pickPoint = resting.collidePoint,
	    .camera = resting.camera,
	    .focus = resting.focus,
	    .spellNeedsReport = [] { return false; },
	    .origin = std::nullopt,
	};
}

constexpr auto k_Hand = static_cast<entt::entity>(4);
} // namespace

// The loaded world gives the last hand report, the last pick and the last view report
TEST(LoadedReports, TheLastReportsComeFromTheLoadedWorld)
{
	const auto loaded = hand_report::FromLoadedWorld(Loaded());
	EXPECT_EQ(loaded.hand, Resting());
	EXPECT_EQ(loaded.pick, static_cast<entt::entity>(9));
	ASSERT_TRUE(loaded.view.has_value());
	EXPECT_EQ(loaded.view->eye, Resting().camera);
	EXPECT_EQ(loaded.view->nearClip, 0.6f);
	EXPECT_EQ(loaded.view->inclusionDistance, 1.0e10f);
	EXPECT_TRUE(loaded.view->gripsLand);
	EXPECT_EQ(loaded.wait, 0);
}

// A world with no view report gives no last view report
TEST(LoadedReports, AWorldWithNoViewReportGivesNoView)
{
	auto reported = Loaded();
	reported.viewReported = false;
	EXPECT_FALSE(hand_report::FromLoadedWorld(reported).view.has_value());
}

// The reports that waited at the save are the last reports. A hand report among them starts the wait after a send. The
// commands of other players do not change the reports.
TEST(LoadedReports, TheWaitingReportsOfThePlayerAreTheLastReports)
{
	auto moved = Resting();
	moved.camera.x += 10.0f;
	player_commands::ViewReport view {.eye = moved.camera, .screenWidth = 800};
	const std::vector<player_commands::Command> sent {
	    {.player = PlayerNames::PLAYER_TWO, .action = player_commands::HandPick {.object = static_cast<entt::entity>(3)}},
	    {.player = PlayerNames::PLAYER_ONE, .action = player_commands::HandPick {.object = static_cast<entt::entity>(7)}},
	    {.player = PlayerNames::PLAYER_ONE, .action = moved},
	    {.player = PlayerNames::PLAYER_ONE, .action = view},
	};
	const auto loaded = hand_report::WithWaiting(hand_report::FromLoadedWorld(Loaded()), sent, PlayerNames::PLAYER_ONE, 1);
	EXPECT_EQ(loaded.hand, moved);
	EXPECT_EQ(loaded.pick, static_cast<entt::entity>(7));
	ASSERT_TRUE(loaded.view.has_value());
	EXPECT_EQ(*loaded.view, view);
	EXPECT_EQ(loaded.wait, 1);
	// With no waiting hand report the wait stays at none
	EXPECT_EQ(hand_report::WithWaiting(hand_report::FromLoadedWorld(Loaded()), {}, PlayerNames::PLAYER_ONE, 1).wait, 0);
}

// After a load the pass of the turn of the load does not occur again. At the next turn, a hand and a camera that did
// not move from the loaded report send nothing.
TEST(LoadedReports, TheReporterContinuesFromTheLoadedReport)
{
	hand_report::OwnReporter reporter;
	reporter.ContinueAfterLoad(50, k_Hand, Resting(), static_cast<entt::entity>(9), 0);
	EXPECT_FALSE(reporter.Start(50, k_Hand, Resting().handPoint).has_value());
	const auto start = reporter.Start(51, k_Hand, Resting().handPoint);
	ASSERT_TRUE(start.has_value());
	EXPECT_FALSE(start->newHand);
	const auto sends = reporter.Finish(RestingInputs(*start));
	EXPECT_FALSE(sends.report.has_value());
	EXPECT_FALSE(sends.pick.has_value());
}

// A report that waited at the save starts the wait after a send, so the next turn waits as it does with no load
TEST(LoadedReports, TheReporterWaitsAfterAWaitingReport)
{
	hand_report::OwnReporter reporter;
	reporter.ContinueAfterLoad(50, k_Hand, Resting(), static_cast<entt::entity>(9), 1);
	auto start = reporter.Start(51, k_Hand, Resting().handPoint + glm::vec3(5.0f, 0.0f, 0.0f));
	ASSERT_TRUE(start.has_value());
	EXPECT_FALSE(reporter.Finish(RestingInputs(*start)).report.has_value());
	EXPECT_EQ(reporter.TurnsToWait(), 0);
	start = reporter.Start(52, k_Hand, Resting().handPoint + glm::vec3(5.0f, 0.0f, 0.0f));
	ASSERT_TRUE(start.has_value());
	EXPECT_TRUE(reporter.Finish(RestingInputs(*start)).report.has_value());
}

// The view report continues from the loaded view: the same view does not go again, and the help pass of the turn of the
// load does not occur again
TEST(LoadedReports, TheViewReportContinuesFromTheLoadedView)
{
	ecs::systems::OwnReportSystem reports;
	SentCommands commands;
	const auto view = hand_report::FromLoadedWorld(Loaded()).view;
	reports.ContinueViewAfterLoad(50, view);
	EXPECT_TRUE(reports.GetState().landGrip);
	EXPECT_EQ(reports.GetState().cameraTurnTurn, 50U);
	EXPECT_FALSE(reports.TakeHelpTurn(50, true));
	EXPECT_TRUE(reports.TakeHelpTurn(51, true));
	ASSERT_TRUE(view.has_value());
	EXPECT_FALSE(reports.SendViewReport(*view, commands));
	auto wider = *view;
	wider.screenWidth = 800;
	EXPECT_TRUE(reports.SendViewReport(wider, commands));
	// A world with no view report sends the first view report
	reports.ContinueViewAfterLoad(60, std::nullopt);
	EXPECT_FALSE(reports.GetState().landGrip);
	EXPECT_TRUE(reports.SendViewReport(wider, commands));
}

// The queue gives the commands that wait and takes them back, the sent commands before the open block
TEST(LoadedReports, TheQueueGivesAndTakesBackTheWaitingCommands)
{
	player_commands::CommandQueue queue;
	queue.Submit(PlayerNames::PLAYER_ONE, Resting());
	queue.Send();
	queue.Submit(PlayerNames::PLAYER_ONE, player_commands::HandPick {.object = static_cast<entt::entity>(7)});
	const auto waiting = queue.Waiting();
	ASSERT_EQ(waiting.sent.size(), 1U);
	ASSERT_EQ(waiting.open.size(), 1U);

	player_commands::CommandQueue loaded;
	loaded.Submit(PlayerNames::PLAYER_TWO, Resting());
	loaded.PutBack(waiting);
	EXPECT_EQ(loaded.OpenCount(), 1U);
	EXPECT_EQ(loaded.SentCount(), 1U);
	const auto taken = loaded.TakeSent();
	ASSERT_EQ(taken.size(), 1U);
	EXPECT_EQ(taken.front().player, PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(std::holds_alternative<player_commands::HandReport>(taken.front().action));
	EXPECT_EQ(std::get<player_commands::HandReport>(taken.front().action), Resting());
}

// The bytes of the waiting commands give back the same commands in the same parts of the queue
TEST(LoadedReports, TheWaitingCommandsReadBackFromTheirBytes)
{
	EXPECT_TRUE(waiting_commands_file::Write({}).empty());
	const player_commands::WaitingCommands waiting {
	    .sent = {{.player = PlayerNames::PLAYER_ONE, .action = Resting()},
	             {.player = PlayerNames::PLAYER_ONE, .action = player_commands::ViewReport {.eye = Resting().camera}}},
	    .open = {{.player = PlayerNames::PLAYER_ONE,
	              .action = player_commands::HandPick {.object = static_cast<entt::entity>(7)}}},
	};
	const auto bytes = waiting_commands_file::Write(waiting);
	ASSERT_FALSE(bytes.empty());
	const auto read = waiting_commands_file::Read(bytes);
	ASSERT_TRUE(read.has_value());
	ASSERT_EQ(read->sent.size(), 2U);
	ASSERT_EQ(read->open.size(), 1U);
	EXPECT_EQ(std::get<player_commands::HandReport>(read->sent[0].action), Resting());
	EXPECT_EQ(std::get<player_commands::ViewReport>(read->sent[1].action).eye, Resting().camera);
	EXPECT_EQ(std::get<player_commands::HandPick>(read->open[0].action).object, static_cast<entt::entity>(7));
	// Bytes that are not a turn give nothing
	EXPECT_FALSE(waiting_commands_file::Read(std::vector<uint8_t> {1, 2, 3}).has_value());
}
