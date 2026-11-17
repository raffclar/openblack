/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Common/DeveloperTools.h>
#include <Debug/ScenarioPlayer.h>
#include <Debug/SimRun.h>
#include <ECS/GameEndConditions.h>
#include <Gui/MultiplayerFrontEndRules.h>
#include <Story/NewGameChoice.h>
#include <gtest/gtest.h>

using namespace openblack::developer_tools;
using openblack::GameLanguage;

namespace
{

/// The arguments that stay after the filter, as text
std::vector<std::string> Kept(const std::vector<std::string_view>& arguments, std::span<const DeveloperOption> options)
{
	const auto filtered = FilterDeveloperOptions(arguments, options);
	std::vector<std::string> kept;
	for (const auto index : filtered.kept)
	{
		kept.emplace_back(arguments.at(index));
	}
	return kept;
}

} // namespace

TEST(DeveloperTools, ABuildWithoutTheToolsRefusesEveryAction)
{
	EXPECT_FALSE(Allows(false, false));
	EXPECT_FALSE(Allows(false, true));
	EXPECT_EQ(RefusalOf(false, false), "the build has no developer tools");
}

TEST(DeveloperTools, ALiveNetworkGameRefusesEveryActionInEveryBuild)
{
	EXPECT_FALSE(Allows(true, true));
	EXPECT_EQ(RefusalOf(true, true), "a network game is live");
}

TEST(DeveloperTools, ADeveloperBuildWithNoNetworkGameAllowsTheActions)
{
	EXPECT_TRUE(Allows(true, false));
	EXPECT_FALSE(RefusalOf(true, false).has_value());
}

TEST(DeveloperTools, TheEditorRightComesFromTheTicketInANetworkGameAndElseFromTheAccount)
{
	// In a network game only the join ticket counts, also when an account is signed in
	EXPECT_EQ(EditorRightOf(true, true, std::nullopt), EditorRight::Granted);
	EXPECT_EQ(EditorRightOf(true, false, true), EditorRight::Refused);
	EXPECT_EQ(EditorRightOf(true, true, false), EditorRight::Granted);
	// Outside a network game the signed-in account gives the right
	EXPECT_EQ(EditorRightOf(false, false, true), EditorRight::Granted);
	EXPECT_EQ(EditorRightOf(false, true, false), EditorRight::Refused);
	// With no account signed in, the right is not known
	EXPECT_EQ(EditorRightOf(false, true, std::nullopt), EditorRight::Unknown);
	for (const auto right : {EditorRight::Unknown, EditorRight::Granted, EditorRight::Refused})
	{
		EXPECT_FALSE(NameOf(right).empty());
	}
	for (const auto mode : {GuiMode::Full, GuiMode::EditorOnly, GuiMode::Dormant})
	{
		EXPECT_FALSE(NameOf(mode).empty());
	}
}

TEST(DeveloperTools, InANetworkGameOnlyAPlayerWithTheEditorClaimOpensTheEditorInEveryBuild)
{
	for (const bool built : {false, true})
	{
		EXPECT_FALSE(EditorRefusalOf(built, true, EditorRight::Granted).has_value()) << built;
		for (const auto right : {EditorRight::Refused, EditorRight::Unknown})
		{
			EXPECT_EQ(EditorRefusalOf(built, true, right),
			          "only a player with the editor claim opens the editor in a network game")
			    << built;
			EXPECT_EQ(GuiModeOf(built, true, right, DebuggerRight::Refused), GuiMode::Dormant) << built;
			EXPECT_TRUE(ShowsRefusalLine(built, true, right)) << built;
		}
		EXPECT_EQ(GuiModeOf(built, true, EditorRight::Granted, DebuggerRight::Refused), GuiMode::EditorOnly) << built;
		EXPECT_FALSE(ShowsRefusalLine(built, true, EditorRight::Granted)) << built;
	}
}

TEST(DeveloperTools, InASinglePlayerGameAnAccountWithTheEditorRightOpensTheEditor)
{
	// A build that players get: only the editor shows, and the editor opens
	EXPECT_FALSE(EditorRefusalOf(false, false, EditorRight::Granted).has_value());
	EXPECT_EQ(GuiModeOf(false, false, EditorRight::Granted, DebuggerRight::Unknown), GuiMode::EditorOnly);
	EXPECT_FALSE(ShowsRefusalLine(false, false, EditorRight::Granted));
}

TEST(DeveloperTools, InASinglePlayerGameAnAccountWithoutTheEditorRightSeesTheRefusalLine)
{
	EXPECT_EQ(EditorRefusalOf(false, false, EditorRight::Refused), "the signed-in account does not have the editor right");
	EXPECT_EQ(GuiModeOf(false, false, EditorRight::Refused, DebuggerRight::Unknown), GuiMode::Dormant);
	EXPECT_TRUE(ShowsRefusalLine(false, false, EditorRight::Refused));
	EXPECT_FALSE(k_EditorRefusedLine.empty());
}

TEST(DeveloperTools, TheRefusalLineIsInTheLanguageOfTheGameOrInEnglish)
{
	EXPECT_EQ(EditorRefusedLineOf(GameLanguage::UkEnglish), k_EditorRefusedLine);
	EXPECT_EQ(EditorRefusedLineOf(GameLanguage::UsEnglish), k_EditorRefusedLine);
	// The game's text files have no refusal line, so a language with no row of its own shows the English line
	for (const auto language : {GameLanguage::French, GameLanguage::German, GameLanguage::Japanese, GameLanguage::Thai})
	{
		const bool hasRow =
		    std::ranges::any_of(k_EditorRefusedLines, [language](const auto& row) { return row.language == language; });
		EXPECT_EQ(EditorRefusedLineOf(language) == k_EditorRefusedLine, !hasRow);
		EXPECT_FALSE(EditorRefusedLineOf(language).empty());
	}
	// Each language has one row at most
	for (const auto& row : k_EditorRefusedLines)
	{
		EXPECT_EQ(std::ranges::count(k_EditorRefusedLines, row.language, &EditorRefusedLine::language), 1);
		EXPECT_FALSE(row.line.empty());
	}
}

TEST(DeveloperTools, InASinglePlayerGameWithNoSignInTheEditorStaysClosedAndShowsNothing)
{
	// The rule from before: a build that players get has no editor outside a network game, and shows no line
	EXPECT_EQ(EditorRefusalOf(false, false, EditorRight::Unknown), "the build has no developer tools");
	EXPECT_EQ(GuiModeOf(false, false, EditorRight::Unknown, DebuggerRight::Unknown), GuiMode::Dormant);
	EXPECT_FALSE(ShowsRefusalLine(false, false, EditorRight::Unknown));
}

TEST(DeveloperTools, ADeveloperBuildOutsideANetworkGameKeepsAllItsWindows)
{
	// The editor right of the account changes nothing here
	for (const auto right : {EditorRight::Unknown, EditorRight::Granted, EditorRight::Refused})
	{
		EXPECT_FALSE(EditorRefusalOf(true, false, right).has_value());
		EXPECT_EQ(GuiModeOf(true, false, right, DebuggerRight::Unknown), GuiMode::Full);
		EXPECT_FALSE(ShowsRefusalLine(true, false, right));
	}
}

TEST(DeveloperTools, TheDebuggerRightComesFromTheTicketInANetworkGameAndElseFromTheAccount)
{
	EXPECT_EQ(DebuggerRightOf(true, true, false), DebuggerRight::Granted);
	EXPECT_EQ(DebuggerRightOf(true, false, true), DebuggerRight::Refused);
	EXPECT_EQ(DebuggerRightOf(false, false, true), DebuggerRight::Granted);
	EXPECT_EQ(DebuggerRightOf(false, true, false), DebuggerRight::Refused);
	EXPECT_EQ(DebuggerRightOf(false, true, std::nullopt), DebuggerRight::Unknown);
}

TEST(DeveloperTools, ThePlayerWithTheDebuggerRightGetsTheMenuBarInEveryBuildAndGame)
{
	for (const bool built : {false, true})
	{
		for (const bool live : {false, true})
		{
			for (const auto editor : {EditorRight::Unknown, EditorRight::Granted, EditorRight::Refused})
			{
				EXPECT_EQ(GuiModeOf(built, live, editor, DebuggerRight::Granted), GuiMode::Full) << built << live;
			}
		}
	}
}

TEST(DeveloperTools, ADeveloperBuildOutsideANetworkGameShowsTheBarWithoutTheDebuggerRight)
{
	for (const auto editor : {EditorRight::Unknown, EditorRight::Granted, EditorRight::Refused})
	{
		for (const auto debugger : {DebuggerRight::Unknown, DebuggerRight::Granted, DebuggerRight::Refused})
		{
			EXPECT_EQ(GuiModeOf(true, false, editor, debugger), GuiMode::Full);
		}
	}
	// A network game and a build that players get keep the rule of the debugger right
	EXPECT_EQ(GuiModeOf(true, true, EditorRight::Granted, DebuggerRight::Refused), GuiMode::EditorOnly);
	EXPECT_EQ(GuiModeOf(false, false, EditorRight::Granted, DebuggerRight::Refused), GuiMode::EditorOnly);
}

TEST(DeveloperTools, ThePlayerWithoutTheDebuggerRightGetsOnlyTheEditorOfTheEditorRight)
{
	for (const bool built : {false, true})
	{
		EXPECT_EQ(GuiModeOf(built, true, EditorRight::Granted, DebuggerRight::Refused), GuiMode::EditorOnly);
		EXPECT_EQ(GuiModeOf(built, true, EditorRight::Refused, DebuggerRight::Refused), GuiMode::Dormant);
	}
	EXPECT_EQ(GuiModeOf(false, false, EditorRight::Granted, DebuggerRight::Refused), GuiMode::EditorOnly);
	EXPECT_EQ(GuiModeOf(false, false, EditorRight::Refused, DebuggerRight::Refused), GuiMode::Dormant);
}

TEST(DeveloperTools, InTheFullModeTheEditorShowsOnlyWithTheEditorRightAndNoOtherWindowInANetworkGame)
{
	// A single player game: the debug windows show, and the editor shows only when it can open
	EXPECT_TRUE(ShowsWindow(GuiMode::Full, false, false, false));
	EXPECT_TRUE(ShowsWindow(GuiMode::Full, false, true, true));
	EXPECT_FALSE(ShowsWindow(GuiMode::Full, false, true, false));
	// A network game: no debug window other than the editor shows, so that the shared world stays the same
	EXPECT_FALSE(ShowsWindow(GuiMode::Full, true, false, true));
	EXPECT_TRUE(ShowsWindow(GuiMode::Full, true, true, true));
	EXPECT_FALSE(ShowsWindow(GuiMode::Full, true, true, false));
	for (const bool live : {false, true})
	{
		EXPECT_TRUE(ShowsWindow(GuiMode::EditorOnly, live, true, true));
		EXPECT_FALSE(ShowsWindow(GuiMode::EditorOnly, live, false, true));
		EXPECT_FALSE(ShowsWindow(GuiMode::Dormant, live, true, true));
		EXPECT_FALSE(ShowsWindow(GuiMode::Dormant, live, false, true));
	}
}

TEST(DeveloperTools, AWindowThatShowsInANetworkGameShowsThereOnlyInTheFullMode)
{
	// A window that changes nothing of the shared world, or that sends its changes as commands, shows in a network game
	EXPECT_TRUE(ShowsWindow(GuiMode::Full, true, false, false, true));
	EXPECT_TRUE(ShowsWindow(GuiMode::Full, false, false, false, true));
	EXPECT_FALSE(ShowsWindow(GuiMode::Full, true, false, false, false));
	// The rule of the editor does not change
	EXPECT_FALSE(ShowsWindow(GuiMode::Full, true, true, false, true));
	// A player with only the editor right, or with no right, does not get the window
	EXPECT_FALSE(ShowsWindow(GuiMode::EditorOnly, true, false, true, true));
	EXPECT_FALSE(ShowsWindow(GuiMode::Dormant, true, false, true, true));
}

TEST(DeveloperTools, TheMenuBarShowsOnlyInTheFullModeWhileTheGameMenuIsOpen)
{
	EXPECT_TRUE(ShowsMenuBar(GuiMode::Full, true, true));
	EXPECT_FALSE(ShowsMenuBar(GuiMode::Full, true, false));
	// With no game's menu, the bar is the only menu
	EXPECT_TRUE(ShowsMenuBar(GuiMode::Full, false, false));
	for (const auto mode : {GuiMode::EditorOnly, GuiMode::Dormant})
	{
		EXPECT_FALSE(ShowsMenuBar(mode, true, true));
		EXPECT_FALSE(ShowsMenuBar(mode, false, false));
	}
}

TEST(DeveloperTools, EveryActionHasAName)
{
	for (const auto action :
	     {Action::DebugWindow, Action::EditorWrite, Action::InspectorWrite, Action::ScenarioLoad, Action::DebugKey})
	{
		EXPECT_NE(NameOf(action), "developer action");
	}
}

TEST(DeveloperTools, TheFilterRemovesDeveloperOptionsWithTheirValues)
{
	const std::vector<std::string_view> arguments {
	    "openblack",      "-g", "C:/black", "--scenario", "benchmark.creatures_100", "--sim-fast", "--seed=7", "-sLand2.txt",
	    "--inspect-port", "0",  "-W",       "800"};
	const auto filtered = FilterDeveloperOptions(arguments, k_DeveloperOptions);
	EXPECT_EQ(Kept(arguments, k_DeveloperOptions), (std::vector<std::string> {"openblack", "-g", "C:/black", "-W", "800"}));
	EXPECT_EQ(filtered.ignored, (std::vector<std::string> {"--scenario benchmark.creatures_100", "--sim-fast", "--seed=7",
	                                                       "-sLand2.txt", "--inspect-port 0"}));
}

TEST(DeveloperTools, TheFilterKeepsThePlayerAndWindowOptions)
{
	const std::vector<std::string_view> arguments {"openblack",
	                                               "--background",
	                                               "-n",
	                                               "300",
	                                               "-l",
	                                               "game.log",
	                                               "--join",
	                                               "wss://127.0.0.1:1/game/a",
	                                               "--join-ticket-file",
	                                               "t",
	                                               "--backend-type",
	                                               "Vulkan",
	                                               "--lobby",
	                                               "https://example.org"};
	const auto filtered = FilterDeveloperOptions(arguments, k_DeveloperOptions);
	EXPECT_EQ(filtered.kept.size(), arguments.size());
	EXPECT_TRUE(filtered.ignored.empty());
}

TEST(DeveloperTools, TheFilterKeepsThePingDisplayOfThePlayer)
{
	// The ping display is an option of the player (deviation 143), so a build without the tools keeps it
	const std::vector<std::string_view> arguments {"openblack", "--show-ping"};
	const auto filtered = FilterDeveloperOptions(arguments, k_DeveloperOptions);
	EXPECT_EQ(filtered.kept.size(), arguments.size());
	EXPECT_TRUE(filtered.ignored.empty());
	EXPECT_FALSE(IsDeveloperOption("show-ping", k_DeveloperOptions));
}

TEST(DeveloperTools, TheFilterRemovesAShortDeveloperOptionAndItsValue)
{
	const std::vector<std::string_view> arguments {"openblack", "-s", "Land2.txt", "-u", "2"};
	EXPECT_EQ(Kept(arguments, k_DeveloperOptions), (std::vector<std::string> {"openblack"}));
}

TEST(DeveloperTools, TheFilterKeepsTheArgumentsAfterTheEndOfTheOptions)
{
	const std::vector<std::string_view> arguments {"openblack", "--", "--scenario", "x"};
	EXPECT_EQ(Kept(arguments, k_DeveloperOptions), (std::vector<std::string> {"openblack", "--", "--scenario", "x"}));
}

TEST(DeveloperTools, TheFilterKeepsAnOptionWhoseNameOnlyStartsTheSame)
{
	const std::vector<std::string_view> arguments {"openblack", "--seeds", "--testbedding"};
	EXPECT_EQ(Kept(arguments, k_DeveloperOptions).size(), arguments.size());
}

TEST(DeveloperTools, AValueAtTheEndOfTheLineIsNotNeeded)
{
	const std::vector<std::string_view> arguments {"openblack", "--scenario"};
	const auto filtered = FilterDeveloperOptions(arguments, k_DeveloperOptions);
	EXPECT_EQ(filtered.kept.size(), 1U);
	EXPECT_EQ(filtered.ignored, (std::vector<std::string> {"--scenario"}));
}

TEST(DeveloperTools, TheServerFilterKeepsTheOptionsOfADeployedServer)
{
	const std::vector<std::string_view> arguments {"openblack-server",
	                                               "--game-path=/game",
	                                               "--start-level=Playgrounds/TwoGods.txt",
	                                               "--listen=127.0.0.1:7650",
	                                               "--server-id=a",
	                                               "--ticket-keys=https://example.org/api/v1/ticket-keys",
	                                               "--lobby-api=https://srv.example.org/api/v1",
	                                               "--client-cert=/c.pem",
	                                               "--client-key=/c.key",
	                                               "--room-name=Game",
	                                               "--gods=2",
	                                               "--min-gods=1",
	                                               "--profile-folder=/p",
	                                               "--log-file=stdout",
	                                               "--come-back-seconds",
	                                               "300"};
	EXPECT_EQ(Kept(arguments, k_ServerDeveloperOptions).size(), arguments.size());
}

TEST(DeveloperTools, TheServerFilterRemovesTheToolOptions)
{
	const std::vector<std::string_view> arguments {
	    "openblack-server", "--testbed", "--scenario",         "x",      "-n", "40", "--fast",
	    "--record",         "r.jsonl",   "--commands=c.jsonl", "--seed", "3"};
	EXPECT_EQ(Kept(arguments, k_ServerDeveloperOptions), (std::vector<std::string> {"openblack-server"}));
}

TEST(DeveloperTools, ADeployedServerKeepsTheConditionsAndTheTimeLimitOfTheOperator)
{
	const std::vector<std::string_view> arguments {
	    "openblack-server", "-g", "/game", "--conditions=mpm_2p_1:WC_KILL_NUM_VILLAGERS=20,WC_GROW_TREES=5",
	    "--time-limit",     "90"};
	const auto kept = Kept(arguments, k_ServerDeveloperOptions);
	ASSERT_EQ(kept.size(), arguments.size());

	// The kept value parses, and the checks of the server accept it as the host's controls do
	constexpr std::string_view k_Template = "FORMAP mpm_2p_1.txt\n"
	                                        "WC_KILL_NUM_VILLAGERS:\t\t1\t1\t500\t1 \t25\n"
	                                        "WC_GROW_TREES:\t1\t1\t1000\t1\t100\n";
	const auto request =
	    openblack::sim_run::ParseConditions(std::string_view(kept.at(3)).substr(std::string_view("--conditions=").size()));
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->limitsLand, "mpm_2p_1");
	const auto limits = openblack::gui::multiplayer::FindConditions(k_Template, request->limitsLand);
	EXPECT_FALSE(
	    openblack::gui::multiplayer::ConditionsRefusal(limits, *openblack::sim_run::PicksWithValues(*request)).has_value());
	EXPECT_FALSE(openblack::game_end::TimeLimitRefusal(90, false).has_value());

	// A value out of the land's limits and a time limit out of the host's limits stop the server
	const auto tooMany = openblack::sim_run::ParseConditions("WC_KILL_NUM_VILLAGERS=501");
	ASSERT_TRUE(tooMany.has_value());
	EXPECT_TRUE(
	    openblack::gui::multiplayer::ConditionsRefusal(limits, *openblack::sim_run::PicksWithValues(*tooMany)).has_value());
	EXPECT_TRUE(openblack::game_end::TimeLimitRefusal(5, false).has_value());
	EXPECT_TRUE(openblack::game_end::TimeLimitRefusal(1500, false).has_value());
}

TEST(DeveloperTools, ADeployedServerRemovesTheTestOptions)
{
	EXPECT_FALSE(IsDeveloperOption("conditions", k_ServerDeveloperOptions));
	EXPECT_FALSE(IsDeveloperOption("time-limit", k_ServerDeveloperOptions));
	const std::vector<std::string_view> arguments {
	    "openblack-server", "--temple-loss", "1:300", "--test-commands", "-g", "path"};
	EXPECT_EQ(Kept(arguments, k_ServerDeveloperOptions), (std::vector<std::string> {"openblack-server", "-g", "path"}));
}

TEST(DeveloperTools, EachDeveloperOptionHasOneName)
{
	for (const auto options :
	     {std::span<const DeveloperOption>(k_DeveloperOptions), std::span<const DeveloperOption>(k_ServerDeveloperOptions)})
	{
		std::set<std::string_view> names;
		std::set<char> shortNames;
		for (const auto& option : options)
		{
			EXPECT_TRUE(names.insert(option.name).second) << option.name;
			EXPECT_TRUE(IsDeveloperOption(option.name, options));
			if (option.shortName != '\0')
			{
				EXPECT_TRUE(shortNames.insert(option.shortName).second) << option.name;
			}
		}
	}
	EXPECT_FALSE(IsDeveloperOption("background", k_DeveloperOptions));
	EXPECT_FALSE(IsDeveloperOption("join", k_DeveloperOptions));
}

TEST(DeveloperTools, AJoinDoesNotKeepTheLandSeedAndStartOptions)
{
	// A client that joins a game server takes the land, the seed and the start from the server
	const std::vector<std::string_view> arguments {"openblack", "--join", "wss://127.0.0.1:1/game/a", "-s",  "Land1.txt",
	                                               "--seed",    "1",      "--skip-opening",           "cow", "--testbed"};
	EXPECT_EQ(Kept(arguments, k_DeveloperOptions),
	          (std::vector<std::string> {"openblack", "--join", "wss://127.0.0.1:1/game/a"}));
	EXPECT_EQ(FilterDeveloperOptions(arguments, k_DeveloperOptions).ignored.size(), 4U);
}

TEST(DeveloperTools, TheQuestionOfAPlayerHasOnlyTheAnswersOfTheOriginalGame)
{
	using openblack::new_game_choice::Choice;
	using openblack::new_game_choice::IsShown;
	EXPECT_FALSE(IsShown(Choice::DebugStartWithCow, true, false));
	EXPECT_TRUE(IsShown(Choice::DebugStartWithCow, true, true));
	for (const auto choice : {Choice::StartNormally, Choice::SkipToCreatureSelect, Choice::SkipCreatureTutorial})
	{
		EXPECT_TRUE(IsShown(choice, false, false));
	}
	EXPECT_TRUE(IsShown(Choice::KeepOldCreature, true, false));
	EXPECT_FALSE(IsShown(Choice::KeepOldCreature, false, true));
}

TEST(DeveloperTools, ABuildForPlayersRefusesTheSoakToolsAndADeveloperBuildAllowsThem)
{
	const SoakTools none;
	const SoakTools computers {.allComputerGods = true};
	const SoakTools seeded {.allComputerGods = false, .aiSeed = 0u};
	EXPECT_FALSE(none.Any());
	EXPECT_TRUE(computers.Any());
	EXPECT_TRUE(seeded.Any());
	EXPECT_FALSE(SoakRefusalOf(false, none).has_value());
	EXPECT_TRUE(SoakRefusalOf(false, computers).has_value());
	EXPECT_TRUE(SoakRefusalOf(false, seeded).has_value());
	EXPECT_FALSE(SoakRefusalOf(true, computers).has_value());
	EXPECT_FALSE(SoakRefusalOf(true, seeded).has_value());
}

TEST(DeveloperTools, ABuildForPlayersRefusesTheScenarioOfAGameServer)
{
	EXPECT_TRUE(ScenarioRefusalOf(false).has_value());
	EXPECT_FALSE(ScenarioRefusalOf(true).has_value());
}

TEST(DeveloperTools, TheSoakOptionsOfTheGameAndTheServerAreDeveloperOptions)
{
	EXPECT_TRUE(IsDeveloperOption("sim-all-computer-gods", k_DeveloperOptions));
	EXPECT_TRUE(IsDeveloperOption("sim-ai-seed", k_DeveloperOptions));
	EXPECT_TRUE(IsDeveloperOption("sim-world-seed", k_DeveloperOptions));
	EXPECT_TRUE(IsDeveloperOption("all-computer-gods", k_ServerDeveloperOptions));
	EXPECT_TRUE(IsDeveloperOption("ai-seed", k_ServerDeveloperOptions));
	const std::vector<std::string_view> arguments {"openblack-server", "--all-computer-gods", "--ai-seed", "5", "-g", "path"};
	EXPECT_EQ(Kept(arguments, k_ServerDeveloperOptions), (std::vector<std::string> {"openblack-server", "-g", "path"}));
}

TEST(DeveloperTools, TheFrameStatisticsAndDrawEncodeOptionsAreDeveloperOptions)
{
	EXPECT_TRUE(IsDeveloperOption("frame-stats-out", k_DeveloperOptions));
	EXPECT_TRUE(IsDeveloperOption("parallel-draw-encode", k_DeveloperOptions));
	EXPECT_TRUE(IsDeveloperOption("no-stage-times", k_DeveloperOptions));
	const std::vector<std::string_view> arguments {
	    "openblack", "--frame-stats-out", "stats.csv", "--parallel-draw-encode", "--no-stage-times", "-W", "800"};
	EXPECT_EQ(Kept(arguments, k_DeveloperOptions), (std::vector<std::string> {"openblack", "-W", "800"}));
}

TEST(DeveloperTools, TheScenarioPlayersExistOnlyInABuildWithTheTools)
{
	const auto ids = openblack::testbed_scenarios::ScenarioIds();
	if constexpr (k_Built)
	{
		ASSERT_FALSE(ids.empty());
		EXPECT_NE(openblack::testbed_scenarios::MakeScenarioPlayer(ids.front()), nullptr);
	}
	else
	{
		EXPECT_TRUE(ids.empty());
		EXPECT_EQ(openblack::testbed_scenarios::MakeScenarioPlayer("benchmark.creatures_10"), nullptr);
	}
	EXPECT_EQ(openblack::testbed_scenarios::MakeScenarioPlayer("no.such.scenario"), nullptr);
}

TEST(DeveloperTools, EveryOptionThatNeedsTheInspectorIsAlsoADeveloperOption)
{
	for (const auto name : k_InspectorOptions)
	{
		EXPECT_TRUE(std::ranges::any_of(k_DeveloperOptions, [name](const DeveloperOption& option) {
			return option.name == name;
		})) << name;
	}
	// This option works with no inspector
	EXPECT_EQ(std::ranges::count(k_InspectorOptions, std::string_view {"sim-commands"}), 0);
}

TEST(DeveloperTools, ABuildWithNoInspectorNamesEachOptionThatNeedsIt)
{
	EXPECT_FALSE(InspectorOptionsError({}).has_value());
	const std::vector<std::string> one {"sim-record"};
	EXPECT_EQ(*InspectorOptionsError(one),
	          "This build has no inspector. It cannot do the option --sim-record. Use a desktop build that has the inspector.");
	const std::vector<std::string> two {"sim-input", "inspect-port"};
	EXPECT_EQ(*InspectorOptionsError(two), "This build has no inspector. It cannot do the options --sim-input, --inspect-port. "
	                                       "Use a desktop build that has the inspector.");
}
