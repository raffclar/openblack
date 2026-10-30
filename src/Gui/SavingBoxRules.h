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

#include <string_view>

#include <SaveSlots.h>

#include "DialogRect.h"

/// The box that the game shows while it saves or loads: "Saving..." or "Loading..." on a grey band across the middle
/// of the screen. The game draws the box as one frame of its own over the last picture and shows that frame. Then it
/// saves or loads while the frame stays on the screen. The screen also gets darker, but not for an automatic save.
namespace openblack::gui::saving_box
{

/// The help texts of the box
inline constexpr std::string_view k_SavingText = "HELP_TEXT_DIALOG_ADDITION_120";
inline constexpr std::string_view k_LoadingText = "HELP_TEXT_DIALOG_ADDITION_85";
/// The big text size of the game
inline constexpr int k_TextSize = 35;
/// The band goes this far up and down from the middle of the screen, and its soft shadows are this deep
inline constexpr int k_HalfBand = 30;
inline constexpr int k_Shadow = 15;
/// Each end of the band shades over this part of the width of the screen
inline constexpr float k_EndShare = 0.4f;
/// The colours, as 0xAARRGGBB
inline constexpr uint32_t k_DarkColour = 0x78000000;
inline constexpr uint32_t k_ShadowColour = 0x9F000000;
inline constexpr uint32_t k_BandDark = 0xFF202020;
inline constexpr uint32_t k_BandLight = 0xFF404040;
inline constexpr uint32_t k_LineColour = 0xF0F0F0F0;
inline constexpr uint32_t k_TextShadowColour = 0xFF000000;
inline constexpr uint32_t k_TextColour = 0xFFFFFFFF;
/// The text is centred across and up and down in this rectangle of the dialog space. Its shadow is a unit right and a
/// unit down.
inline constexpr DialogRect k_TextRect {.min = {100, 200}, .max = {700, 400}};

/// Whether the box makes the screen darker: not for the slots of the automatic saves
[[nodiscard]] constexpr bool DarkensScreen(uint32_t slot)
{
	return slot < gamesave::k_FirstAutosaveSlot || slot > gamesave::k_LastAutosaveSlot;
}

} // namespace openblack::gui::saving_box
