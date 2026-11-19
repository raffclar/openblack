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
#include <string_view>

#include "ErrorReportsInterface.h"
#include "ReportRedaction.h"
#include "SentryBuildDsn.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#if !defined(__EMSCRIPTEN__)
#error "The reports through the browser SDK are only for the web build"
#endif

namespace openblack::error_reports
{

/// What the reports of the web game tell Sentry
struct BrowserReportSettings
{
	/// Empty when there is no DSN. Then the service never starts.
	std::string dsn;
	std::string release;
	std::string environment;
	Role role {Role::Client};
};

/// The reports to Sentry of the web game, through the Sentry browser SDK of the page (web/page/launcher-reports.js).
/// The page attaches the game log to each report, and it removes the personal data first. The page also hides the names
/// that the game gives (HideName) and the email addresses. The crash handler of the game
/// gives each crash to the page, with the stack of the thread of the crash. The page itself reports the traps and the
/// aborts of the WebAssembly code.
class BrowserErrorReports final: public ErrorReportsInterface
{
public:
	explicit BrowserErrorReports(BrowserReportSettings settings);
	~BrowserErrorReports() override;
	BrowserErrorReports(const BrowserErrorReports&) = delete;
	BrowserErrorReports& operator=(const BrowserErrorReports&) = delete;
	BrowserErrorReports(BrowserErrorReports&&) = delete;
	BrowserErrorReports& operator=(BrowserErrorReports&&) = delete;

	[[nodiscard]] bool HasDestination() const noexcept override { return !_settings.dsn.empty(); }
	[[nodiscard]] bool IsRunning() const noexcept override { return _running; }
	void SetEnabled(bool enabled) override;
	void SetAccountConsent(std::optional<AccountConsent> consent) override;
	void SendDesync(const DesyncReport& report) override;
	void SendEvent(const FailureEvent& event) override;
	/// Gives the name to the page, which hides it in the log and the texts of each report
	void HideName(std::string_view name) override;
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

	BrowserReportSettings _settings;
	bool _running {false};
	bool _enabled {false};
	std::optional<AccountConsent> _account;
	bool _personalData {false};
	/// The id of the account that the reports name now, or empty
	std::string _namedAccount;
	uint32_t _desyncEvents {0};
	/// The names that the page has already
	NameRedactor _hiddenNames;
};

} // namespace openblack::error_reports
