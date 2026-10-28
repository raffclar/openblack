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

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "DialogPainter.h"
#include "Enums.h"

/// The layout, the pictures and the rules of the chat controls at the top right of the screen. The layout is in the front
/// end's units, with its left edge on the screen's left edge and its top edge on the screen's top edge. W and H are the
/// width and the height of the screen in these units.
///
/// - The speech button is a 20 unit square in the top right corner. It opens the chat.
/// - The open chat has the send button in the same corner, the line of the chat to its left and the quick chat button
///   to the left of the line.
/// - The list of players is below the send button, 130 units wide. The quick chat list replaces it while it is open.
namespace openblack::gui::chat_controls
{

/// The size of the speech, send and quick chat buttons
constexpr int k_ButtonSize = 20;
/// The height of the line of the chat
constexpr int k_EditHeight = 20;
/// The size of the text of the line and of the lists
constexpr int k_TextSize = 18;
/// The width of the list of players
constexpr int k_PlayerListWidth = 130;
/// The quick chat list is at most this wide, and leaves at least this gap at the left of the screen
constexpr int k_QuickListMostWidth = 520;
constexpr int k_QuickListGap = 50;
/// The scroll bars of the lists are 2 units narrower than a button
constexpr int k_ScrollBarWidth = k_ButtonSize - 2;
/// The controls are drawn at this opacity
constexpr float k_Alpha = 0.9f;
/// A button that is not pushed in is drawn 2 units up and to the left
constexpr int k_Lift = 2;
/// The list of players is at least 10 units high, and has 8 units below its items
constexpr int k_LeastListHeight = 10;
constexpr int k_ListMargin = 8;

[[nodiscard]] constexpr DialogRect ButtonAt(glm::ivec2 corner) noexcept
{
	return {.min = corner, .max = corner + k_ButtonSize};
}
/// The speech button and the send button: the top right corner
[[nodiscard]] constexpr DialogRect CornerButtonRect(int width) noexcept
{
	return ButtonAt({width - k_ButtonSize, 0});
}
/// The line of the chat: its right edge is 20 units from the right of the screen, and it is as wide as EditWidth gives
[[nodiscard]] constexpr DialogRect EditRect(int width, int editWidth) noexcept
{
	const auto right = width - k_ButtonSize;
	return {.min = {right - editWidth, 0}, .max = {right, k_EditHeight}};
}
/// The quick chat button: at the left of the line of the chat
[[nodiscard]] constexpr DialogRect QuickButtonRect(const DialogRect& edit) noexcept
{
	return ButtonAt({edit.min.x - k_ButtonSize, 0});
}
/// The list of players: below the send button, as high as its items and 8 more, from 10 units to half of the screen
[[nodiscard]] DialogRect PlayerListRect(int width, int height, int contentHeight) noexcept;
/// The quick chat list: below the line, against the right of the screen, half of the screen high
[[nodiscard]] DialogRect QuickListRect(int width, int height) noexcept;
/// The speech button of the heading of the list of players: 22 units from the right of the box of the items
[[nodiscard]] constexpr int HeadingButtonLeft(int boxRight) noexcept
{
	return boxRight - k_ButtonSize - 2;
}

/// Where the layout's corner goes on the window: its left edge on the window's left edge, its top on the window's top.
/// The layout keeps the scale of the front end's layout (FrontEndSpace).
[[nodiscard]] glm::vec2 Origin(glm::u16vec2 resolution) noexcept;
/// The width and the height of the window in the layout's units
[[nodiscard]] glm::ivec2 LayoutSize(glm::u16vec2 resolution) noexcept;

/// The pictures of the buttons, from the front end's buttons (data/textures/Front_end_buttons.raw)
enum class Look : uint8_t
{
	Speech,
	NoSpeech,
	ExclaimArrow,
	/// The arrow of the setup boxes, to the right (DialogPainter::DrawArrow)
	RightArrow,
};
struct Picture
{
	glm::vec2 uvMin {0.0f};
	glm::vec2 uvMax {0.0f};
	bool operator==(const Picture&) const = default;
};
/// The picture of a look. It lights up under the pointer or with the focus. The right arrow has no picture here.
[[nodiscard]] Picture PictureOf(Look look, bool lit) noexcept;

/// The send button shows that the line is empty, or that it can go
[[nodiscard]] constexpr Look SendLook(bool lineEmpty) noexcept
{
	return lineEmpty ? Look::NoSpeech : Look::RightArrow;
}
/// With Shift and no other modifier key, the speech button opens the quick chat at once
[[nodiscard]] constexpr Look OpenLook(bool onlyShift) noexcept
{
	return onlyShift ? Look::ExclaimArrow : Look::Speech;
}

/// What decides each frame whether the chat controls show
struct ShowFacts
{
	/// A game of one player, or a network game that is connected
	bool connected {false};
	bool singlePlayer {false};
	/// The world shows, with no video
	bool inGame {false};
	bool open {false};
	/// A message is in the incoming messages
	bool incoming {false};
	/// The hand holds the player's creature to interact with it
	bool creatureInteracting {false};
	/// The custom music player plays, with no video and no cinema bars
	bool music {false};
};
/// Whether the controls show after this frame. The answer stays from frame to frame until a fact changes it:
/// 1. With no connected game, or in a game of one player with the chat shut, the controls go.
/// 2. Out of the world, nothing more changes.
/// 3. An open chat or an incoming message shows the controls.
/// 4. The creature in the hand hides them.
/// 5. The custom music shows them, unless the creature is in the hand.
/// Thus after the creature leaves the hand, the speech button stays away until a message comes in (vanilla bug).
[[nodiscard]] bool ShowsNext(bool shownBefore, const ShowFacts& facts) noexcept;

/// The colour of a player in the list of players, as 0xAARRGGBB: the colour of the god, half of the way to white, with
/// no alpha. A selected player has full alpha, which gives the row its bar.
[[nodiscard]] uint32_t PlayerColour(uint32_t godColour) noexcept;
/// The colour of the heading of the list of players: white with no alpha
constexpr uint32_t k_HeadingColour = 0x00FFFFFFu;
/// The bits that a selected player adds to its colour
constexpr uint32_t k_SelectedBits = 0xFF000000u;

/// A colour of 0xAARRGGBB with full alpha, for the text and the bars of the lists
[[nodiscard]] glm::vec4 ToColour(uint32_t bits) noexcept;

} // namespace openblack::gui::chat_controls
