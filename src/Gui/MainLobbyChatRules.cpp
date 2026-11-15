/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MainLobbyChatRules.h"

#include <fmt/format.h>

#include "TextDatabase.h"

namespace openblack::gui::main_chat
{

namespace
{

/// A public line of the game room is white
constexpr uint32_t k_OtherColourBits = 0xFFFFFFFF;

} // namespace

glm::vec4 ColourOf(uint32_t argb) noexcept
{
	return {static_cast<float>((argb >> 16u) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8u) & 0xFFu) / 255.0f,
	        static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>((argb >> 24u) & 0xFFu) / 255.0f};
}

multiplayer::ChatItem LineItem(const online::main_room::Line& line, int64_t ownAccount)
{
	const auto text = multiplayer::ClampChat(ToUtf16(line.text));
	if (line.system)
	{
		return {.text = text, .colour = ColourOf(k_SystemColourBits)};
	}
	const auto name = ToUtf16(line.name);
	if (line.account == ownAccount)
	{
		return multiplayer::OwnChatLine(name, text);
	}
	return {.text = name + u": " + text, .colour = ColourOf(line.staff ? k_StaffColourBits : k_OtherColourBits)};
}

std::u16string PeopleHeading(size_t count)
{
	return ToUtf16(fmt::format("In the room ({})", count));
}

std::string RefusalLine(const ecs::systems::LobbyError& error)
{
	if (error.code == "muted")
	{
		return "An admin muted your account. The line was not sent.";
	}
	if (error.code == "throttled")
	{
		const auto wait = error.retryAfterSeconds.value_or(1);
		return fmt::format("Too many lines. Wait {} {} and send the line again.", wait, wait == 1 ? "second" : "seconds");
	}
	if (error.code == "too_long")
	{
		return "A line has at most 254 characters.";
	}
	return "The lobby did not get the line. Send it again.";
}

} // namespace openblack::gui::main_chat
