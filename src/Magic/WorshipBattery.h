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

#include "ECS/Components/WorshipChants.h"

namespace openblack
{
struct GWorshipSiteInfo;
} // namespace openblack

// The prayer power of a worship site. Each turn the villagers dancing at the site chant prayer power into it. Miracles
// and the site's spell icons draw on what the dancers chant that turn and on a battery that stores what is left over.
// The dance speeds up when more is drawn and while the battery is low. Pure functions of the site's battery, its
// rules and its number of dancers.

namespace openblack::magic
{
/// The prayer power a site with infinite prayer power reports
inline constexpr float k_InfiniteChants = 1.0e6f;

/// The rules of a worship site, from its tribe's worship site record
struct WorshipBatteryRules
{
	float chantsPerVillager {0.0f};            ///< what each dancer chants each turn
	float chantsToFillBattery {0.0f};          ///< the battery's size with no dancers
	float eachVillagerAddToFillBattery {0.0f}; ///< what each dancer adds to the battery's size
	/// What the icons leave for the miracles already cast while any of their seeds is out. The file holds an integer
	/// that the game reads as a float, so in practice it is next to nothing; kept as the game reads it.
	float chantsToReserveForMaintaining {0.0f};
	/// The player's tribal power the sites chant with. Every site uses the player's Aztec multiplier, whatever its
	/// tribe.
	float tribalPower {1.0f};
};

[[nodiscard]] WorshipBatteryRules WorshipBatteryRulesFor(const GWorshipSiteInfo& info, float tribalPower);

/// The prayer power state of one worship site
using WorshipBattery = ecs::components::WorshipChants;

/// A spell icon of a worship site, as the site's turn sees it
struct WorshipIconCharge
{
	bool charging {false}; ///< it is charging a seed for the hand
	bool seedOut {false};  ///< its seed is out of it, in the hand or cast
	float required {0.0f}; ///< what its seed needs to be charged full
	float store {0.0f};    ///< what it has been charged so far
};

/// What the dancers chant each turn at full intensity
[[nodiscard]] float WorshipCapacity(const WorshipBatteryRules& rules, uint32_t dancers);

/// How much the battery can store
[[nodiscard]] float WorshipMaxBattery(const WorshipBatteryRules& rules, uint32_t dancers);

/// What is left to draw this turn
[[nodiscard]] float WorshipAvailable(const WorshipBattery& site);

/// What the site can give its spell icons this turn: what is left, less the reserve for miracles while any icon's seed
/// is out, never below 0
[[nodiscard]] float WorshipAvailableForIcons(const WorshipBattery& site, const WorshipBatteryRules& rules, bool seedsOut);

/// Draws prayer power from the site, at most what is left this turn. Returns what was drawn (0 for a negative amount);
/// the caller adds it to the player's prayer power statistics.
float UseWorshipChants(WorshipBattery& site, float amount);

/// Draws prayer power unless the site is infinite; an infinite site gives the whole amount without booking it
float UseWorshipChantsIfNotInfinite(WorshipBattery& site, float amount);

/// A miracle cast from the site asks for upkeep: free with the free maintenance cheat
float MaintainSpellFromWorship(WorshipBattery& site, float amount);

/// The start of the site's turn: records how far the demand of the last turn outstripped the dancers
void UpdateWorshipStrain(WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers);

/// What each charging spell icon gets this turn when the dancers keep up with demand: the smaller of what the icons
/// need and what the site can give them, shared evenly. 0 while the site is strained or no icon needs any.
[[nodiscard]] float WorshipIconShare(const WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t chargingIcons,
                                     float neededByIcons, bool seedsOut);

/// The end of the site's turn: sets the dance intensity from what was drawn and how full the battery is, adds what the
/// dancers chanted at that intensity to the battery less what was drawn, and opens the next turn
void EndWorshipTurn(WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers);

/// A share of prayer power goes into an icon's store, filling it no higher than it needs. Returns what the site is
/// charged for it: the whole share, or when the share overfills the icon only the part that overflows, as the game
/// reckons it.
float AddToIconStore(WorshipIconCharge& icon, float share);

/// The site's whole turn, before any miracle's upkeep is drawn: how strained it was, its charging icons each given an
/// even share when the dancers keep up, then the end of the turn. Returns the prayer power drawn, for the player's
/// statistics.
float ProcessWorshipTurn(WorshipBattery& site, const WorshipBatteryRules& rules, uint32_t dancers,
                         std::span<WorshipIconCharge> icons);

/// What the hand acting outside its player's influence may take from the site this turn: only the dancers' chanting not
/// yet drawn, less the reserve for miracles, never below 0, split among the player's interfaces (hands)
[[nodiscard]] float WorshipAvailableForVirtualInfluence(const WorshipBattery& site, const WorshipBatteryRules& rules,
                                                        uint32_t dancers, uint32_t interfaces);
} // namespace openblack::magic
