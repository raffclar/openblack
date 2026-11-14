/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <string>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Gui/MultiplayerFrontEndRules.h"
#include "Online/MainLobbyRoom.h"

/// The rules of the panel of the main lobby room (deviation 267), at the right of the list of the sessions. The panel
/// uses the list, the edit box and the colours of the chat of the game room. These rules have no state.
namespace openblack::gui::main_chat
{

/// The colour of a line of the staff, as 0xAARRGGBB: the yellow of the staff in the gathering box
inline constexpr uint32_t k_StaffColourBits = 0xFFFFFF00;
/// The colour of a line of the game itself, as 0xAARRGGBB: the light grey of the system lines of the game room
inline constexpr uint32_t k_SystemColourBits = 0xFFC0C0C0;

/// The colour of a line as a vector, from 0xAARRGGBB
[[nodiscard]] glm::vec4 ColourOf(uint32_t argb) noexcept;

/// A line of the room as the panel shows it:
/// - The player's own line: "name> text" in light grey, as the game room shows it.
/// - A line of the staff: "name: text" in the yellow of the staff.
/// - A line of another person: "name: text" in white, as a public line of the game room.
/// - A line of the game itself: only the text, in light grey.
/// The text is at most 254 characters, as the edit box of the chat takes them.
[[nodiscard]] multiplayer::ChatItem LineItem(const online::main_room::Line& line, int64_t ownAccount);

/// The heading of the list of the people: "In the room (3)"
[[nodiscard]] std::u16string PeopleHeading(size_t count);

/// The line that the panel adds when the lobby refuses a line of the player
[[nodiscard]] std::string RefusalLine(const ecs::systems::LobbyError& error);

} // namespace openblack::gui::main_chat
