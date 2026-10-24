/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Common/DeveloperTools.h>
#include <Story/NewGameChoice.h>
#include <gtest/gtest.h>

using namespace openblack::developer_tools;

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
	                                               "--join-ticket",
	                                               "t",
	                                               "--backend-type",
	                                               "Vulkan",
	                                               "--lobby",
	                                               "https://example.org"};
	const auto filtered = FilterDeveloperOptions(arguments, k_DeveloperOptions);
	EXPECT_EQ(filtered.kept.size(), arguments.size());
	EXPECT_TRUE(filtered.ignored.empty());
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

TEST(DeveloperTools, AJoinKeepsTheOptionsThatTheGameServerChecks)
{
	const std::vector<std::string_view> arguments {"openblack", "--join", "wss://127.0.0.1:1/game/a", "-s",  "Land1.txt",
	                                               "--seed",    "1",      "--skip-opening",           "cow", "--testbed",
	                                               "--sim-fast"};
	ASSERT_TRUE(JoinsGameServer(arguments));
	const auto removed = OptionsToRemove(k_DeveloperOptions, true);
	EXPECT_EQ(removed.size(), k_DeveloperOptions.size() - k_JoinWorldOptions.size());
	EXPECT_EQ(Kept(arguments, removed), (std::vector<std::string> {"openblack", "--join", "wss://127.0.0.1:1/game/a", "-s",
	                                                               "Land1.txt", "--seed", "1", "--skip-opening", "cow"}));
}

TEST(DeveloperTools, WithNoJoinTheFilterRemovesTheWorldOptions)
{
	const std::vector<std::string_view> arguments {"openblack", "-s", "Land2.txt", "--seed=4", "--skip-opening", "cow"};
	EXPECT_FALSE(JoinsGameServer(arguments));
	EXPECT_EQ(Kept(arguments, OptionsToRemove(k_DeveloperOptions, false)), (std::vector<std::string> {"openblack"}));
}

TEST(DeveloperTools, AJoinIsTheJoinOptionOnly)
{
	EXPECT_TRUE(JoinsGameServer(std::vector<std::string_view> {"openblack", "--join=wss://127.0.0.1:1/game/a"}));
	EXPECT_FALSE(JoinsGameServer(std::vector<std::string_view> {"openblack", "--join-ticket", "t"}));
	EXPECT_FALSE(JoinsGameServer(std::vector<std::string_view> {"--join"}));
	EXPECT_FALSE(JoinsGameServer(std::vector<std::string_view> {}));
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
