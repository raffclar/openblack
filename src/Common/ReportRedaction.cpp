/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ReportRedaction.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <ranges>
#include <utility>

#include "ErrorReportsRules.h"

namespace openblack::error_reports
{

namespace
{

[[nodiscard]] constexpr char Lower(char c) noexcept
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] constexpr bool IsDigit(char c) noexcept
{
	return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr bool IsLetterOrDigit(char c) noexcept
{
	return IsDigit(c) || (Lower(c) >= 'a' && Lower(c) <= 'z');
}

/// A character of a word. The bytes of a UTF-8 character that is not ASCII are also characters of a word.
[[nodiscard]] constexpr bool IsWordCharacter(char c) noexcept
{
	return IsLetterOrDigit(c) || c == '_' || static_cast<unsigned char>(c) >= 0x80;
}

[[nodiscard]] constexpr bool IsLocalCharacter(char c) noexcept
{
	return IsLetterOrDigit(c) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-';
}

[[nodiscard]] constexpr bool IsDomainCharacter(char c) noexcept
{
	return IsLetterOrDigit(c) || c == '.' || c == '-';
}

/// The end of the domain of an email address that starts after the "@" at the position, or the position when there is
/// no domain. A domain has a dot, with text before it and two letters or more after the last dot.
[[nodiscard]] size_t DomainEnd(std::string_view text, size_t at)
{
	auto end = at + 1;
	while (end < text.size() && IsDomainCharacter(text[end]))
	{
		++end;
	}
	while (end > at + 1 && (text[end - 1] == '.' || text[end - 1] == '-'))
	{
		--end;
	}
	const auto domain = text.substr(at + 1, end - at - 1);
	const auto dot = domain.rfind('.');
	if (dot == std::string_view::npos || dot == 0 || domain.size() - dot - 1 < 2)
	{
		return at;
	}
	return end;
}

[[nodiscard]] bool MatchesAt(std::string_view text, size_t at, std::string_view lowerName) noexcept
{
	if (text.size() - at < lowerName.size())
	{
		return false;
	}
	for (size_t i = 0; i < lowerName.size(); ++i)
	{
		if (Lower(text[at + i]) != lowerName[i])
		{
			return false;
		}
	}
	const auto after = at + lowerName.size();
	return after == text.size() || !IsWordCharacter(text[after]);
}

/// The file of the log that the reports attach, on the disk
class FileStorage final: public ReportLogStorage
{
public:
	explicit FileStorage(std::filesystem::path path)
	    : _path(std::move(path))
	{
	}

	[[nodiscard]] std::string Read() override
	{
		std::ifstream file(_path, std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}

	void Replace(std::string_view text) override { Write(text, std::ios::binary | std::ios::trunc); }

	void Append(std::string_view text) override { Write(text, std::ios::binary | std::ios::app); }

private:
	void Write(std::string_view text, std::ios::openmode mode)
	{
		std::ofstream file(_path, mode);
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
	}

	std::filesystem::path _path;
};

} // namespace

std::string HideEmails(std::string_view text)
{
	std::string result;
	result.reserve(text.size());
	size_t copied = 0;
	for (auto at = text.find('@'); at != std::string_view::npos; at = text.find('@', at + 1))
	{
		auto start = at;
		while (start > copied && IsLocalCharacter(text[start - 1]))
		{
			--start;
		}
		const auto end = DomainEnd(text, at);
		if (start == at || end == at)
		{
			continue;
		}
		result.append(text.substr(copied, start - copied));
		result.append(k_HiddenEmailText);
		copied = end;
		at = end - 1;
	}
	result.append(text.substr(copied));
	return result;
}

bool NameRedactor::Add(std::string_view name)
{
	if (name.size() < k_ShortestHiddenName || std::ranges::all_of(name, IsDigit))
	{
		return false;
	}
	std::string lower(name);
	std::ranges::transform(lower, lower.begin(), Lower);
	if (_names.contains(lower))
	{
		return false;
	}
	const auto place = std::ranges::upper_bound(_byLength, lower.size(), std::greater<>(),
	                                            [](const std::string& each) { return each.size(); });
	_byLength.insert(place, lower);
	_names.insert(std::move(lower));
	return true;
}

std::string NameRedactor::Redact(std::string_view text) const
{
	auto hidden = HideEmails(text);
	if (_byLength.empty())
	{
		return hidden;
	}
	const std::string_view view(hidden);
	std::string result;
	result.reserve(view.size());
	size_t at = 0;
	while (at < view.size())
	{
		if (at == 0 || !IsWordCharacter(view[at - 1]))
		{
			const auto found = std::ranges::find_if(_byLength, [&](const auto& name) { return MatchesAt(view, at, name); });
			if (found != _byLength.end())
			{
				result.append(k_HiddenNameText);
				at += found->size();
				continue;
			}
		}
		result += view[at];
		++at;
	}
	return result;
}

ReportNames::ReportNames(std::string homeFolder, bool windows)
    : _homeFolder(std::move(homeFolder))
    , _windows(windows)
{
}

bool ReportNames::Add(std::string_view name)
{
	const std::lock_guard lock(_mutex);
	if (!_redactor.Add(name))
	{
		return false;
	}
	++_version;
	return true;
}

uint64_t ReportNames::Version() const
{
	const std::lock_guard lock(_mutex);
	return _version;
}

std::string ReportNames::Redact(std::string_view text) const
{
	// The home folder goes first, because a part of the folder can be a name
	const auto withoutHome = HideHomeFolderInText(text, _homeFolder, _windows);
	const std::lock_guard lock(_mutex);
	return _redactor.Redact(withoutHome);
}

ReportLogWriter::ReportLogWriter(std::unique_ptr<ReportLogStorage> storage, std::shared_ptr<const ReportNames> names)
    : _storage(std::move(storage))
    , _names(std::move(names))
{
}

void ReportLogWriter::Write(std::string_view lines)
{
	const auto version = _names->Version();
	if (!_started)
	{
		_storage->Replace(_names->Redact(lines));
		_started = true;
		_version = version;
		return;
	}
	// A name that came after the last write can be in the lines that the file has already
	if (version != _version)
	{
		_storage->Replace(_names->Redact(_storage->Read()));
		_version = version;
	}
	_storage->Append(_names->Redact(lines));
}

std::filesystem::path ReportLogPath(const std::filesystem::path& logFile)
{
	auto name = logFile.stem();
	name += ".report";
	name += logFile.extension();
	return logFile.parent_path() / name;
}

std::unique_ptr<ReportLogStorage> ReportLogFile(std::filesystem::path path)
{
	return std::make_unique<FileStorage>(std::move(path));
}

} // namespace openblack::error_reports
