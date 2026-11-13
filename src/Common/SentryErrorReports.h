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

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::error_reports
{

/// The DSN of the build, from the cache value OPENBLACK_SENTRY_DSN, or empty
[[nodiscard]] std::string_view BuildDsn() noexcept;

/// What the reports tell Sentry. ChooseDsn, ReleaseName and the other rules of ErrorReportsRules.h give the values.
struct SentrySettings
{
	/// Empty when there is no DSN. Then the service never starts.
	std::string dsn;
	std::string release;
	std::string environment;
	Role role {Role::Client};
	/// The log file that each report attaches
	std::optional<std::filesystem::path> attachment;
	/// The folder of the reports that wait to go
	std::filesystem::path databaseFolder;
	/// The home folder of the user, which the reports show as "~", or empty
	std::string homeFolder;
};

/// The reports to Sentry through sentry-native, with the crashpad backend. The crashpad handler process gets the
/// crashes. The crash handler of the game also gives each crash that crashpad does not see. The reports have no user,
/// no IP address and no save file.
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

private:
	void Start();
	void Stop();

	SentrySettings _settings;
	bool _running {false};
};

} // namespace openblack::error_reports
