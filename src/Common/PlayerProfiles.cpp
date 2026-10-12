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

#include <algorithm>
#include <format>
#include <iterator>

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
constexpr char k_Separator = '\t';

char16_t FoldCase(char16_t c)
{
	return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c - u'A' + u'a') : c;
}

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
		    {.name = text, .creatureFile = std::string(creatureFile), .creatureName = text, .symbol = symbol});
		found = &profiles.profiles.back();
	}
	if (profiles.current.empty())
	{
		profiles.current = found->name;
	}
	return *found;
}

std::string Write(const Profiles& profiles)
{
	std::string text = std::format("{}{}{}\n", k_CurrentKey, k_Separator, profiles.current);
	for (const auto& profile : profiles.profiles)
	{
		text += std::format("{1}{0}{2}{0}{3}{0}{4}{0}{5}\n", k_Separator, k_ProfileKey, profile.name, profile.creatureFile,
		                    profile.symbol, profile.creatureName);
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
		                             .symbol = symbol});
	}
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
