/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpText.h"

#include <cctype>
#include <cstdio>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace
{

std::u16string Utf16(const std::vector<uint8_t>& bytes)
{
	std::u16string text;
	size_t start = bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE ? 2 : 0;
	text.reserve((bytes.size() - start) / 2);
	for (size_t i = start; i + 1 < bytes.size(); i += 2)
	{
		text.push_back(static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8)));
	}
	return text;
}

bool IsBlank(char16_t c)
{
	return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n';
}

std::u16string_view Trim(std::u16string_view s)
{
	while (!s.empty() && IsBlank(s.front()))
	{
		s.remove_prefix(1);
	}
	while (!s.empty() && IsBlank(s.back()))
	{
		s.remove_suffix(1);
	}
	return s;
}

std::string Narrow(std::u16string_view s)
{
	std::string out;
	out.reserve(s.size());
	for (const auto c : s)
	{
		out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return out;
}

// A number as the leading digits (inferred: like _wtoi; the original script reader is not read). The declarations of
// W120 end with U+0A0D after the digits (a stray CR LF in the UTF-16 file).
std::optional<int32_t> Number(std::u16string_view s)
{
	s = Trim(s);
	bool negative = false;
	if (!s.empty() && s.front() == u'-')
	{
		negative = true;
		s.remove_prefix(1);
	}
	if (s.empty() || s.front() < u'0' || s.front() > u'9')
	{
		return std::nullopt;
	}
	int32_t value = 0;
	for (const auto c : s)
	{
		if (c < u'0' || c > u'9')
		{
			break;
		}
		value = value * 10 + static_cast<int32_t>(c - u'0');
	}
	return negative ? -value : value;
}

/// A help text database of Scripts\ (`file`), parsed once into the help text cache; a file that cannot be read is cached
/// empty, so it is tried once. Null without a file system or resources (the unit tests)
const std::vector<openblack::helptext::Entry>* Table(const char* file)
{
	using openblack::Locator;
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& texts = Locator::resources::value().GetHelpTexts();
	const auto id = entt::hashed_string(file).value();
	if (!texts.Contains(id))
	{
		auto& fileSystem = Locator::filesystem::value();
		try
		{
			const auto path = fileSystem.FindPath(fileSystem.GetPath<openblack::filesystem::Path::Scripts>() / file);
			texts.Load(id, openblack::resources::HelpTextLoader::FromDiskTag {}, path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Help texts: {} entries from {}", texts.Handle(id)->size(), file);
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Help texts: cannot read {}: {}", file, e.what());
			texts.Load(id, openblack::resources::HelpTextLoader::FromBufferTag {}, std::vector<uint8_t> {});
		}
	}
	return &*texts.Handle(id);
}

/// The language, read once per game (Locator::scriptState)
struct HelpTextLanguage
{
	std::optional<uint32_t> language;
};
} // namespace

uint32_t openblack::helptext::Language()
{
	// (openblack guard, not cached: the unit tests have no file system; the original always reads the file)
	if (!openblack::Locator::filesystem::has_value())
	{
		return 0;
	}
	auto& cached = openblack::Locator::scriptState::value().Get<HelpTextLanguage>().language;
	if (!cached.has_value())
	{
		cached = []() -> uint32_t {
			try
			{
				auto& fileSystem = openblack::Locator::filesystem::value();
				const auto& bytes = openblack::resources::LoadBlob(openblack::Locator::resources::value().GetBlobs(),
				                                                   fileSystem.FindPath("Country.txt")); // read as text
				return LanguageIndex(bytes);
			}
			catch (const std::exception&)
			{
				return 0; // no file
			}
		}();
	}
	return *cached;
}

uint32_t openblack::helptext::LanguageIndex(std::span<const uint8_t> countryFile)
{
	std::string code;
	for (size_t i = 0; i < countryFile.size() && i < 2 && countryFile[i] != 0; ++i) // two bytes
	{
		code.push_back(static_cast<char>(std::tolower(countryFile[i])));
	}
	constexpr std::array<std::string_view, 15> k_Codes = {"uk", "us", "fr", "de", "se", "es", "jp", "nl",
	                                                      "br", "it", "sc", "tc", "pl", "kr", "th"};
	for (uint32_t i = 0; i < k_Codes.size(); ++i)
	{
		if (code == k_Codes.at(i))
		{
			return i;
		}
	}
	return 0; // no match
}

bool openblack::helptext::NeedsBiggerText()
{
	const uint32_t language = Language();
	return language == 6 || language == 10 || language == 11 || language == 13 || language == 14;
}

std::u16string openblack::helptext::ConvertScriptText(std::u16string_view text)
{
	constexpr char16_t k_Space = 0xF8FE;
	std::u16string out;
	out.reserve(text.size());
	for (size_t i = 0; i < text.size();)
	{
		const char16_t c = text[i];
		const char16_t next = i + 1 < text.size() ? text[i + 1] : u'\0';
		if ((c == u' ' && next == u'~') || (c == u'~' && next == u' ')) // " ~" or "~ "
		{
			out.push_back(k_Space);
			i += 2;
		}
		else if (c == u'\\' && next == u'n') // a backslash and an n
		{
			out.push_back(u'\n');
			i += 2;
		}
		else
		{
			out.push_back(c == u'~' ? k_Space : c);
			++i;
		}
	}
	return out;
}

std::vector<openblack::helptext::Entry> openblack::helptext::Parse(const std::vector<uint8_t>& utf16)
{
	const auto text = Utf16(utf16);
	std::vector<std::u16string_view> lines;
	for (size_t pos = 0; pos < text.size();)
	{
		const auto end = text.find(u'\n', pos);
		const auto stop = end == std::u16string::npos ? text.size() : end;
		lines.emplace_back(std::u16string_view(text).substr(pos, stop - pos));
		pos = stop + 1;
	}

	// the declarations "HELP_TEXT_NARRATOR_GOOD_SPIRIT =  2"
	std::unordered_map<std::u16string, int32_t> values;
	for (const auto line : lines)
	{
		const auto equals = line.find(u'=');
		if (equals == std::u16string_view::npos || line.find(u"ADD_TEXT") != std::u16string_view::npos)
		{
			continue;
		}
		if (const auto value = Number(line.substr(equals + 1)))
		{
			values[std::u16string(Trim(line.substr(0, equals)))] = *value;
		}
	}
	const auto resolve = [&values](std::u16string_view argument) {
		if (const auto number = Number(argument))
		{
			return *number;
		}
		const auto found = values.find(std::u16string(Trim(argument)));
		return found != values.end() ? found->second : k_NarratorUndeclared;
	};

	std::vector<Entry> entries;
	for (const auto line : lines)
	{
		const auto command = line.find(u"ADD_TEXT");
		if (command == std::u16string_view::npos)
		{
			continue;
		}
		Entry entry;
		// the name is the first quoted string, the text the last one; the numbers come before the first quote
		const auto open = line.find(u'(', command);
		const auto nameOpen = line.find(u'"', command);
		const auto nameClose = nameOpen == std::u16string_view::npos ? nameOpen : line.find(u'"', nameOpen + 1);
		const auto textClose = line.rfind(u'"');
		const auto textOpen = textClose == std::u16string_view::npos || textClose == 0 ? std::u16string_view::npos
		                                                                               : line.rfind(u'"', textClose - 1);
		if (open != std::u16string_view::npos && nameOpen != std::u16string_view::npos && open < nameOpen)
		{
			const auto numbers = line.substr(open + 1, nameOpen - open - 1);
			const auto comma = numbers.find(u',');
			if (comma != std::u16string_view::npos)
			{
				entry.arg0 = resolve(numbers.substr(0, comma));
				const auto second = numbers.substr(comma + 1);
				entry.narrator = resolve(second.substr(0, second.find(u',')));
			}
		}
		if (nameClose != std::u16string_view::npos)
		{
			entry.name = Narrow(ConvertScriptText(line.substr(nameOpen + 1, nameClose - nameOpen - 1)));
		}
		if (textOpen != std::u16string_view::npos && textOpen > nameClose)
		{
			entry.text = ConvertScriptText(line.substr(textOpen + 1, textClose - textOpen - 1));
		}
		entries.push_back(std::move(entry));
	}
	return entries;
}

const openblack::helptext::Entry& openblack::helptext::GetEntry(uint32_t id)
{
	static const Entry k_Empty;
	const auto* entries = Table("InfoScript2.txt");
	if (entries == nullptr || entries->empty())
	{
		return k_Empty;
	}
	return id > 0 && id < entries->size() ? (*entries)[id] : (*entries)[0];
}

const std::u16string& openblack::helptext::GetPatch(uint32_t id)
{
	static const std::u16string k_Empty;
	const auto* patch = Table("InfoScriptPatch2.txt");
	if (patch == nullptr || patch->empty())
	{
		return k_Empty;
	}
	return id < patch->size() ? (*patch)[id].text : (*patch)[0].text;
}

const std::u16string& openblack::helptext::Get(uint32_t id)
{
	return GetEntry(id).text;
}

size_t openblack::helptext::Count()
{
	const auto* entries = Table("InfoScript2.txt");
	return entries != nullptr ? entries->size() : 0;
}

std::u16string openblack::helptext::Format(uint32_t id, double value)
{
	return Format(id, std::span<const double>(&value, 1));
}

std::u16string openblack::helptext::Format(uint32_t id, std::span<const double> values)
{
	const auto& text = Get(id);
	std::u16string out;
	size_t next = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] != u'%')
		{
			out.push_back(text[i]);
			continue;
		}
		if (i + 1 < text.size() && text[i + 1] == u'%')
		{
			out.push_back(u'%'); // "%%"
			++i;
			continue;
		}
		// the conversion: flags, width, precision up to its letter
		size_t end = i + 1;
		while (end < text.size() && std::u16string_view(u"-+ #0123456789.").find(text[end]) != std::u16string_view::npos)
		{
			++end;
		}
		if (end >= text.size())
		{
			out.append(text, i, std::u16string::npos);
			break;
		}
		if (next >= values.size())
		{
			out.append(text, i, end - i + 1); // no value left: the conversion as it is
			i = end;
			continue;
		}
		std::string spec;
		for (size_t k = i; k <= end; ++k)
		{
			spec.push_back(static_cast<char>(text[k]));
		}
		std::array<char, 64> buffer;
		const char letter = spec.back();
		if (letter == 'd' || letter == 'i')
		{
			std::snprintf(buffer.data(), buffer.size(), spec.c_str(), static_cast<int>(values[next]));
		}
		else if (std::string_view("eEfFgGaA").find(letter) != std::string_view::npos)
		{
			std::snprintf(buffer.data(), buffer.size(), spec.c_str(), values[next]);
		}
		else
		{
			// a conversion that does not take a double (%s, %c, %x ...): copied as it is, its value not used
			out.append(text, i, end - i + 1);
			i = end;
			continue;
		}
		++next;
		for (const char* c = buffer.data(); *c != '\0'; ++c)
		{
			out.push_back(static_cast<char16_t>(static_cast<unsigned char>(*c)));
		}
		i = end;
	}
	return out;
}
