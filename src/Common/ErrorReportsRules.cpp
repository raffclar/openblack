/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ErrorReportsRules.h"

#include <cctype>

#include <algorithm>
#include <format>

namespace openblack::error_reports
{

std::optional<std::string> ChooseDsn(std::optional<std::string_view> fromEnvironment, std::string_view fromBuild)
{
	if (fromEnvironment.has_value() && !fromEnvironment->empty())
	{
		return std::string(*fromEnvironment);
	}
	if (!fromBuild.empty())
	{
		return std::string(fromBuild);
	}
	return std::nullopt;
}

std::string ReleaseName(std::string_view semanticVersion, std::string_view commit)
{
	return std::format("openblack@{}+{}", semanticVersion, commit);
}

std::string_view EnvironmentName(bool developerTools) noexcept
{
	return developerTools ? "development" : "production";
}

std::string_view RoleTag(Role role) noexcept
{
	switch (role)
	{
	case Role::Client:
		return "client";
	case Role::Server:
		return "server";
	}
	return "client";
}

std::optional<std::filesystem::path> LogAttachment(std::string_view logFile)
{
	if (logFile.empty() || logFile == "stdout" || logFile == "logcat")
	{
		return std::nullopt;
	}
	return std::filesystem::path(logFile);
}

std::filesystem::path DatabaseFolder(std::string_view logFile)
{
	const auto log = LogAttachment(logFile);
	if (!log.has_value())
	{
		return std::filesystem::path(k_DatabaseFolderName);
	}
	return log->parent_path() / k_DatabaseFolderName;
}

bool SendsCrashEvent(bool systemException, bool fromSignal, bool windows) noexcept
{
	if (windows)
	{
		return !systemException;
	}
	return !fromSignal;
}

std::optional<std::string> HideHomeFolder(std::string_view path, std::string_view homeFolder, bool windows)
{
	const auto separator = [](char c) { return c == '/' || c == '\\'; };
	// The home folder without a separator at its end
	while (!homeFolder.empty() && separator(homeFolder.back()))
	{
		homeFolder.remove_suffix(1);
	}
	if (homeFolder.empty() || path.size() < homeFolder.size())
	{
		return std::nullopt;
	}
	const auto same = [windows, separator](char a, char b) {
		if (separator(a) && separator(b))
		{
			return true;
		}
		if (windows)
		{
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		}
		return a == b;
	};
	if (!std::equal(homeFolder.begin(), homeFolder.end(), path.begin(), same))
	{
		return std::nullopt;
	}
	const auto rest = path.substr(homeFolder.size());
	// The folder must end where the home folder ends, so that "C:/Users/Ann" does not hide a part of "C:/Users/Anna"
	if (!rest.empty() && !separator(rest.front()))
	{
		return std::nullopt;
	}
	return "~" + std::string(rest);
}

std::string HideHomeFolderInText(std::string_view text, std::string_view homeFolder, bool windows)
{
	const auto separator = [](char c) { return c == '/' || c == '\\'; };
	while (!homeFolder.empty() && separator(homeFolder.back()))
	{
		homeFolder.remove_suffix(1);
	}
	if (homeFolder.empty())
	{
		return std::string(text);
	}
	std::string result;
	result.reserve(text.size());
	size_t at = 0;
	while (at < text.size())
	{
		// A path in the home folder at this place: the home folder, then a separator or the end of the text
		const auto rest = text.substr(at);
		const auto end = std::min(rest.size(), homeFolder.size() + 1);
		if (rest.size() >= homeFolder.size() && HideHomeFolder(rest.substr(0, end), homeFolder, windows).has_value())
		{
			result += '~';
			at += homeFolder.size();
			continue;
		}
		result += text[at];
		++at;
	}
	return result;
}

std::string TestErrorMessage(int64_t unixSeconds)
{
	return std::format("{} {}", k_TestErrorText, unixSeconds);
}

CrashEventTexts CrashEventTextsOf(const crash_report::CrashInfo& info)
{
	const auto kind = crash_report::KindName(info.kind);
	return {.type = std::string(info.detail.empty() ? kind : info.detail),
	        .message = std::string(info.message.empty() ? kind : info.message)};
}

} // namespace openblack::error_reports
