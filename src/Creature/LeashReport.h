/*******************************************************************************
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

#include "Enums.h"

/// The log line that each computer writes when a turn does a leash command of a player. The server and every client
/// write the same line for the same command, so a network check can compare the leash of each creature on all of them.
namespace openblack::creature_leash
{

/// The leash that a creature wears after the command
struct WornReport
{
	LeashType type {LeashType::Rope};
	/// The god slot of the player that holds the leash
	uint32_t holder {0};
	/// The thing that the leash is tied to, or none when the hand holds the leash
	std::optional<uint32_t> tiedTo;

	bool operator==(const WornReport&) const = default;
};

/// What a leash command did, as the turn saw it after the command
struct CommandReport
{
	/// The god slot of the player that sent the command
	uint32_t player {0};
	/// The words that tell the command, such as "presses the leash key"
	std::string_view command;
	uint32_t turn {0};
	/// The primary creature of the player, or none when the player has no creature
	std::optional<uint32_t> creature;
	/// The leash of the creature, or none when the creature wears no leash
	std::optional<WornReport> worn;
};

/// The log line of a leash command. It starts with "Leash command: player N". The text after the turn tells the leash
/// of the creature after the command.
[[nodiscard]] std::string ReportLine(const CommandReport& report);

} // namespace openblack::creature_leash
