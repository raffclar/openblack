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

#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/Implementations/OwnReportSystem.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"

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

player_commands::ViewReport ViewAt(glm::vec3 eye)
{
	return player_commands::ViewReport {.eye = eye, .focus = glm::vec3(0.0f), .screenWidth = 640, .screenHeight = 480};
}
} // namespace

// The first view report always goes. The same view does not go again.
TEST(OwnReports, TheViewReportGoesOnlyWhenItChanged)
{
	ecs::systems::OwnReportSystem reports;
	SentCommands commands;

	EXPECT_TRUE(reports.SendViewReport(ViewAt(glm::vec3(1.0f)), commands));
	EXPECT_FALSE(reports.SendViewReport(ViewAt(glm::vec3(1.0f)), commands));
	ASSERT_EQ(commands.sent.size(), 1U);
	EXPECT_TRUE(std::holds_alternative<player_commands::ViewReport>(commands.sent.front()));
	EXPECT_TRUE(reports.GetState().lastViewReport.has_value());
}

// A far move of the eye sends the view again. A field other than the camera also sends it.
TEST(OwnReports, AMoveOrAnotherFieldSendsTheViewAgain)
{
	ecs::systems::OwnReportSystem reports;
	SentCommands commands;

	EXPECT_TRUE(reports.SendViewReport(ViewAt(glm::vec3(1.0f)), commands));
	EXPECT_TRUE(reports.SendViewReport(ViewAt(glm::vec3(100.0f)), commands));
	auto wider = ViewAt(glm::vec3(100.0f));
	wider.screenWidth = 800;
	EXPECT_TRUE(reports.SendViewReport(wider, commands));
	EXPECT_FALSE(reports.SendViewReport(wider, commands));
	EXPECT_EQ(commands.sent.size(), 3U);
}

// A new hand forgets the last view report, so the same view goes again
TEST(OwnReports, ANewHandSendsTheSameViewAgain)
{
	ecs::systems::OwnReportSystem reports;
	SentCommands commands;

	EXPECT_TRUE(reports.SendViewReport(ViewAt(glm::vec3(1.0f)), commands));
	reports.ForgetViewReport();
	EXPECT_FALSE(reports.GetState().lastViewReport.has_value());
	EXPECT_TRUE(reports.SendViewReport(ViewAt(glm::vec3(1.0f)), commands));
	EXPECT_EQ(commands.sent.size(), 2U);
}

// The help report is due once a turn, and only when the help is there. A turn with no help stays due later.
TEST(OwnReports, TheHelpReportIsDueOnceATurn)
{
	ecs::systems::OwnReportSystem reports;

	EXPECT_FALSE(reports.TakeHelpTurn(5, false));
	EXPECT_FALSE(reports.GetState().helpReportTurn.has_value());
	EXPECT_TRUE(reports.TakeHelpTurn(5, true));
	EXPECT_FALSE(reports.TakeHelpTurn(5, true));
	EXPECT_TRUE(reports.TakeHelpTurn(6, true));
	EXPECT_EQ(reports.GetState().helpReportTurn, 6U);
}

// The grip of the land starts with a drag and stays while the Move button is held
TEST(OwnReports, TheLandGripStaysWhileMoveIsHeld)
{
	ecs::systems::OwnReportSystem reports;

	EXPECT_FALSE(reports.StepLandGrip(false, true));
	EXPECT_TRUE(reports.StepLandGrip(true, false));
	EXPECT_TRUE(reports.StepLandGrip(false, true));
	EXPECT_FALSE(reports.StepLandGrip(false, false));
	EXPECT_FALSE(reports.StepLandGrip(false, true));
}

// A camera turn counts for the whole game turn, and the next game turn starts again
TEST(OwnReports, TheCameraTurnCountsForTheGameTurn)
{
	ecs::systems::OwnReportSystem reports;

	EXPECT_TRUE(reports.StepCameraTurned(3, true));
	EXPECT_TRUE(reports.StepCameraTurned(3, false));
	EXPECT_FALSE(reports.StepCameraTurned(4, false));
	EXPECT_EQ(reports.GetState().cameraTurnTurn, 4U);
}
