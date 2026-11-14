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

#include "ErrorReportsInterface.h"
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
/// The page attaches the game log to each report, and it removes the personal data first. The crash handler of the game
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
	void SendDesync(const DesyncReport& report) override;
	[[nodiscard]] ErrorReportsState GetState() const override
	{
		return {.hasDestination = HasDestination(),
		        .running = _running,
		        .enabled = _enabled,
		        .role = _settings.role,
		        .release = _settings.release,
		        .environment = _settings.environment,
		        .desyncEvents = _desyncEvents};
	}

private:
	void Start();
	void Stop();

	BrowserReportSettings _settings;
	bool _running {false};
	bool _enabled {false};
	uint32_t _desyncEvents {0};
};

} // namespace openblack::error_reports
