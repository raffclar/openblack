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

#include <span>
#include <string_view>
#include <vector>

#include <HelpStatsFile.h>

#include "Help/HelpProfile.h"

/// The help record as a player's profile stores it in helpstats.dat
namespace openblack::help::profile
{

/// Every event's count as the profile stores it. The clock is not stored: each game starts it again.
[[nodiscard]] helpstats::HelpStats ToHelpStats(const HelpProfile& profile);

/// Every event's count from the profile's record. The clock stays as it is.
void FromHelpStats(HelpProfile& profile, const helpstats::HelpStats& stats);

/// The name of the file in the folder of the player
constexpr std::string_view k_FileName = "helpstats.dat";

/// The bytes of the file that keeps every event's count
[[nodiscard]] std::vector<uint8_t> ToFileBytes(const HelpProfile& profile);
/// Every event's count from the bytes of the file. The game reads the bytes over the record that it has:
/// - A shorter file changes only the counts at its start. The other counts stay as they are.
/// - The game does not read the bytes after the length of the record.
void FromFileBytes(HelpProfile& profile, std::span<const uint8_t> bytes);

} // namespace openblack::help::profile
