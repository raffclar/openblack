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

#include <fmt/format.h>

/// The words that the game server and the lockstep host write in their log for a player. The log of a server holds no
/// account name, no profile name, no IP address and no typed text of a player. It names a player by the god of the
/// player or by the number of an observer, so that an operator can still follow one player through the log.
namespace openblack::server::log_names
{

/// The text for a player: "the player of god N" for a god, "observer N" for an observer with a number, "an observer"
/// for an observer with no number yet, and "a player" for a connection that did not join.
[[nodiscard]] inline std::string PlayerOf(std::optional<uint8_t> seat, uint8_t observer, bool joined)
{
	if (seat.has_value())
	{
		return fmt::format("the player of god {}", *seat);
	}
	if (!joined)
	{
		return "a player";
	}
	return observer == 0 ? std::string("an observer") : fmt::format("observer {}", observer);
}

/// The same text with a capital first letter, for the start of a sentence
[[nodiscard]] inline std::string Capitalised(std::string text)
{
	if (!text.empty() && text.front() >= 'a' && text.front() <= 'z')
	{
		text.front() = static_cast<char>(text.front() - 'a' + 'A');
	}
	return text;
}

} // namespace openblack::server::log_names
