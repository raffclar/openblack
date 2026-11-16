/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ErrorReportsInterface.h"
#include "SentryBuildDsn.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::error_reports
{

/// What the reports tell Sentry. ChooseDsn, ReleaseName and the other rules of ErrorReportsRules.h give the values.
struct SentrySettings
{
	/// Empty when there is no DSN. Then the service never starts.
	std::string dsn;
	std::string release;
	std::string environment;
	Role role {Role::Client};
	/// The log file that a report attaches when it has personal data (LogAttachmentFor)
	std::optional<std::filesystem::path> attachment;
	/// The folder of the reports that wait to go
	std::filesystem::path databaseFolder;
	/// The home folder of the user, which the reports show as "~", or empty
	std::string homeFolder;
};

/// The reports to Sentry through sentry-native, with the crashpad backend. The crashpad handler process gets the
/// crashes. The crash handler of the game also gives each crash that crashpad does not see. The reports have no save
/// file. They have no user and no IP address, unless the lobby account gave the choice Full.
class SentryErrorReports final: public ErrorReportsInterface
{
public:
	explicit SentryErrorReports(SentrySettings settings);
	~SentryErrorReports() override;
	SentryErrorReports(const SentryErrorReports&) = delete;
	SentryErrorReports& operator=(const SentryErrorReports&) = delete;
	SentryErrorReports(SentryErrorReports&&) = delete;
	SentryErrorReports& operator=(SentryErrorReports&&) = delete;

	[[nodiscard]] bool HasDestination() const noexcept override { return !_settings.dsn.empty(); }
	[[nodiscard]] bool IsRunning() const noexcept override { return _running; }
	void SetEnabled(bool enabled) override;
	void SetAccountConsent(std::optional<AccountConsent> consent) override;
	void SendDesync(const DesyncReport& report) override;
	void SendEvent(const FailureEvent& event) override;
	[[nodiscard]] ErrorReportsState GetState() const override
	{
		return {.hasDestination = HasDestination(),
		        .running = _running,
		        .enabled = _enabled,
		        .role = _settings.role,
		        .release = _settings.release,
		        .environment = _settings.environment,
		        .desyncEvents = _desyncEvents,
		        .account = _account,
		        .personalData = _personalData};
	}

private:
	/// Starts or stops the service and names the account or not, as ReportsPlanOf tells
	void Apply();
	void Start();
	void Stop();
	/// Gives the reports the user of the account and the IP address, or removes the user. Only for a running service.
	void SetUser(const std::optional<AccountConsent>& account);

	SentrySettings _settings;
	bool _running {false};
	bool _enabled {false};
	std::optional<AccountConsent> _account;
	bool _personalData {false};
	/// The running service attaches the log file to its reports
	bool _logAttached {false};
	/// The next start of the service attaches the log file
	bool _attachLog {false};
	/// The id of the account that the reports name now, or empty
	std::string _namedAccount;
	uint32_t _desyncEvents {0};
};

} // namespace openblack::error_reports
