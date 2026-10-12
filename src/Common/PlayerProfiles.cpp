/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerProfiles.h"

#include <charconv>
#include <cstddef>

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>

#include "Common/GameRandom.h"
#include "Gui/TextDatabase.h"

namespace openblack::player_profiles
{
namespace
{
/// The seed the scrambling's random numbers always start from
constexpr uint32_t k_ScrambleSeed = 0x913;
constexpr std::string_view k_CurrentKey = "current";
constexpr std::string_view k_ProfileKey = "profile";
constexpr std::string_view k_SettingKey = "setting";
constexpr char k_Separator = '\t';

char16_t FoldCase(char16_t c)
{
	return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

/// The profiles are listed by name, the case of plain letters ignored
bool ListedBefore(const Profile& a, const Profile& b)
{
	const auto first = gui::ToUtf16(a.name);
	const auto second = gui::ToUtf16(b.name);
	return std::ranges::lexicographical_compare(first, second, {}, FoldCase, FoldCase);
}

void SortForTheList(Profiles& profiles)
{
	std::ranges::stable_sort(profiles.profiles, ListedBefore);
}

/// A setting as it is kept: its name and how its value is written and read
struct SettingField
{
	std::string_view name;
	std::string (*write)(const Settings& settings);
	bool (*read)(Settings& settings, std::string_view text);
};

template <typename T>
bool ParseNumber(std::string_view text, T& value)
{
	T parsed {};
	const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
	if (result.ec != std::errc {} || result.ptr != text.data() + text.size())
	{
		return false;
	}
	value = parsed;
	return true;
}

/// A fraction written in the classic locale, whatever the computer's
bool ParseFraction(std::string_view text, float& value)
{
	std::istringstream stream {std::string(text)};
	stream.imbue(std::locale::classic());
	float parsed = 0.0f;
	stream >> parsed;
	if (!stream || stream.peek() != std::char_traits<char>::eof())
	{
		return false;
	}
	value = parsed;
	return true;
}

bool ParseFlag(std::string_view text, bool& value)
{
	int parsed = 0;
	if (!ParseNumber(text, parsed) || (parsed != 0 && parsed != 1))
	{
		return false;
	}
	value = parsed == 1;
	return true;
}

// clang-format off
constexpr std::array k_SettingFields = {
    SettingField {"tooltips", [](const Settings& s) { return std::to_string(s.toolTips); }, [](Settings& s, std::string_view t) { return ParseNumber(t, s.toolTips); }},
    SettingField {"story_text", [](const Settings& s) { return std::to_string(s.storyText); }, [](Settings& s, std::string_view t) { return ParseNumber(t, s.storyText); }},
    SettingField {"help_level", [](const Settings& s) { return std::to_string(s.helpLevel); }, [](Settings& s, std::string_view t) { return ParseNumber(t, s.helpLevel); }},
    SettingField {"creature_help", [](const Settings& s) { return std::to_string(static_cast<int>(s.creatureHelp)); }, [](Settings& s, std::string_view t) { return ParseFlag(t, s.creatureHelp); }},
    SettingField {"read_speed", [](const Settings& s) { return std::format("{}", s.readSpeed); }, [](Settings& s, std::string_view t) { return ParseFraction(t, s.readSpeed); }},
    SettingField {"text_from_bottom", [](const Settings& s) { return std::to_string(static_cast<int>(s.textFromBottom)); }, [](Settings& s, std::string_view t) { return ParseFlag(t, s.textFromBottom); }},
    SettingField {"left_handed", [](const Settings& s) { return std::to_string(static_cast<int>(s.leftHanded)); }, [](Settings& s, std::string_view t) { return ParseFlag(t, s.leftHanded); }},
    SettingField {"push_scrolling", [](const Settings& s) { return std::to_string(static_cast<int>(s.pushScrolling)); }, [](Settings& s, std::string_view t) { return ParseFlag(t, s.pushScrolling); }},
    SettingField {"auto_save", [](const Settings& s) { return std::to_string(static_cast<int>(s.autoSave)); }, [](Settings& s, std::string_view t) { return ParseFlag(t, s.autoSave); }},
};
// clang-format on

/// The next field of a line, and the rest of the line after its separator
std::string_view NextField(std::string_view& line)
{
	const auto end = line.find(k_Separator);
	const auto field = line.substr(0, end);
	line = end == std::string_view::npos ? std::string_view {} : line.substr(end + 1);
	return field;
}
} // namespace

uint32_t ScrambleTicks(uint32_t ticks)
{
	uint32_t seed = k_ScrambleSeed;
	uint32_t scrambled = 0;
	for (uint32_t byte = 0; byte < 4; ++byte)
	{
		const uint32_t shift = byte * 8;
		// Only the low byte of the random number counts, the sum wrapping within the byte
		const auto value = static_cast<uint8_t>(((ticks >> shift) & 0xFFu) + static_cast<uint32_t>(game_random::CrtRand(seed)));
		scrambled |= static_cast<uint32_t>(value) << shift;
	}
	return scrambled;
}

std::string CreatureFileName(uint32_t ticks)
{
	return std::format("C{:x}.erc", ScrambleTicks(ticks));
}

bool SameName(std::u16string_view a, std::u16string_view b)
{
	return std::ranges::equal(a, b, [](char16_t x, char16_t y) { return FoldCase(x) == FoldCase(y); });
}

NameCheck CheckNewName(const Profiles& profiles, std::u16string_view name)
{
	if (Find(profiles, name) != nullptr)
	{
		return NameCheck::AlreadyExists;
	}
	if (name.empty() || name.size() > k_LongestName)
	{
		return NameCheck::Invalid;
	}
	return NameCheck::Accepted;
}

const Profile* Current(const Profiles& profiles)
{
	const auto found = std::ranges::find(profiles.profiles, profiles.current, &Profile::name);
	return found != profiles.profiles.end() ? &*found : nullptr;
}

const Profile* Find(const Profiles& profiles, std::u16string_view name)
{
	if (name.empty())
	{
		return nullptr;
	}
	const auto found = std::ranges::find_if(
	    profiles.profiles, [name](const Profile& profile) { return SameName(gui::ToUtf16(profile.name), name); });
	return found != profiles.profiles.end() ? &*found : nullptr;
}

const Profile& Add(Profiles& profiles, std::u16string_view name, std::u16string_view accountName, std::string_view creatureFile,
                   int symbol)
{
	const auto given = !name.empty()          ? std::u16string(name)
	                   : !accountName.empty() ? std::u16string(accountName)
	                                          : gui::ToUtf16(k_UnnamedPlayer);
	const auto* found = Find(profiles, given);
	if (found == nullptr)
	{
		const auto text = gui::ToUtf8(given);
		profiles.profiles.push_back(
		    {.name = text, .creatureFile = std::string(creatureFile), .creatureName = text, .symbol = symbol, .settings = {}});
		SortForTheList(profiles);
		found = Find(profiles, given);
	}
	if (profiles.current.empty())
	{
		profiles.current = found->name;
	}
	return *found;
}

std::optional<size_t> IndexOf(const Profiles& profiles, std::u16string_view name)
{
	const auto* found = Find(profiles, name);
	if (found == nullptr)
	{
		return std::nullopt;
	}
	return static_cast<size_t>(found - profiles.profiles.data());
}

bool Select(Profiles& profiles, std::u16string_view name)
{
	const auto* found = Find(profiles, name);
	if (found == nullptr)
	{
		return false;
	}
	profiles.current = found->name;
	return true;
}

bool Remove(Profiles& profiles, std::u16string_view name)
{
	const auto index = IndexOf(profiles, name);
	if (!index.has_value())
	{
		return false;
	}
	const auto removed = profiles.profiles.at(*index).name;
	profiles.profiles.erase(profiles.profiles.begin() + static_cast<std::ptrdiff_t>(*index));
	if (profiles.current == removed)
	{
		profiles.current.clear();
	}
	return true;
}

void Edit(Profile& profile, std::u16string_view creatureName, int symbol)
{
	profile.creatureName = creatureName.empty() ? profile.name : gui::ToUtf8(creatureName);
	profile.symbol = symbol >= 0 && symbol < k_SymbolCount ? symbol : 0;
}

std::string Write(const Profiles& profiles)
{
	std::string text = std::format("{}{}{}\n", k_CurrentKey, k_Separator, profiles.current);
	for (const auto& profile : profiles.profiles)
	{
		text += std::format("{1}{0}{2}{0}{3}{0}{4}{0}{5}\n", k_Separator, k_ProfileKey, profile.name, profile.creatureFile,
		                    profile.symbol, profile.creatureName);
		for (const auto& field : k_SettingFields)
		{
			text += std::format("{1}{0}{2}{0}{3}{0}{4}\n", k_Separator, k_SettingKey, profile.name, field.name,
			                    field.write(profile.settings));
		}
	}
	return text;
}

Profiles Read(std::string_view text)
{
	Profiles profiles;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r')
		{
			line.remove_suffix(1);
		}
		const auto key = NextField(line);
		if (key == k_CurrentKey)
		{
			profiles.current = std::string(line);
			continue;
		}
		if (key == k_SettingKey)
		{
			const auto owner = NextField(line);
			const auto name = NextField(line);
			const auto field = std::ranges::find(k_SettingFields, name, &SettingField::name);
			const auto profile = std::ranges::find(profiles.profiles, owner, &Profile::name);
			if (field != k_SettingFields.end() && profile != profiles.profiles.end())
			{
				// A value that can't be read leaves the setting as it was
				auto settings = profile->settings;
				if (field->read(settings, line))
				{
					profile->settings = settings;
				}
			}
			continue;
		}
		if (key != k_ProfileKey)
		{
			continue;
		}
		const auto name = NextField(line);
		const auto file = NextField(line);
		const auto symbolText = NextField(line);
		int symbol = 0;
		const auto parsed = std::from_chars(symbolText.data(), symbolText.data() + symbolText.size(), symbol);
		if (name.empty() || file.empty() || parsed.ec != std::errc {})
		{
			continue;
		}
		profiles.profiles.push_back({.name = std::string(name),
		                             .creatureFile = std::string(file),
		                             .creatureName = std::string(line),
		                             .symbol = symbol,
		                             .settings = {}});
	}
	SortForTheList(profiles);
	return profiles;
}

Store StoreFor(const RunOptions& options)
{
	if (options.profileFolder.has_value())
	{
		return {.folder = options.profileFolder,
		        .creatureMinds = *options.profileFolder / "Scripts" / "CreatureMind",
		        .developer = false};
	}
	if (!options.userFolder.has_value())
	{
		return {.folder = std::nullopt, .creatureMinds = std::nullopt, .developer = options.automated};
	}
	if (options.automated)
	{
		const auto folder = *options.userFolder / k_DeveloperFolder;
		return {.folder = folder, .creatureMinds = folder / "Scripts" / "CreatureMind", .developer = true};
	}
	return {
	    .folder = options.userFolder, .creatureMinds = *options.userFolder / "Scripts" / "CreatureMind", .developer = false};
}

} // namespace openblack::player_profiles
