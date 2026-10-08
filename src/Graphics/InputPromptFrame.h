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

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

// The input prompt icons of one frame as the draw sees them (OverlayFrame::inputPrompts, the frame snapshot):
// help::input_prompt's list copied by FillOverlayFrame after its two updates, so Renderer::DrawInputPrompts reads no game
// state. Values only.
namespace openblack::graphics
{

/// One InputPromptIcon with its alpha after each of the frame's two updates
struct InputPromptDraw
{
	int32_t animType {0};
	int32_t clickType {0};
	int32_t row {3};        ///< the same field as keyName, as an int
	std::u16string keyName; ///< the same field as row, as a string (animType -1, clickType 0)
	std::u16string text;
	int32_t x {0};
	int32_t y {0};
	int32_t size {0};
	uint32_t align {0};
	uint32_t c1 {0}; ///< ARGB: its rgb (the alpha byte is rewritten per pass)
	uint32_t c2 {0}; ///< ARGB: its rgb
	int32_t alpha8 {0};
	/// a = clamp(current, 0, 1) after the first update (the help system's draw) and the second (the help text)
	std::array<float, 2> alpha {0.0f, 0.0f};
};

/// The frame's input prompt icons, newest first, and what the key or mouse draw reads besides them
struct InputPromptFrame
{
	std::vector<InputPromptDraw> icons;
	/// The hand's position projected to the screen, in pixels from the top left, truncated (inferred: the projection
	/// returns ints); none when it is not projected
	std::optional<glm::vec2> hand;
	/// game_clock::TickCount(): the system tick count, for the mouse button's blink
	uint32_t tickCount {0};
};

} // namespace openblack::graphics
