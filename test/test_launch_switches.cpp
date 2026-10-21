/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>

#include <LaunchSwitches.h>
#include <gtest/gtest.h>

using namespace openblack::launch_switches;

TEST(LaunchSwitches, ASwitchStartsWithADashOrASlashInAnyCase)
{
	const auto switches = Scan(R"(C:\Games\runblack.exe -newgame /NoLoadMusic -PreIntroVideoPlease)");
	EXPECT_TRUE(switches.newGame);
	EXPECT_TRUE(switches.noLoadMusic);
	// Other text can follow the name
	EXPECT_TRUE(switches.preIntroVideo);
	EXPECT_FALSE(switches.skirmish);
	EXPECT_FALSE(switches.convert);
	EXPECT_FALSE(switches.settingsFile.has_value());
}

TEST(LaunchSwitches, TheCharacterAfterADashNeverStartsASwitch)
{
	EXPECT_FALSE(Scan("game --NEWGAME").newGame);
	EXPECT_FALSE(Scan("game -/NEWGAME").newGame);
	EXPECT_TRUE(Scan("game -x-NEWGAME").newGame);
	// The name must be all there
	EXPECT_FALSE(Scan("game -NEWGAM").newGame);
	EXPECT_FALSE(Scan("game -").newGame);
}

TEST(LaunchSwitches, OnlyTheFirstOfNewGameAndMultiplayerCounts)
{
	const auto first = Scan("game -NEWGAME -MULTIPLAYER");
	EXPECT_TRUE(first.newGame);
	EXPECT_FALSE(first.multiplayer);
	const auto second = Scan("game -MULTIPLAYER -NEWGAME");
	EXPECT_FALSE(second.newGame);
	EXPECT_TRUE(second.multiplayer);
}

TEST(LaunchSwitches, NewGameComesBeforeSkirmishAndSkirmishBeforeMultiplayer)
{
	EXPECT_EQ(FirstGameOf(Scan("game")), FirstGame::Plain);
	EXPECT_EQ(FirstGameOf(Scan("game -SKIRMISH -NEWGAME")), FirstGame::NewGame);
	EXPECT_EQ(FirstGameOf(Scan("game -MULTIPLAYER -SKIRMISH")), FirstGame::Skirmish);
	EXPECT_EQ(FirstGameOf(Scan("game -MULTIPLAYER")), FirstGame::Multiplayer);
}

TEST(LaunchSwitches, TheSettingsSwitchTakesTheTextUpToTheNextSpace)
{
	EXPECT_EQ(Scan("game -SETTINGS:mods\\my.txt -NEWGAME").settingsFile, std::optional<std::string>("mods\\my.txt"));
	// A later switch replaces the file, and an empty name gives no file
	EXPECT_EQ(Scan("game -settings:a.txt /SETTINGS:b.txt").settingsFile, std::optional<std::string>("b.txt"));
	EXPECT_FALSE(Scan("game -SETTINGS:a.txt -SETTINGS: -NEWGAME").settingsFile.has_value());
	// The text of the name can hold another switch
	EXPECT_TRUE(Scan("game -SETTINGS:a-CONVERT").convert);
}

TEST(LaunchSwitches, TheSwitchesThatNothingUsesAreRead)
{
	const auto switches = Scan("game -LAND -EDITOR -FORCEINETCONN -NOINETCONN -VERSION -CONVERT");
	EXPECT_TRUE(switches.land);
	EXPECT_TRUE(switches.editor);
	EXPECT_TRUE(switches.forceInternetCheck);
	EXPECT_TRUE(switches.noInternetCheck);
	EXPECT_TRUE(switches.version);
	EXPECT_TRUE(switches.convert);
}

TEST(LaunchSwitches, OpenblacksOwnOptionsAreNotSwitches)
{
	EXPECT_TRUE(IsSwitchArgument("-NEWGAME"));
	EXPECT_TRUE(IsSwitchArgument("/settings:x.txt"));
	EXPECT_FALSE(IsSwitchArgument("--new-game"));
	EXPECT_FALSE(IsSwitchArgument("-n"));
	EXPECT_FALSE(IsSwitchArgument("--game-path"));
	EXPECT_FALSE(IsSwitchArgument("NEWGAME"));
	EXPECT_FALSE(IsSwitchArgument(""));
}

TEST(LaunchSwitches, TheLastQuestFolderLineOfTheSettingsFileCounts)
{
	EXPECT_EQ(QuestFolderOf("QUESTDIR .\\Scripts\\MyQuests\\\r\n"), std::optional<std::string>(".\\Scripts\\MyQuests\\"));
	EXPECT_EQ(QuestFolderOf("QUESTDIR a\\\nQUESTDIR,b\\\n"), std::optional<std::string>("b\\"));
	// Words are divided by spaces, commas and tabs, and the leading ones are passed over
	EXPECT_EQ(QuestFolderOf("  \tQUESTDIR\tc\\ d\n"), std::optional<std::string>("c\\"));
	// A carriage return that is not before a line end stays in the word
	EXPECT_EQ(QuestFolderOf("QUESTDIR e\\\r"), std::optional<std::string>("e\\\r"));
	// The word is compared in capitals only, and a line with no folder changes nothing
	EXPECT_FALSE(QuestFolderOf("questdir f\\\n").has_value());
	EXPECT_EQ(QuestFolderOf("QUESTDIR g\\\nQUESTDIR\n"), std::optional<std::string>("g\\"));
	EXPECT_FALSE(QuestFolderOf("").has_value());
}

TEST(LaunchSwitches, ASlashFirstOrSecondInTheFirstWordMakesAComment)
{
	EXPECT_FALSE(QuestFolderOf("// QUESTDIR a\\\n").has_value());
	EXPECT_FALSE(QuestFolderOf("/QUESTDIR a\\\n").has_value());
	// The second character of the first word counts too
	EXPECT_FALSE(QuestFolderOf("Q/ QUESTDIR a\\\n").has_value());
	EXPECT_EQ(QuestFolderOf("QUESTDIR /a/\n"), std::optional<std::string>("/a/"));
}

TEST(LaunchSwitches, ACtrlZEndsTheSettingsFile)
{
	EXPECT_FALSE(QuestFolderOf("\x1aQUESTDIR a\\\n").has_value());
	EXPECT_EQ(QuestFolderOf("QUESTDIR a\\\n\x1aQUESTDIR b\\\n"), std::optional<std::string>("a\\"));
}

TEST(LaunchSwitches, TheChallengeFileNameFollowsTheFolderText)
{
	EXPECT_EQ(ChallengePath(k_DefaultQuestFolder), "Scripts/Quests/Challenge.chl");
	EXPECT_EQ(ChallengePath(".\\Scripts\\MyQuests\\"), "Scripts/MyQuests/Challenge.chl");
	// With no "\" at its end, the folder's last name becomes part of the file name
	EXPECT_EQ(ChallengePath("Mods\\Quests"), "Mods/QuestsChallenge.chl");
	EXPECT_EQ(ChallengePath("C:\\Mods\\"), "C:/Mods/Challenge.chl");
}
