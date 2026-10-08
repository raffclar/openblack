/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleToggles.h"

#include <cctype>

#include <algorithm>
#include <string_view>
#include <utility>

#include "Audio/Device/Sound.h"

using namespace openblack;

namespace
{
/// The main room's buttons: the names of their submeshes (which the mesh has in other cases) and their tooltips, in
/// the order of the flags they set, as the game saves them
struct ButtonPlace
{
	std::string_view checked;
	std::string_view unchecked;
	uint32_t toolTip;
};
constexpr std::array<ButtonPlace, TempleToggles::k_Count> k_Buttons = {
    ButtonPlace {"LH_CheckBox_DisplayCitadel_Checked", "LH_CheckBox_DisplayCitadel_Unchecked", 0xE91 - 0xE73},
    ButtonPlace {"LH_CheckBox_DisplayCreature_Checked", "LH_CheckBox_DisplayCreature_Unchecked", 0xE8F - 0xE73},
    ButtonPlace {"LH_CheckBox_DisplayMagicActivity_Checked", "LH_CheckBox_DisplayMagicActivity_Unchecked", 0xE93 - 0xE73},
    ButtonPlace {"LH_CheckBox_DisplayInfluence_Checked", "LH_CheckBox_DisplayInfluence_Unchecked", 0xE92 - 0xE73},
    ButtonPlace {"LH_CheckBox_DisplayChallenges_Checked", "LH_CheckBox_DisplayChallenges_Unchecked", 0xE90 - 0xE73},
};

/// How each button clicks, in the same order: the pitch in percent of its click down and of its click up, and the
/// number its click up is keyed by. The creatures' button gives its click up 110 as that number instead of as its
/// pitch, so that click is at the usual pitch, on a channel of its own.
struct ButtonClick
{
	int downPitch;
	int upPitch;
	std::optional<uint32_t> upKey;
};
constexpr std::array<ButtonClick, TempleToggles::k_Count> k_Clicks = {
    ButtonClick {.downPitch = 100, .upPitch = 100, .upKey = std::nullopt},
    ButtonClick {.downPitch = 110, .upPitch = 100, .upKey = 110},
    ButtonClick {.downPitch = 95, .upPitch = 95, .upKey = std::nullopt},
    ButtonClick {.downPitch = 105, .upPitch = 105, .upKey = std::nullopt},
    ButtonClick {.downPitch = 108, .upPitch = 108, .upKey = std::nullopt},
};

bool SameName(std::string_view a, std::string_view b)
{
	return std::ranges::equal(a, b, [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}
} // namespace

TempleToggles::TempleToggles(PlaySound playSound)
    : _playSound(std::move(playSound))
{
}

void TempleToggles::Find(std::span<const std::string> subMeshNames)
{
	for (size_t button = 0; button < k_Count; ++button)
	{
		_buttons.at(button) = {};
		for (uint32_t i = 0; i < subMeshNames.size(); ++i)
		{
			if (SameName(subMeshNames[i], k_Buttons.at(button).checked))
			{
				_buttons.at(button).checked = i;
			}
			else if (SameName(subMeshNames[i], k_Buttons.at(button).unchecked))
			{
				_buttons.at(button).unchecked = i;
			}
		}
	}
	_held.reset();
}

std::optional<uint32_t> TempleToggles::Drawn(size_t button) const
{
	// The buttons' draw callbacks: the checked one is drawn while the flag is set, the unchecked one while it isn't
	return _shown.at(button) ? _buttons.at(button).checked : _buttons.at(button).unchecked;
}

bool TempleToggles::Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh)
{
	const bool pressing = pressed && !_wasPressed;
	_wasPressed = pressed;
	if (pressing && hoveredSubMesh.has_value())
	{
		for (size_t button = 0; button < k_Count; ++button)
		{
			if (Drawn(button) == hoveredSubMesh)
			{
				_held = button;
			}
		}
	}
	if (pressed || !_held.has_value())
	{
		return _held.has_value();
	}

	// A button fires its callback as it is let go over it: the checked one turns
	// its things off with a click down, the unchecked one on with a click up
	const auto button = *std::exchange(_held, std::nullopt);
	if (Drawn(button) == hoveredSubMesh)
	{
		_shown.at(button) = !_shown.at(button);
		if (_playSound)
		{
			_playSound(ClickOf(static_cast<Display>(button), _shown.at(button)));
		}
		// TODO(raffclar): turning influence on also resets the colours of the map's land, once the map is
		// drawn
	}
	return true;
}

std::vector<TempleToggles::Control> TempleToggles::GetControls() const
{
	std::vector<Control> controls;
	for (size_t button = 0; button < k_Count; ++button)
	{
		if (const auto drawn = Drawn(button); drawn.has_value())
		{
			controls.push_back({.subMesh = *drawn, .toolTip = k_Buttons.at(button).toolTip});
		}
	}
	return controls;
}

std::vector<uint32_t> TempleToggles::GetHidden() const
{
	std::vector<uint32_t> hidden;
	for (size_t button = 0; button < k_Count; ++button)
	{
		if (const auto& other = _shown.at(button) ? _buttons.at(button).unchecked : _buttons.at(button).checked;
		    other.has_value())
		{
			hidden.push_back(*other);
		}
	}
	return hidden;
}

bool TempleToggles::IsControl(uint32_t subMesh) const
{
	return std::ranges::any_of(GetControls(), [subMesh](const Control& control) { return control.subMesh == subMesh; });
}

temple_sounds::Options TempleToggles::ClickOf(Display display, bool shown)
{
	// Without a place, starting again on its channel if it still plays
	const auto& click = k_Clicks.at(static_cast<size_t>(display));
	temple_sounds::Options options {
	    .sound =
	        static_cast<entt::id_type>(shown ? audio::SoundId::G_CitadelButtonUp_01 : audio::SoundId::G_CitadelButtonDown_01),
	    .is3D = false,
	    .pitch = shown ? click.upPitch : click.downPitch,
	    .mode = 3,
	};
	if (shown && click.upKey.has_value())
	{
		options.owner = audio::Owner::Key(*click.upKey);
	}
	return options;
}
