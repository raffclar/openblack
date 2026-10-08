/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

// How many of a town's villagers worship: the player drags the town's totem statue (a locked select, sent as a
// network packet) to set the percentage; the town sends or calls back villagers to match it. See
// docs/bw1-notes/magic.md.

namespace openblack::worship::percentage
{
/// 0 without a worship site; else stored, also on the totem statue, and GetWorshipersNeeded(1, 1) villagers sent
/// (AdjustWorshipersWorshipping(need, 1, 0))
void SetWorshipPercentage(entt::entity town, float percentage);
[[nodiscard]] float GetWorshipPercentage(entt::entity town);

/// target = pct > 0 ? max(1, int(pop x pct + 0.5))
/// : 0; result = target - (worshipping + on the way) + the go-home requests; *out = result > 0 && current >= target
[[nodiscard]] int GetWorshipersNeeded(entt::entity town, bool countOnWay, bool countGoHome, bool* out);

/// Two passes (the second also takes the villagers flagged 0x200); n > 0: the available villagers, the highest
/// WorshipScore (the nearest) first, go (CheckWorshipActivity, with life above damageThresholdToGoHome unless
/// skipLifeCheck); n < 0: those at or on the way to the site, the lowest score (the farthest) first, are sent back
/// (state 163)
void AdjustWorshipersWorshipping(entt::entity town, int count, bool skipLifeCheck, bool requireReachable);

/// The town's villagers on the way to the site, and its count of villagers at the site
void AddVillagerOnWay(entt::entity town, entt::entity villager);
void RemoveVillagerOnWay(entt::entity town, entt::entity villager);
void AddWorshipper(entt::entity town);
void RemoveWorshipper(entt::entity town);

/// The order AdjustWorshipersWorshipping takes the villagers in: gutils::GetDistanceModifier (the villager's distance
/// to the site's centre (the site's local point (12.55, 0, -26.1)), the centre's distance to the town + 100) x life^3.
/// The modifier falls off with the distance, so the nearest villagers score highest and go first.
[[nodiscard]] float WorshipScore(entt::entity villager);

/// Per frame, the logic part of the statue's draw: the rise advanced by the engine clock, the plinth at 8 x the
/// smoothed percentage and the icon standing on it
void UpdateTotems();
/// From the town centre's process: the rise advanced by the engine clock; arrived (within 0.005) while rising: the
/// rising loop goes and the stop sound plays. `statue` is the plinth's entity
void ProcessTotem(entt::entity statue);

/// The totem drag (pct = clamp(pct + dy x 0.1, 0, 1)), for the hand: the town of a totem statue it may drag (same
/// player, the statue, the player's citadel heart and the town's worship site built), or entt::null.
/// TODO(worship): only the site test is done here; the same-player, statue-built and heart-built tests of the original
/// are missing (no caller does them yet)
[[nodiscard]] entt::entity TotemTown(entt::entity statue);
} // namespace openblack::worship::percentage
