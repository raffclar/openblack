/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SentryErrorReports.h"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include <sentry.h>
#include <spdlog/spdlog.h>

#include "CrashHandler.h"
#include "DesyncReports.h"
#include "ErrorReportsRules.h"

namespace openblack::error_reports
{

namespace
{

#ifdef _WIN32
constexpr bool k_Windows = true;
#else
constexpr bool k_Windows = false;
#endif

void LogInfo(std::string_view text)
{
	if (auto logger = spdlog::get("game"))
	{
		logger->info("{}", text);
	}
}

/// Gives a crash to Sentry after the crash handler wrote its report. A system exception on Windows goes to the
/// crashpad handler, which writes a minidump and ends the process. Another crash goes as a fatal event with the stack
/// of this thread and the text of the report.
void ForwardCrash(const crash_report::CrashInfo& info, std::string_view report, void* exceptionPointers)
{
#ifdef _WIN32
	if (exceptionPointers != nullptr)
	{
		sentry_ucontext_t context {};
		context.exception_ptrs = *static_cast<EXCEPTION_POINTERS*>(exceptionPointers);
		sentry_handle_exception(&context);
		return;
	}
#endif
	if (!SendsCrashEvent(exceptionPointers != nullptr, info.fromSignal, k_Windows))
	{
		return;
	}
	const auto texts = CrashEventTextsOf(info);
	auto event = sentry_value_new_event();
	sentry_value_set_by_key(event, "level", sentry_value_new_string("fatal"));
	auto exception = sentry_value_new_exception(texts.type.c_str(), texts.message.c_str());
	sentry_value_set_stacktrace(exception, nullptr, 0);
	sentry_event_add_exception(event, exception);
	auto extra = sentry_value_new_object();
	const std::string reportText(report);
	sentry_value_set_by_key(extra, "crash_report", sentry_value_new_string(reportText.c_str()));
	sentry_value_set_by_key(event, "extra", extra);
	sentry_capture_event(event);
	sentry_flush(k_CrashFlushMilliseconds);
}

/// The fields of a module in the list of the modules of an event
constexpr std::array k_ImageKeys = {"type",      "arch",    "image_addr", "image_size",
                                    "code_file", "code_id", "debug_file", "debug_id"};

/// Changes the home folder of the user to "~" in the paths of the program's modules and in the report of the crash
/// handler, before each event goes. The paths otherwise hold the name of the user.
sentry_value_t HideUserFolders(sentry_value_t event, sentry_hint_t* /*hint*/, void* homeFolder)
{
	const auto& home = *static_cast<const std::string*>(homeFolder);
	// sentry-native keeps one frozen list of the modules for all events. Thus the event gets a new list with new
	// objects, which have the hidden paths.
	auto debugMeta = sentry_value_get_by_key(event, "debug_meta");
	const auto images = sentry_value_get_by_key(debugMeta, "images");
	const auto count = sentry_value_get_length(images);
	auto hiddenImages = sentry_value_new_list();
	for (size_t i = 0; i < count; ++i)
	{
		const auto image = sentry_value_get_by_index(images, i);
		auto copy = sentry_value_new_object();
		for (const char* key : k_ImageKeys)
		{
			auto value = sentry_value_get_by_key(image, key);
			if (sentry_value_is_null(value))
			{
				continue;
			}
			const auto* text = sentry_value_as_string(value);
			const bool path = std::string_view(key) == "code_file" || std::string_view(key) == "debug_file";
			if (const auto hidden = path ? HideHomeFolder(text, home, k_Windows) : std::nullopt; hidden.has_value())
			{
				sentry_value_set_by_key(copy, key, sentry_value_new_string(hidden->c_str()));
				continue;
			}
			sentry_value_incref(value);
			sentry_value_set_by_key(copy, key, value);
		}
		sentry_value_append(hiddenImages, copy);
	}
	if (count > 0)
	{
		sentry_value_set_by_key(debugMeta, "images", hiddenImages);
	}
	else
	{
		sentry_value_decref(hiddenImages);
	}
	// The report of the crash handler names the files of the crash, such as the minidump
	auto extra = sentry_value_get_by_key(event, "extra");
	if (const auto* report = sentry_value_as_string(sentry_value_get_by_key(extra, "crash_report"));
	    report != nullptr && *report != '\0')
	{
		const auto hidden = HideHomeFolderInText(report, home, k_Windows);
		sentry_value_set_by_key(extra, "crash_report", sentry_value_new_string(hidden.c_str()));
	}
	return event;
}

} // namespace

SentryErrorReports::SentryErrorReports(SentrySettings settings)
    : _settings(std::move(settings))
{
}

SentryErrorReports::~SentryErrorReports()
{
	Stop();
}

void SentryErrorReports::SetEnabled(bool enabled)
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

void SentryErrorReports::SendDesync(const DesyncReport& report)
{
	if (!_running)
	{
		return;
	}
	const auto fields = DesyncEventFieldsOf(report);
	const auto level = fields.level == "info" ? SENTRY_LEVEL_INFO : SENTRY_LEVEL_WARNING;
	auto event = sentry_value_new_message_event(level, "desync", fields.message.c_str());
	auto fingerprint = sentry_value_new_list();
	for (const auto& part : fields.fingerprint)
	{
		sentry_value_append(fingerprint, sentry_value_new_string(part.c_str()));
	}
	sentry_value_set_by_key(event, "fingerprint", fingerprint);
	auto tags = sentry_value_new_object();
	for (const auto& [key, value] : fields.tags)
	{
		sentry_value_set_by_key(tags, key.c_str(), sentry_value_new_string(value.c_str()));
	}
	sentry_value_set_by_key(event, "tags", tags);
	auto extra = sentry_value_new_object();
	for (const auto& [key, value] : fields.extras)
	{
		sentry_value_set_by_key(extra, key.c_str(), sentry_value_new_string(value.c_str()));
	}
	sentry_value_set_by_key(event, "extra", extra);
	sentry_capture_event(event);
	++_desyncEvents;
}

void SentryErrorReports::Start()
{
	if (_running || !HasDestination())
	{
		return;
	}
	auto* options = sentry_options_new();
	sentry_options_set_dsn(options, _settings.dsn.c_str());
	sentry_options_set_release(options, _settings.release.c_str());
	sentry_options_set_environment(options, _settings.environment.c_str());
	// The reports have no user and no IP address: the service never sets a user, and sentry-native sends no personal
	// data when there is no user
	// The reports have no traces of the performance
	sentry_options_set_traces_sample_rate(options, 0.0);
	sentry_options_set_shutdown_timeout(options, k_CloseMilliseconds);
	if (!_settings.homeFolder.empty())
	{
		sentry_options_set_before_send(options, HideUserFolders, &_settings.homeFolder);
	}
#ifdef _WIN32
	sentry_options_set_database_pathw(options, _settings.databaseFolder.wstring().c_str());
	if (_settings.attachment.has_value())
	{
		sentry_options_add_attachmentw(options, _settings.attachment->wstring().c_str());
	}
#else
	sentry_options_set_database_path(options, _settings.databaseFolder.string().c_str());
	if (_settings.attachment.has_value())
	{
		sentry_options_add_attachment(options, _settings.attachment->string().c_str());
	}
#endif
	if (sentry_init(options) != 0)
	{
		if (auto logger = spdlog::get("game"))
		{
			logger->warn("The error reports did not start: Sentry did not start its crash handler");
		}
		return;
	}
	const std::string roleTag(k_RoleTagName);
	const std::string role(RoleTag(_settings.role));
	sentry_set_tag(roleTag.c_str(), role.c_str());
	// The crash handler of the game writes its own report first, then gives the crash to Sentry
	crash_handler::ReclaimSystemHooks();
	crash_handler::SetReportForwarder(ForwardCrash);
	_running = true;
	LogInfo("The error reports to Sentry started");
}

void SentryErrorReports::Stop()
{
	if (!_running)
	{
		return;
	}
	crash_handler::SetReportForwarder(nullptr);
	sentry_close();
	_running = false;
	LogInfo("The error reports to Sentry stopped");
}

} // namespace openblack::error_reports
