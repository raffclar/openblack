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

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/// The rules that keep the names of the players and the email addresses out of the log that the reports attach. The
/// log keeps the turns, the keys of the desyncs and the build, which the checks of a desync use.
namespace openblack::error_reports
{

/// The text that the reports show in place of a name of a player, an account or a profile
constexpr std::string_view k_HiddenNameText = "<name>";
/// The text that the reports show in place of an email address
constexpr std::string_view k_HiddenEmailText = "<email>";
/// The shortest name that the reports hide. A shorter name would hide too much of the log.
constexpr size_t k_ShortestHiddenName = 2;

/// The text with each email address changed to k_HiddenEmailText
[[nodiscard]] std::string HideEmails(std::string_view text);

/// The names that the reports keep out of the log. The names have no difference for upper case. A name matches only
/// as a full word: the characters before and after it are not letters, digits or "_".
class NameRedactor
{
public:
	/// Adds the name. False when the redactor has the name already, or the name is too short or has only digits. A name
	/// with only digits would hide the turns and the other numbers of the log.
	bool Add(std::string_view name);
	[[nodiscard]] size_t Count() const noexcept { return _names.size(); }
	/// The text with each name changed to k_HiddenNameText and each email address changed to k_HiddenEmailText
	[[nodiscard]] std::string Redact(std::string_view text) const;

private:
	/// The names in lower case
	std::set<std::string, std::less<>> _names;
	/// The names in lower case, the longest first, so that a long name matches before a name at its start
	std::vector<std::string> _byLength;
};

/// The names that the reports hide, for more than one thread. The thread of the game adds names and the thread of the
/// log redacts the lines. The redaction also changes the home folder of the user to "~".
class ReportNames
{
public:
	ReportNames(std::string homeFolder, bool windows);

	/// Adds the name. False when the name is there already or the redactor refuses it.
	bool Add(std::string_view name);
	/// A number that increases each time that a name comes
	[[nodiscard]] uint64_t Version() const;
	[[nodiscard]] std::string Redact(std::string_view text) const;

private:
	mutable std::mutex _mutex;
	NameRedactor _redactor;
	uint64_t _version {0};
	std::string _homeFolder;
	bool _windows {false};
};

/// The fields of an event (FailureEvent or DesyncEventFields) with the names hidden in the message, the tags and the
/// extra values. The fingerprint stays the same, so that the event goes into the same issue.
template <typename Fields>
[[nodiscard]] Fields HideNamesInFields(Fields fields, const ReportNames& names)
{
	fields.message = names.Redact(fields.message);
	for (auto& [key, value] : fields.tags)
	{
		value = names.Redact(value);
	}
	for (auto& [key, value] : fields.extras)
	{
		value = names.Redact(value);
	}
	return fields;
}

/// The file of the log that the reports attach
class ReportLogStorage
{
public:
	virtual ~ReportLogStorage() = default;
	/// All the text of the file. Empty when there is no file.
	[[nodiscard]] virtual std::string Read() = 0;
	/// Writes the text in place of all the text of the file
	virtual void Replace(std::string_view text) = 0;
	/// Writes the text at the end of the file
	virtual void Append(std::string_view text) = 0;
};

/// Writes the lines of the log, with the names hidden, to the file that the reports attach. The first write makes the
/// file empty. A name can come after the log wrote it. Thus, when names came after the last write, the next write
/// redacts all the file again.
class ReportLogWriter
{
public:
	ReportLogWriter(std::unique_ptr<ReportLogStorage> storage, std::shared_ptr<const ReportNames> names);

	void Write(std::string_view lines);

private:
	std::unique_ptr<ReportLogStorage> _storage;
	std::shared_ptr<const ReportNames> _names;
	bool _started {false};
	uint64_t _version {0};
};

/// The file of the log that the reports attach: next to the log file, with ".report" before the extension
[[nodiscard]] std::filesystem::path ReportLogPath(const std::filesystem::path& logFile);
/// The storage of a file on the disk
[[nodiscard]] std::unique_ptr<ReportLogStorage> ReportLogFile(std::filesystem::path path);

} // namespace openblack::error_reports
