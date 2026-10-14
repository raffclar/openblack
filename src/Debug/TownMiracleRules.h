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

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "Enums.h"

/// The rules of a debug tool that gives a town a miracle or takes it away. The tool uses the steps that the land's
/// scripts use. These rules choose the magic types to give or take, and tell why a town has no worship site.
namespace openblack::town_miracle_tool
{

/// The magic types to give a town, in the order to give them. A land file line gives the miracle and then its plain
/// miracle when the town does not hold it. A script command gives only the miracle.
[[nodiscard]] std::vector<MagicType> TypesToGive(MagicType type, MagicType plain, bool withPlain);

/// The magic types to take from a town, in the order to take them. A script command takes only the miracle. A creature
/// that steals a miracle takes its plain miracle first, then each power-up level that the town holds.
/// levels holds the magic type of each power-up level of the miracle's seed, None where the seed has no such level.
[[nodiscard]] std::vector<MagicType> TypesToTake(MagicType type, MagicType plain, const std::array<MagicType, 3>& levels,
                                                 bool allLevels);

/// Why a town has no worship site after the tool let it have one
enum class NoSite : uint8_t
{
	/// The town belongs to no player
	Neutral,
	/// The town's player has no temple
	NoTemple,
	/// The town's player's temple is not built yet
	TempleNotBuilt,
	/// The land is the first land, which has no worship sites
	LandWithoutSites,
	/// The town has no people
	NoPeople,
	/// The temple has no free place for one more site
	NoPlace,
};

/// What the game knows about a town that wants a worship site
struct SiteChecks
{
	bool hasSite {false};
	bool neutral {false};
	bool hasTemple {false};
	bool templeBuilt {false};
	bool landWithoutSites {false};
	/// The tool lets the land without worship sites have one
	bool anyLand {false};
	uint32_t population {0};
};

/// Why the town has no worship site, the first reason in the order the game checks; none when it has one
[[nodiscard]] std::optional<NoSite> WhyNoSite(const SiteChecks& checks);

/// A short text for a reason, for the tool's window and answers
[[nodiscard]] std::string_view Describe(NoSite reason);

} // namespace openblack::town_miracle_tool
