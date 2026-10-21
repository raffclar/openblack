/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleControlNames.h"

#include <cctype>

#include <algorithm>
#include <string_view>

#include <fmt/format.h>

using namespace openblack;

namespace
{
/// The challenge room and the save game room have 21 places, each with its own controls
constexpr uint32_t k_Places = 21;

bool SameName(std::string_view a, std::string_view b)
{
	return std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}
} // namespace

std::vector<std::string> temple::controls::Names(TempleRoom room)
{
	std::vector<std::string> names;
	switch (room)
	{
	case TempleRoom::Challenge:
		// Each place's picture, its frames and its replay button, counted from 1
		for (const std::string_view kind : {"LH_Picture_Challenge_", "LH_FRAME_CHALLENGE_", "LH_FRAME_CHALLENGE_GOLD_",
		                                    "LH_FRAME_CHALLENGE_SILVER_", "LH_Button_Challenge_"})
		{
			for (uint32_t place = 1; place <= k_Places; ++place)
			{
				names.push_back(fmt::format("{}{:02}", kind, place));
			}
		}
		names.emplace_back("LH_Scroll_Challenge");
		break;
	case TempleRoom::SaveGame:
		// Each place's picture counted from 1, and its buttons counted from 0
		for (uint32_t place = 1; place <= k_Places; ++place)
		{
			names.push_back(fmt::format("LH_PICTURE_SAVEGAME_{:02}", place));
		}
		for (const std::string_view kind : {"LH_BUTTON_SAVE", "LH_BUTTON_LOAD", "LH_BUTTON_DELETE"})
		{
			for (uint32_t place = 0; place < k_Places; ++place)
			{
				names.push_back(fmt::format("{}{:02}", kind, place));
			}
		}
		names.emplace_back("LH_Scroll_Savegame");
		break;
	case TempleRoom::CreatureCave:
		names = {"LH_SCROLL_CREATURE", "LH_SCROLL_FIGHT", "LH_SCROLL_LIKES", "LH_SCROLL_MAGIC"};
		break;
	case TempleRoom::Credits:
		for (uint32_t scroll = 1; scroll <= 7; ++scroll)
		{
			names.push_back(fmt::format("LH_SCROLL_LIB{:02}", scroll));
		}
		break;
	case TempleRoom::Main:
		for (const std::string_view shown : {"Citadel", "Creature", "MagicActivity", "Influence", "Challenges"})
		{
			names.push_back(fmt::format("LH_CheckBox_Display{}_Checked", shown));
			names.push_back(fmt::format("LH_CheckBox_Display{}_Unchecked", shown));
		}
		names.emplace_back("LH_Scroll_World");
		break;
	default:
		break;
	}
	return names;
}

std::vector<uint32_t> temple::controls::SubMeshes(TempleRoom room, std::span<const std::string> subMeshNames)
{
	// Each control is the first submesh of its name. The game gives a control whose name it cannot find the first
	// submesh.
	std::vector<uint32_t> subMeshes;
	for (const auto& name : Names(room))
	{
		const auto found =
		    std::ranges::find_if(subMeshNames, [&name](const std::string& other) { return SameName(name, other); });
		const auto subMesh =
		    found != subMeshNames.end() ? static_cast<uint32_t>(std::distance(subMeshNames.begin(), found)) : 0u;
		if (std::ranges::find(subMeshes, subMesh) == subMeshes.end())
		{
			subMeshes.push_back(subMesh);
		}
	}
	std::ranges::sort(subMeshes);
	return subMeshes;
}
