/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "CrashReport.h"
#include "ErrorReportsInterface.h"

/// The rules of the error and crash reports, with no state, so that the tests can check them with no Sentry
namespace openblack::error_reports
{

/// The name of the tag that tells which program sent the report
constexpr std::string_view k_RoleTagName = "role";
/// The longest time, in milliseconds, that a crash waits for its report to go before the program ends
constexpr uint64_t k_CrashFlushMilliseconds = 5000;
/// The longest time, in milliseconds, that a normal stop waits for the reports that are not sent
constexpr uint64_t k_CloseMilliseconds = 2000;
/// The start of the message of the test error, which the developer switch --sentry-test-error throws
constexpr std::string_view k_TestErrorText = "Sentry test error";
/// The name of the folder of the reports that wait, next to the log file
constexpr std::string_view k_DatabaseFolderName = ".sentry-native";

/// The DSN to use: the SENTRY_DSN environment variable when it is set and not empty, else the DSN of the build. None when
/// both are empty.
[[nodiscard]] std::optional<std::string> ChooseDsn(std::optional<std::string_view> fromEnvironment, std::string_view fromBuild);
/// The release of the reports: "openblack@" with the semantic version, a "+" and the commit
[[nodiscard]] std::string ReleaseName(std::string_view semanticVersion, std::string_view commit);
/// "development" for a build with the developer tools, "production" for a build that players get
[[nodiscard]] std::string_view EnvironmentName(bool developerTools) noexcept;
/// The value of the role tag: "client" or "server"
[[nodiscard]] std::string_view RoleTag(Role role) noexcept;
/// The log file to attach to each report. None when the log goes to the terminal ("stdout" or "logcat") or nowhere.
[[nodiscard]] std::optional<std::filesystem::path> LogAttachment(std::string_view logFile);
/// The log file that the reports of the role attach. The game server attaches none: its log has the names of the
/// players, and each player gives a separate choice for the error reports. The staff find the log of a game on the host.
[[nodiscard]] std::optional<std::filesystem::path> LogAttachmentOf(Role role, std::string_view logFile);
/// The folder of the reports that wait: next to the log file, or in the working folder when the log goes to the
/// terminal
[[nodiscard]] std::filesystem::path DatabaseFolder(std::string_view logFile);
/// Whether the crash handler sends a report of its own for a crash. The crashpad handler gets the system exceptions on
/// Windows from the crash handler, and the signals on the other systems before the crash handler.
[[nodiscard]] bool SendsCrashEvent(bool systemException, bool fromSignal, bool windows) noexcept;
/// The path with the home folder of the user changed to "~", so that the report does not hold the name of the user.
/// None when the path is not in the home folder. Windows compares the folders with no difference for upper case.
[[nodiscard]] std::optional<std::string> HideHomeFolder(std::string_view path, std::string_view homeFolder, bool windows);
/// The text with each path in the home folder of the user changed as HideHomeFolder changes it
[[nodiscard]] std::string HideHomeFolderInText(std::string_view text, std::string_view homeFolder, bool windows);
/// The message of the test error: the start text and the time in seconds since 1970
[[nodiscard]] std::string TestErrorMessage(int64_t unixSeconds);

/// The choice of the text that the lobby gives ("full", "anonymous" or "off"). None for another text.
[[nodiscard]] std::optional<ReportsConsent> ParseReportsConsent(std::string_view text) noexcept;
/// The text of the choice that the lobby reads
[[nodiscard]] std::string_view ReportsConsentText(ReportsConsent consent) noexcept;

/// What the service does with the option of the player and the choice of the account
struct ReportsPlan
{
	/// The service sends reports
	bool running {false};
	/// The reports have the name of the account and the IP address
	bool personalData {false};

	bool operator==(const ReportsPlan&) const = default;
};

/// The plan of the reports. With a choice of the account, the choice wins: Off stops the reports, and only Full gives
/// personal data. With no choice, the option of the player decides, and the reports never have personal data.
[[nodiscard]] ReportsPlan ReportsPlanOf(bool option, const std::optional<AccountConsent>& account) noexcept;
/// The value of the box "Send Crash Reports" that follows the choice of the account: set for Full and Anonymous, clear
/// for Off
[[nodiscard]] bool OptionOfConsent(ReportsConsent consent) noexcept;
/// The choice that the game sends to the lobby when the player changes the box while an account is logged in. A set
/// box gives Anonymous, because the player gives Full only on the site, where the privacy notice is.
[[nodiscard]] ReportsConsent ConsentOfOption(bool option) noexcept;
/// Whether the game server can put the name or the address of a player into a report: only for the choice Full in the
/// join ticket of the player
[[nodiscard]] bool MayNamePlayer(std::optional<ReportsConsent> ticketChoice) noexcept;

/// The player that a report of the game server may name, or nothing when the choice on the join ticket of the player is
/// not Full or the player has no name
[[nodiscard]] std::optional<ReportedPlayer> ReportedPlayerOf(std::string_view name, std::string_view address,
                                                             std::optional<ReportsConsent> ticketChoice);

/// The type and the message of the exception of a crash event
struct CrashEventTexts
{
	std::string type;
	std::string message;
};

/// The texts of the event of a crash. The type is the detail of the crash, such as the type of the exception, or else
/// the name of the kind. The message is the message of the crash, or else the name of the kind.
[[nodiscard]] CrashEventTexts CrashEventTextsOf(const crash_report::CrashInfo& info);

} // namespace openblack::error_reports
