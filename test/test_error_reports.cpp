/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <Common/ErrorReportsRules.h>
#include <gtest/gtest.h>

using namespace openblack::error_reports;

TEST(ErrorReports, TheEnvironmentDsnComesBeforeTheDsnOfTheBuild)
{
	EXPECT_EQ(ChooseDsn(std::string_view("https://a@example.org/1"), "https://b@example.org/2"), "https://a@example.org/1");
	// An empty variable counts as no variable
	EXPECT_EQ(ChooseDsn(std::string_view(""), "https://b@example.org/2"), "https://b@example.org/2");
	EXPECT_EQ(ChooseDsn(std::nullopt, "https://b@example.org/2"), "https://b@example.org/2");
	// With no DSN, the reports never start
	EXPECT_EQ(ChooseDsn(std::nullopt, ""), std::nullopt);
	EXPECT_EQ(ChooseDsn(std::string_view(""), ""), std::nullopt);
}

TEST(ErrorReports, TheReleaseHasTheVersionAndTheCommit)
{
	EXPECT_EQ(ReleaseName("0.0.1", "0123456789ab"), "openblack@0.0.1+0123456789ab");
	EXPECT_EQ(ReleaseName("0.0.1", "unknown"), "openblack@0.0.1+unknown");
}

TEST(ErrorReports, TheEnvironmentAndTheRoleNameTheBuildAndTheProgram)
{
	EXPECT_EQ(EnvironmentName(true), "development");
	EXPECT_EQ(EnvironmentName(false), "production");
	EXPECT_EQ(RoleTag(Role::Client), "client");
	EXPECT_EQ(RoleTag(Role::Server), "server");
	EXPECT_EQ(k_RoleTagName, "role");
}

TEST(ErrorReports, OnlyALogFileIsAttachedAndTheReportsWaitNextToIt)
{
	EXPECT_EQ(LogAttachment("stdout"), std::nullopt);
	EXPECT_EQ(LogAttachment("logcat"), std::nullopt);
	EXPECT_EQ(LogAttachment(""), std::nullopt);
	EXPECT_EQ(LogAttachment("logs/openblack.log"), std::filesystem::path("logs/openblack.log"));
	EXPECT_EQ(DatabaseFolder("logs/openblack.log"), std::filesystem::path("logs") / ".sentry-native");
	EXPECT_EQ(DatabaseFolder("stdout"), std::filesystem::path(".sentry-native"));
}

TEST(ErrorReports, TheCrashHandlerSendsOnlyTheCrashesThatCrashpadDoesNotSee)
{
	// On Windows, the crash handler gives the system exceptions to crashpad, and sends a report for each other crash
	EXPECT_FALSE(SendsCrashEvent(true, false, true));
	EXPECT_TRUE(SendsCrashEvent(false, false, true));
	EXPECT_TRUE(SendsCrashEvent(false, true, true));
	// On the other systems, crashpad gets the signals before the crash handler
	EXPECT_FALSE(SendsCrashEvent(false, true, false));
	EXPECT_TRUE(SendsCrashEvent(false, false, false));
}

TEST(ErrorReports, ThePathsShowTheHomeFolderAsATildeAndNotTheNameOfTheUser)
{
	EXPECT_EQ(HideHomeFolder(R"(C:\Users\Ann\AppData\x.dll)", R"(C:\Users\Ann)", true), R"(~\AppData\x.dll)");
	// Windows has no difference for upper case, and a separator at the end of the home folder does not count
	EXPECT_EQ(HideHomeFolder(R"(c:\users\ann\x.dll)", R"(C:\Users\Ann\)", true), R"(~\x.dll)");
	EXPECT_EQ(HideHomeFolder("/home/ann/game/openblack-server", "/home/ann", false), "~/game/openblack-server");
	// Other systems compare the case
	EXPECT_EQ(HideHomeFolder("/home/Ann/x", "/home/ann", false), std::nullopt);
	// A folder that only starts with the same letters is not the home folder
	EXPECT_EQ(HideHomeFolder(R"(C:\Users\Anna\x.dll)", R"(C:\Users\Ann)", true), std::nullopt);
	EXPECT_EQ(HideHomeFolder(R"(C:\Windows\System32\x.dll)", R"(C:\Users\Ann)", true), std::nullopt);
	EXPECT_EQ(HideHomeFolder(R"(C:\Windows\x.dll)", "", true), std::nullopt);
}

TEST(ErrorReports, TheCrashReportShowsTheHomeFolderAsATilde)
{
	EXPECT_EQ(
	    HideHomeFolderInText("Minidump: C:\\Users\\Ann\\crashes\\a.dmp\nLog: c:\\users\\ann\\b.log\n", R"(C:\Users\Ann)", true),
	    "Minidump: ~\\crashes\\a.dmp\nLog: ~\\b.log\n");
	// The name of another user that starts with the same letters stays
	EXPECT_EQ(HideHomeFolderInText(R"(C:\Users\Anna\a.dmp)", R"(C:\Users\Ann)", true), R"(C:\Users\Anna\a.dmp)");
	EXPECT_EQ(HideHomeFolderInText("/home/ann", "/home/ann", false), "~");
	EXPECT_EQ(HideHomeFolderInText("no path", "", false), "no path");
}

TEST(ErrorReports, TheTestErrorStartsWithItsTextAndHasTheTime)
{
	EXPECT_EQ(TestErrorMessage(1763035200), "Sentry test error 1763035200");
}

TEST(ErrorReports, TheCrashEventHasTheDetailAndTheMessageOrElseTheNameOfTheKind)
{
	openblack::crash_report::CrashInfo info;
	info.kind = openblack::crash_report::CrashKind::Terminate;
	info.detail = "class std::runtime_error";
	info.message = "Sentry test error 1";
	auto texts = CrashEventTextsOf(info);
	EXPECT_EQ(texts.type, "class std::runtime_error");
	EXPECT_EQ(texts.message, "Sentry test error 1");
	// A crash with no exception, such as an abort, has the name of its kind in both texts
	info.kind = openblack::crash_report::CrashKind::Abort;
	info.detail = {};
	info.message = {};
	texts = CrashEventTextsOf(info);
	EXPECT_EQ(texts.type, "abort() called");
	EXPECT_EQ(texts.message, "abort() called");
}
