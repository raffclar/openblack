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

#include <optional>
#include <string>
#include <string_view>

#include <glm/vec2.hpp>

/// The ping that a network game can show to the player (deviation 143). The game shows no ping, so this display is
/// off until the player turns it on with --show-ping. It shows only the round trip of this computer's own game socket.
/// It is presentation only: no value of it goes into the synced world. Pure functions with no global state.
namespace openblack::gui::ping_display
{

/// The words of the ping. The game makes this text from its own data, but it never shows it.
inline constexpr std::u16string_view k_Format = u"ping %d";

/// The round trip in whole milliseconds, with the part below a millisecond cut off. None before the first answer.
/// The number stops at the largest value that %d can show.
[[nodiscard]] std::optional<int32_t> Milliseconds(std::optional<uint64_t> roundTripMicroseconds) noexcept;

/// The text of the ping, such as "ping 48". None before the first answer of the server.
[[nodiscard]] std::optional<std::u16string> Text(std::optional<uint64_t> roundTripMicroseconds);

/// The line of the player's own name in the room's player list, with the ping after it and one space between. A line
/// with no ping stays as it is.
[[nodiscard]] std::u16string AfterOwnName(std::u16string line, const std::optional<std::u16string>& ping);

/// The ping line on the game screen. The game has no ping line, so the place and the look come from the game's other
/// small texts. The line is in the top left corner, because the game uses the other corners in play. Two panels of the
/// game can come into the top left corner: the fight panel, and the creature's status panel while the camera follows a
/// creature. The line does not show while one of these panels shows.
///
/// The layout is in the front end's units, with its left edge on the left edge of the screen and its top edge on the
/// top edge of the screen. The line is 20 units high at the top of the screen, as the line of the chat is. The text
/// starts 5 units from the left edge, as far as the incoming message line stops from the right edge.
///
/// The look is the look of the incoming message line: text size 18, white, with a black shadow 2 units down and to
/// the right, at an opacity of 229 in 255.
inline constexpr int k_LineLeft = 5;
inline constexpr int k_LineHeight = 20;
inline constexpr int k_TextSize = 18;
inline constexpr int k_ShadowOffset = 2;
inline constexpr float k_Alpha = 229.0f / 255.0f;

/// The top left corner of the text in the layout: the text is in the middle of the line, from top to bottom
[[nodiscard]] constexpr glm::ivec2 TextCorner() noexcept
{
	return {k_LineLeft, (k_LineHeight / 2) - (k_TextSize / 2)};
}

/// Where the corner of the layout goes on the window, so that the layout's top left is the window's top left. The
/// layout keeps the scale of the front end's layout (FrontEndSpace).
[[nodiscard]] glm::vec2 Origin(glm::u16vec2 resolution) noexcept;

/// What decides in each frame if the ping line shows
struct LineFacts
{
	/// The text of the ping. None when --show-ping is off, out of a network game, or before the first answer.
	bool hasText {false};
	/// The menu, the multiplayer front end or another box of the game is open
	bool menuOpen {false};
	bool inTemple {false};
	/// A video plays
	bool videoPlays {false};
	/// The cinema bars are in
	bool cinemaBars {false};
	/// The fight panel shows in the top left corner
	bool fightPanel {false};
	/// The creature's status panel shows near the top of the screen, as while the camera follows a creature
	bool creaturePanelNearTop {false};
};

/// The line shows with a text, only while the game shows the world with no box, video or cinema bars, as the incoming
/// message line does. The line also does not show while a panel of the game shows in the top left corner.
[[nodiscard]] constexpr bool LineShows(const LineFacts& facts) noexcept
{
	return facts.hasText && !facts.menuOpen && !facts.inTemple && !facts.videoPlays && !facts.cinemaBars && !facts.fightPanel &&
	       !facts.creaturePanelNearTop;
}

} // namespace openblack::gui::ping_display
