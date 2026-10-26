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

} // namespace openblack::gui::ping_display
