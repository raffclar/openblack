/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include <Common/ErrorReportsInterface.h>
#include <Common/ReportRedaction.h>
#include <gtest/gtest.h>

using namespace openblack::error_reports;

namespace
{

/// A file of the log in memory, which counts its writes
struct FakeFile
{
	std::string text;
	int replaces {0};
	int appends {0};
};

class FakeStorage final: public ReportLogStorage
{
public:
	explicit FakeStorage(std::shared_ptr<FakeFile> file)
	    : _file(std::move(file))
	{
	}
	[[nodiscard]] std::string Read() override { return _file->text; }
	void Replace(std::string_view text) override
	{
		_file->text = std::string(text);
		++_file->replaces;
	}
	void Append(std::string_view text) override
	{
		_file->text += text;
		++_file->appends;
	}

private:
	std::shared_ptr<FakeFile> _file;
};

} // namespace

TEST(ReportRedaction, TheEmailAddressesGetAMark)
{
	EXPECT_EQ(HideEmails("Mail bob.smith+tag@example.co.uk now."), "Mail <email> now.");
	EXPECT_EQ(HideEmails("a@b.cd and x@y.zw"), "<email> and <email>");
	// Texts with an "@" that are not addresses stay the same
	EXPECT_EQ(HideEmails("turn 5 @ 3, a@b, @home, me@site.x"), "turn 5 @ 3, a@b, @home, me@site.x");
}

TEST(ReportRedaction, ANameMatchesAsAFullWordWithNoDifferenceForUpperCase)
{
	NameRedactor redactor;
	ASSERT_TRUE(redactor.Add("Player2"));
	EXPECT_EQ(redactor.Redact("slot 1 (Player2), PLAYER2 left, player2x stays, xplayer2 stays"),
	          "slot 1 (<name>), <name> left, player2x stays, xplayer2 stays");
	EXPECT_FALSE(redactor.Add("player2"));
	EXPECT_EQ(redactor.Count(), 1U);
}

TEST(ReportRedaction, TheRedactorRefusesShortNamesAndNamesOfDigits)
{
	NameRedactor redactor;
	EXPECT_FALSE(redactor.Add(""));
	EXPECT_FALSE(redactor.Add("a"));
	EXPECT_FALSE(redactor.Add("1200"));
	EXPECT_EQ(redactor.Count(), 0U);
	// The turns, the keys of the desyncs and the build stay in the log
	const std::string_view line = "Desync report: key Animal of game 4f2a at turn 1200, build openblack@0.1+abc";
	EXPECT_EQ(redactor.Redact(line), line);
}

TEST(ReportRedaction, ALongNameMatchesBeforeANameAtItsStart)
{
	NameRedactor redactor;
	ASSERT_TRUE(redactor.Add("Bob"));
	ASSERT_TRUE(redactor.Add("Bob Smith"));
	EXPECT_EQ(redactor.Redact("Bob Smith and Bob joined"), "<name> and <name> joined");
}

TEST(ReportRedaction, ANameInAnEmailAddressGoesWithTheAddress)
{
	NameRedactor redactor;
	ASSERT_TRUE(redactor.Add("alice"));
	EXPECT_EQ(redactor.Redact("alice wrote from alice@mail.example"), "<name> wrote from <email>");
}

TEST(ReportRedaction, TheReportNamesHideTheHomeFolderAndCountTheNames)
{
	ReportNames names("C:\\Users\\alice", true);
	EXPECT_EQ(names.Version(), 0U);
	EXPECT_EQ(names.Redact("Read C:\\Users\\alice\\openblack.cfg"), "Read ~\\openblack.cfg");
	EXPECT_TRUE(names.Add("Zanidrak"));
	EXPECT_FALSE(names.Add("zanidrak"));
	EXPECT_EQ(names.Version(), 1U);
	EXPECT_EQ(names.Redact("The lobby session of Zanidrak has the editor"), "The lobby session of <name> has the editor");
}

TEST(ReportRedaction, TheWriterRedactsTheFileAgainWhenANameComesLater)
{
	auto file = std::make_shared<FakeFile>();
	file->text = "an old run of the game\n";
	auto names = std::make_shared<ReportNames>("", false);
	ReportLogWriter writer(std::make_unique<FakeStorage>(file), names);

	// The first write makes the file empty
	writer.Write("slot 1 (Mira) joined at turn 10\n");
	EXPECT_EQ(file->text, "slot 1 (Mira) joined at turn 10\n");
	EXPECT_EQ(file->replaces, 1);

	// With no new name, the writer only adds the lines
	writer.Write("turn 11\n");
	EXPECT_EQ(file->replaces, 1);
	EXPECT_EQ(file->appends, 1);

	// A name that comes after the log wrote it goes from all the file
	ASSERT_TRUE(names->Add("Mira"));
	writer.Write("Mira left at turn 12\n");
	EXPECT_EQ(file->text, "slot 1 (<name>) joined at turn 10\nturn 11\n<name> left at turn 12\n");
	EXPECT_EQ(file->replaces, 2);

	writer.Write("turn 13\n");
	EXPECT_EQ(file->replaces, 2);
	EXPECT_EQ(file->text.find("Mira"), std::string::npos);
}

TEST(ReportRedaction, TheFieldsOfAnEventHideTheNamesButKeepTheFingerprint)
{
	ReportNames names("", false);
	ASSERT_TRUE(names.Add("Mira"));
	const FailureEvent event {.message = "Mira could not load the map",
	                          .level = "warning",
	                          .fingerprint = {"map", "Mira"},
	                          .tags = {{"step", "Mira"}},
	                          .extras = {{"who", "mira@mail.example"}, {"turn", "12"}}};
	const auto hidden = HideNamesInFields(event, names);
	EXPECT_EQ(hidden.message, "<name> could not load the map");
	EXPECT_EQ(hidden.fingerprint, event.fingerprint);
	EXPECT_EQ(hidden.tags.front().second, "<name>");
	EXPECT_EQ(hidden.extras.front().second, "<email>");
	EXPECT_EQ(hidden.extras.back().second, "12");
}

TEST(ReportRedaction, TheCopyOfTheLogIsNextToTheLogFile)
{
	EXPECT_EQ(ReportLogPath(std::filesystem::path("logs") / "openblack.log"),
	          std::filesystem::path("logs") / "openblack.report.log");
	EXPECT_EQ(ReportLogPath("openblack.log"), std::filesystem::path("openblack.report.log"));
}
