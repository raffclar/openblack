/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkirmishBoxRules.h"

#include <algorithm>
#include <array>
#include <utility>

using namespace openblack::gui;

namespace
{
/// The extension of the files that the list shows
constexpr std::u16string_view k_Extension = u".txt";

/// The lands that show a text in place of their names, and the texts
constexpr std::array<std::pair<std::u16string_view, std::string_view>, 3> k_TitleTexts = {{
    {u"TwoGods", "HELP_TEXT_LAST_LINES_01"},
    {u"ThreeGods", "HELP_TEXT_LAST_LINES_02"},
    {u"FourGods", "HELP_TEXT_LAST_LINES_03"},
}};

/// A character as a capital, as the file system compares names
constexpr char16_t Upper(char16_t c) noexcept
{
	constexpr char16_t k_CaseStep = 0x20;
	// The Latin small letters of Basic Latin
	if (c >= u'a' && c <= u'z')
	{
		return static_cast<char16_t>(c - k_CaseStep);
	}
	// The micro sign has the Greek capital mu as its capital
	if (c == 0x00B5)
	{
		return 0x039C;
	}
	// The small letters of Latin-1, but for the division sign. The sharp s has no capital of one character.
	if (c >= 0x00E0 && c <= 0x00FE && c != 0x00F7)
	{
		return static_cast<char16_t>(c - k_CaseStep);
	}
	// The y with diaeresis has its capital in Latin Extended-A
	if (c == 0x00FF)
	{
		return 0x0178;
	}
	return c;
}

bool SameIgnoringCase(std::u16string_view a, std::u16string_view b) noexcept
{
	return std::ranges::equal(a, b, [](char16_t x, char16_t y) { return Upper(x) == Upper(y); });
}
} // namespace

bool skirmish_box::IsLandFile(std::u16string_view fileName) noexcept
{
	return fileName.size() >= k_Extension.size() &&
	       SameIgnoringCase(fileName.substr(fileName.size() - k_Extension.size()), k_Extension);
}

std::u16string_view skirmish_box::Stem(std::u16string_view fileName) noexcept
{
	return fileName.size() >= k_Extension.size() ? fileName.substr(0, fileName.size() - k_Extension.size()) : fileName;
}

std::optional<std::string_view> skirmish_box::TitleTextOf(std::u16string_view stem) noexcept
{
	const auto found =
	    std::ranges::find_if(k_TitleTexts, [stem](const auto& entry) { return SameIgnoringCase(entry.first, stem); });
	if (found == k_TitleTexts.end())
	{
		return std::nullopt;
	}
	return found->second;
}

bool skirmish_box::FolderOrderLess(std::u16string_view a, std::u16string_view b) noexcept
{
	return std::ranges::lexicographical_compare(a, b, [](char16_t x, char16_t y) { return Upper(x) < Upper(y); });
}

std::vector<skirmish_box::Land> skirmish_box::ListLands(std::span<const std::u16string> fileNames)
{
	std::vector<Land> lands;
	for (const auto& fileName : fileNames)
	{
		if (!IsLandFile(fileName))
		{
			continue;
		}
		const auto stem = Stem(fileName);
		lands.push_back({.fileName = fileName, .stem = std::u16string(stem), .titleText = TitleTextOf(stem)});
	}
	std::ranges::stable_sort(lands, [](const Land& a, const Land& b) { return FolderOrderLess(a.fileName, b.fileName); });
	return lands;
}
