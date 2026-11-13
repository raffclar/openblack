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

#include <string>

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
};

/// The error and crash reports of this program to Sentry. The service has no effect until it starts, and it starts only
/// when it has a DSN. The game starts it when the player turns on the option. The game server starts it at once.
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
	/// The state of the service, with no DSN
	[[nodiscard]] virtual ErrorReportsState GetState() const = 0;
	/// Sends the event of a desync, which DesyncReportRule allowed. Nothing when the service does not send reports.
	virtual void SendDesync(const DesyncReport& report) = 0;
};

} // namespace openblack::error_reports
