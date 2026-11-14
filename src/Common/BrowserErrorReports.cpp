/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "BrowserErrorReports.h"

#include <cstdlib>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <emscripten/em_js.h>
#include <emscripten/threading.h>
#include <spdlog/spdlog.h>

#include "CrashHandler.h"
#include "DesyncReports.h"
#include "ErrorReportsRules.h"

namespace
{

// The stack of the thread that calls, as the browser writes it, with the frames of the WebAssembly code
// clang-format off
EM_JS(char*, openblack_reports_stack, (), {
	return stringToNewUTF8(String(new Error().stack));
});
// clang-format on
EM_JS_DEPS(openblack_browser_error_reports, "$UTF8ToString,$stringToNewUTF8");

} // namespace

namespace openblack::error_reports
{

namespace
{

void LogInfo(std::string_view text)
{
	if (auto logger = spdlog::get("game"))
	{
		logger->info("{}", text);
	}
}

/// The texts of the list as C strings, which the page reads until the call returns
std::vector<const char*> CStrings(const std::vector<std::string>& texts)
{
	std::vector<const char*> strings;
	strings.reserve(texts.size());
	for (const auto& text : texts)
	{
		strings.push_back(text.c_str());
	}
	return strings;
}

/// The pairs as one list of C strings: the name, then the value, of each pair
std::vector<const char*> CStrings(const std::vector<std::pair<std::string, std::string>>& pairs)
{
	std::vector<const char*> strings;
	strings.reserve(pairs.size() * 2);
	for (const auto& [name, value] : pairs)
	{
		strings.push_back(name.c_str());
		strings.push_back(value.c_str());
	}
	return strings;
}

/// Gives a crash to the page after the crash handler wrote its report: the type, the message, the text of the report
/// and the stack of this thread. The call waits until the page has the event, because the program ends next.
void ForwardCrash(const crash_report::CrashInfo& info, std::string_view report, void* /*exceptionPointers*/)
{
	const auto texts = CrashEventTextsOf(info);
	const std::string reportText(report);
	const std::unique_ptr<char, decltype(&std::free)> stack(openblack_reports_stack(), &std::free);
	// clang-format off
	MAIN_THREAD_EM_ASM({
		if (globalThis.openblackErrorReports) {
			globalThis.openblackErrorReports.crash(UTF8ToString($0), UTF8ToString($1), UTF8ToString($2),
				UTF8ToString($3));
		}
	}, texts.type.c_str(), texts.message.c_str(), reportText.c_str(), stack ? stack.get() : "");
	// clang-format on
}

} // namespace

BrowserErrorReports::BrowserErrorReports(BrowserReportSettings settings)
    : _settings(std::move(settings))
{
}

BrowserErrorReports::~BrowserErrorReports()
{
	Stop();
}

void BrowserErrorReports::SetEnabled(bool enabled)
{
	_enabled = enabled;
	if (enabled)
	{
		Start();
	}
	else
	{
		Stop();
	}
}

void BrowserErrorReports::SendDesync(const DesyncReport& report)
{
	if (!_running)
	{
		return;
	}
	const auto fields = DesyncEventFieldsOf(report);
	const auto fingerprint = CStrings(fields.fingerprint);
	const auto tags = CStrings(fields.tags);
	const auto extras = CStrings(fields.extras);
	// The page reads the lists of strings before the call returns
	// clang-format off
	MAIN_THREAD_EM_ASM({
		if (!globalThis.openblackErrorReports) {
			return;
		}
		const strings = (at, count) => {
			const list = [];
			for (let i = 0; i < count; ++i) {
				list.push(UTF8ToString(HEAPU32[(at >> 2) + i]));
			}
			return list;
		};
		const pairs = (list) => {
			const result = [];
			for (let i = 0; i + 1 < list.length; i += 2) {
				result.push([list[i], list[i + 1]]);
			}
			return result;
		};
		globalThis.openblackErrorReports.desync({
			message: UTF8ToString($0), level: UTF8ToString($1), fingerprint: strings($2, $3),
			tags: pairs(strings($4, $5)), extras: pairs(strings($6, $7)),
		});
	}, fields.message.c_str(), fields.level.c_str(), fingerprint.data(), fingerprint.size(), tags.data(), tags.size(),
	   extras.data(), extras.size());
	// clang-format on
	++_desyncEvents;
}

void BrowserErrorReports::Start()
{
	if (_running || !HasDestination())
	{
		return;
	}
	const std::string role(RoleTag(_settings.role));
	// clang-format off
	const int started = MAIN_THREAD_EM_ASM_INT({
		if (!globalThis.openblackErrorReports) {
			return 0;
		}
		return globalThis.openblackErrorReports.start({
			dsn: UTF8ToString($0), release: UTF8ToString($1), environment: UTF8ToString($2), role: UTF8ToString($3),
		}) ? 1 : 0;
	}, _settings.dsn.c_str(), _settings.release.c_str(), _settings.environment.c_str(), role.c_str());
	// clang-format on
	if (started == 0)
	{
		if (auto logger = spdlog::get("game"))
		{
			logger->warn("The error reports did not start: the page has no reports");
		}
		return;
	}
	// The crash handler of the game writes its own report first, then gives the crash to the page
	crash_handler::SetReportForwarder(ForwardCrash);
	_running = true;
	LogInfo("The game gave the error reports to the page");
}

void BrowserErrorReports::Stop()
{
	if (!_running)
	{
		return;
	}
	crash_handler::SetReportForwarder(nullptr);
	// clang-format off
	MAIN_THREAD_EM_ASM({
		if (globalThis.openblackErrorReports) {
			globalThis.openblackErrorReports.stop();
		}
	});
	// clang-format on
	_running = false;
	LogInfo("The game stopped the error reports of the page");
}

} // namespace openblack::error_reports
