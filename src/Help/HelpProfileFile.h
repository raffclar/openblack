/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <HelpStatsFile.h>

#include "Help/HelpProfile.h"

/// The help record as a player's profile stores it in helpstats.dat
namespace openblack::help::profile
{

/// Every event's count as the profile stores it. The clock is not stored: each game starts it again.
[[nodiscard]] helpstats::HelpStats ToHelpStats(const HelpProfile& profile);

/// Every event's count from the profile's record. The clock stays as it is.
void FromHelpStats(HelpProfile& profile, const helpstats::HelpStats& stats);

} // namespace openblack::help::profile
