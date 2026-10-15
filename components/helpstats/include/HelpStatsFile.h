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

#include <array>
#include <optional>
#include <span>
#include <vector>

/// The help record of a player's profile, as the game keeps it in helpstats.dat in the profile's folder. It holds the
/// count of each event that the help system counts, and the file is the 49 counts one after the other, with no header.
namespace openblack::helpstats
{

/// The number of events counted
constexpr size_t k_EventCount = 49;
/// The latest times of an event that a count keeps
constexpr size_t k_TimesKept = 64;

/// The count of one event
struct EventRecord
{
	/// The game turns on which the event occurred
	uint32_t total;
	/// A smoothed share of the turns on which the event occurs
	float smoothedRate;
	/// The slot of the next time to keep, and how many slots have a time
	uint8_t timeHead;
	uint8_t timeCount;
	/// 1 when the event occurred in the turn that has not ended
	uint8_t triggeredThisTurn;
	uint8_t padding;
	/// The times of the help clock, in milliseconds, at which the event occurred last
	std::array<uint32_t, k_TimesKept> times;
};

static_assert(sizeof(EventRecord) == 268);

using HelpStats = std::array<EventRecord, k_EventCount>;

/// The size of the file
constexpr size_t k_FileSize = sizeof(EventRecord) * k_EventCount;

/// The record in the bytes of a file. Nothing when the bytes are not of the size of the file.
[[nodiscard]] std::optional<HelpStats> Read(std::span<const uint8_t> bytes);

/// The bytes of a file that holds the record
[[nodiscard]] std::vector<uint8_t> Write(const HelpStats& stats);

} // namespace openblack::helpstats
