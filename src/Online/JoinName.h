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

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

/// The profile name that a join on the local network sends from the command line (the developer option --join-name).
/// The front end sends the name of the profile of the player. The command line has no profile to read, so it gives the name.
namespace openblack::online::join_name
{

/// The most bytes of a name that a game server on the local network accepts
constexpr size_t k_MostBytes = 64;

/// The name when a game server on the local network accepts it: not empty, at most k_MostBytes bytes, and no control
/// character. None for other text.
[[nodiscard]] inline std::optional<std::string> Check(std::string_view name)
{
	const bool control = std::ranges::any_of(name, [](char letter) {
		const auto byte = static_cast<uint8_t>(letter);
		return byte < 0x20 || byte == 0x7F;
	});
	if (name.empty() || name.size() > k_MostBytes || control)
	{
		return std::nullopt;
	}
	return std::string(name);
}

/// The name of the option, none when the option is not given. Text that the server does not accept gives none too, and
/// `valid` is false then.
struct Parsed
{
	std::optional<std::string> name;
	bool valid = true;
};

/// The name of an option that may be absent. An absent option is valid and gives no name.
[[nodiscard]] inline Parsed Parse(const std::optional<std::string>& option)
{
	if (!option.has_value())
	{
		return {};
	}
	auto name = Check(*option);
	return {.name = name, .valid = name.has_value()};
}

} // namespace openblack::online::join_name
