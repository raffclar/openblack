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

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openblack::error_reports
{

/// The program that sends the reports. Each report has it as the tag "role".
enum class Role : uint8_t
{
	/// The game on the computer of a player
	Client,
	/// The dedicated game server
	Server,
};

struct DesyncReport;

/// The choice of a lobby account for the error reports. The player makes it on the site, from the privacy notice.
enum class ReportsConsent : uint8_t
{
	/// The reports have the name and the id of the account, and the IP address of the computer
	Full,
	/// The reports have no personal data
	Anonymous,
	/// The game sends no reports
	Off,
};

/// The choice of the account that is logged in to the lobby, with the account that the reports name for Full
struct AccountConsent
{
	ReportsConsent choice {ReportsConsent::Anonymous};
	/// The id of the lobby account, as text
	std::string accountId;
	/// The login name of the lobby account
	std::string name;

	bool operator==(const AccountConsent&) const = default;
};

/// A player that a report of the game server names: the name of the account and the address of the player's computer.
/// The server names a player only when the join ticket of the player gave the choice Full (ReportedPlayerOf).
struct ReportedPlayer
{
	std::string name;
	std::string address;

	bool operator==(const ReportedPlayer&) const = default;
};

/// An event of a failure that the game finds itself, with no player name, account, address or chat
struct FailureEvent
{
	std::string message;
	/// "warning" or "info"
	std::string level;
	/// Sentry puts the events with the same fingerprint into one issue
	std::vector<std::string> fingerprint;
	std::vector<std::pair<std::string, std::string>> tags;
	std::vector<std::pair<std::string, std::string>> extras;
};

/// What the service tells about itself, for the tools. It never holds the DSN.
struct ErrorReportsState
{
	/// Whether the service has a DSN
	bool hasDestination {false};
	/// Whether the service sends reports now
	bool running {false};
	/// The last value that SetEnabled got: for the game, the option of the player
	bool enabled {false};
	Role role {Role::Client};
	std::string release;
	std::string environment;
	/// The desync events that the service sent
	uint32_t desyncEvents {0};
	/// The last value that SetAccountConsent got: the choice of the account of the lobby session, or none
	std::optional<AccountConsent> account;
	/// Whether the reports have the name of the account and the IP address now
	bool personalData {false};
};

/// The error and crash reports of this program to Sentry. The service has no effect until it starts, and it starts only
/// when it has a DSN. The game starts it when the player turns on the option. The game server starts it at once.
/// The choice of the lobby account wins over the option (ReportsPlanOf in ErrorReportsRules.h). Only the choice Full
/// lets the reports have the name of the account and the IP address.
class ErrorReportsInterface
{
public:
	virtual ~ErrorReportsInterface() = default;

	/// Whether the service has a DSN: from the SENTRY_DSN environment variable, or else from the build. With no DSN, the
	/// service never starts and the game does not show the option.
	[[nodiscard]] virtual bool HasDestination() const noexcept = 0;
	/// Whether the service sends reports now
	[[nodiscard]] virtual bool IsRunning() const noexcept = 0;
	/// Starts the service when it is on and has a DSN. Stops the service when it is off. Stop sends the reports that
	/// wait first.
	virtual void SetEnabled(bool enabled) = 0;
	/// Gives the choice of the account of the lobby session, or none when no account is logged in or the account made
	/// no choice. The service then starts, stops and names the account as ReportsPlanOf tells.
	virtual void SetAccountConsent(std::optional<AccountConsent> consent) = 0;
	/// The state of the service, with no DSN
	[[nodiscard]] virtual ErrorReportsState GetState() const = 0;
	/// Sends the event of a desync, which DesyncReportRule allowed. Nothing when the service does not send reports.
	virtual void SendDesync(const DesyncReport& report) = 0;
	/// Sends an event of a failure that the game found. Nothing when the service does not send reports.
	virtual void SendEvent(const FailureEvent& event) = 0;
};

} // namespace openblack::error_reports
