/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveGameRoomRules.h"

#include <cctype>
#include <charconv>

#include <algorithm>
#include <array>

using namespace openblack::temple;

namespace
{
struct PartName
{
	std::string_view prefix;
	save_game_room::Part part;
	/// The number after the name of the first slot's control
	uint32_t first;
};
/// The controls by the start of their names, each followed by two digits: the pictures from 01, the buttons from 00
constexpr std::array k_PartNames {
    PartName {"LH_PICTURE_savegame_", save_game_room::Part::Picture, 1},
    PartName {"LH_BUTTON_SAVE", save_game_room::Part::Save, 0},
    PartName {"LH_BUTTON_LOAD", save_game_room::Part::Load, 0},
    PartName {"LH_BUTTON_DELETE", save_game_room::Part::Delete, 0},
};

bool StartsWithIgnoringCase(std::string_view text, std::string_view prefix)
{
	return text.size() >= prefix.size() && std::ranges::equal(text.substr(0, prefix.size()), prefix, [](char a, char b) {
		       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	       });
}

/// The empty picture's halves: the left for evil, the right for good
constexpr save_game_room::UvRect k_EvilHalf {.u0 = 0.0f, .v0 = 0.0f, .u1 = 0.5f, .v1 = 1.0f};
constexpr save_game_room::UvRect k_GoodHalf {.u0 = 0.5f, .v0 = 0.0f, .u1 = 1.0f, .v1 = 1.0f};
/// A tooltip's name keeps this many characters, and a longer name ends in dots from here
constexpr size_t k_ToolTipNameLength = 16;
constexpr size_t k_ToolTipNameDots = 13;
} // namespace

std::optional<save_game_room::Control> save_game_room::ParseControl(std::string_view subMeshName)
{
	for (const auto& [prefix, part, first] : k_PartNames)
	{
		if (!StartsWithIgnoringCase(subMeshName, prefix))
		{
			continue;
		}
		const auto digits = subMeshName.substr(prefix.size());
		if (digits.size() != 2 || !std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; }))
		{
			return std::nullopt;
		}
		uint32_t number = 0;
		std::from_chars(digits.data(), digits.data() + digits.size(), number);
		if (number < first || number - first >= k_Slots)
		{
			return std::nullopt;
		}
		return Control {.part = part, .slot = number - first};
	}
	return std::nullopt;
}

bool save_game_room::IsShown(Control control, const View& view, bool hasSave)
{
	const bool lookingAtPicture = view.closeUp && view.pictureLookedAt;
	switch (control.part)
	{
	case Part::Picture:
		return true;
	case Part::Save:
		return !view.multiplayer && control.slot <= k_LastPlayerSlot && !view.keptInRoom && lookingAtPicture;
	case Part::Load:
	case Part::Delete:
		return hasSave && lookingAtPicture;
	}
	return false;
}

save_game_room::Click save_game_room::Clicked(Control control, const View& view, bool hasSave)
{
	switch (control.part)
	{
	case Part::Picture:
		if (!view.closeUp || !view.pictureLookedAt)
		{
			return Click::LookCloser;
		}
		// Close up, a picture loads its slot's save. A picture of an empty slot saves into it, even while the player is
		// kept in the room
		if (hasSave)
		{
			return Click::AskLoad;
		}
		return control.slot <= k_LastPlayerSlot && !view.multiplayer ? Click::AskName : Click::Nothing;
	case Part::Save:
		return hasSave ? Click::AskOverwrite : Click::AskName;
	case Part::Load:
		return hasSave ? Click::AskLoad : Click::Nothing;
	case Part::Delete:
		return Click::AskDelete;
	}
	return Click::Nothing;
}

glm::vec3 save_game_room::HeldOffset(Part part, glm::vec3 frameZ)
{
	return part == Part::Picture ? glm::vec3(0.0f) : -k_ButtonPushIn * frameZ;
}

save_game_room::UvRect save_game_room::EmptyPicture(float alignment)
{
	return alignment >= 0.0f ? k_GoodHalf : k_EvilHalf;
}

std::u16string save_game_room::ToolTipName(std::u16string_view name)
{
	if (name.size() < k_ToolTipNameLength)
	{
		return std::u16string(name);
	}
	auto shown = std::u16string(name.substr(0, k_ToolTipNameDots));
	shown.append(k_ToolTipNameLength - k_ToolTipNameDots, u'.');
	return shown;
}

bool save_game_room::PictureZoomsIn(const PictureToolTipInput& input)
{
	return input.hovered && (input.zoom == 0.0f || (input.zoom == 1.0f && !input.pictureLookedAt));
}

std::optional<save_game_room::PictureToolTip> save_game_room::PictureToolTipOf(const PictureToolTipInput& input)
{
	if (PictureZoomsIn(input))
	{
		if (input.saveName.has_value() && !input.saveName->empty())
		{
			return PictureToolTip {.text = k_ZoomInNamedText, .name = ToolTipName(*input.saveName)};
		}
		return PictureToolTip {.text = k_ZoomInText, .name = {}};
	}
	const bool closeUp = input.zoom == 1.0f && input.highDetail == input.slot && !input.buttonHovered;
	if (input.hovered && closeUp)
	{
		if (input.saveName.has_value())
		{
			return PictureToolTip {.text = k_LoadText, .name = {}};
		}
		if (input.slot <= k_LastPlayerSlot && !input.multiplayer)
		{
			return PictureToolTip {.text = k_SaveText, .name = {}};
		}
		return std::nullopt;
	}
	if (!input.hovered && closeUp && input.pictureLookedAt)
	{
		return PictureToolTip {.text = k_ZoomOutText, .name = {}};
	}
	return std::nullopt;
}
