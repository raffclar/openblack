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

#include <functional>
#include <optional>
#include <span>

#include "Enums.h"

/// The rules of the disciples' own work once put down: the trader carrying food or wood between its town and another,
/// the craftsman keeping its town's workshops in wood, the missionary preaching in another town, and the waiting of a
/// disciple with nothing to do. Free of the game's state so they can be tested on their own.
namespace openblack::disciple_work
{

/// A trader carries what the town it trades with wants more, food before wood when wanted as much; nothing when that
/// town wants neither
[[nodiscard]] std::optional<ResourceType> TradeResource(float foodWanted, float woodWanted);

/// Which way a trader goes next: to fetch from its own town's store or to deliver to the other town's
enum class TradeLeg : uint8_t
{
	Fetch,
	Deliver,
};
/// The fuller its load, the more it is drawn to deliver, the emptier, to fetch, each weighed by how near the store is
/// (the game's distance falloff out to five kilometres); fetching wins only when it is drawn more
[[nodiscard]] TradeLeg ChooseTradeLeg(uint32_t held, uint32_t carryLimit, float metresToOwnStore, float metresToTradeStore);

/// How much a trader fetches: what it can still carry of its limit, none when its load is already that much
[[nodiscard]] uint32_t TradeFetchAmount(uint32_t carryLimit, uint32_t held);

/// What a trader asks its own store for: what it already carries, or the wanted resource when it carries nothing
[[nodiscard]] ResourceType TradeFetchResource(ResourceType carried, ResourceType wanted);

/// A town's wants having changed (a store going from empty to stocked, a site added, a death): the mark stays up for
/// the rest of this town turn and all of the next, then comes down. `lastTurn` says it was already up a turn ago.
void MarkDesiresChanged(bool& changed, bool& lastTurn);
/// The end of a town's turn: a mark that has lasted a whole turn comes down
void EndTownTurn(bool& changed, bool& lastTurn);

/// A waiting disciple's turns before it looks for work again, after this turn: when its town's wants have just changed
/// it looks again soon (random below ten turns, `random(limit)` giving below limit), else one turn nearer
[[nodiscard]] int32_t IdleTurnsLeft(int32_t turnsLeft, bool townWantsChanged, const std::function<uint32_t(uint32_t)>& random);

/// A workshop holds at most this many scaffolds, one fewer while it makes one
inline constexpr uint32_t k_WorkshopScaffoldPlaces = 3;
/// The scaffolds a workshop still has room for
[[nodiscard]] uint32_t WorkshopRoom(bool makingScaffold, uint32_t scaffoldsHeld);
/// How far a workshop is from having wood for another scaffold: 1 with none, 0 with enough
[[nodiscard]] float WorkshopWoodShortfall(uint32_t wood, uint32_t scaffoldWood);
/// Whether a workshop wants wood: always, unless it has no room for another scaffold and already holds wood enough for one
[[nodiscard]] float WorkshopWantsWood(uint32_t room, uint32_t wood, uint32_t scaffoldWood);
/// Craftsmen look for workshops this far off, in metres
inline constexpr float k_WorkshopReach = 500.0f;

/// A workshop of a town as a craftsman weighs it
struct WorkshopFacts
{
	float metresAway {0.0f};
	float wantsWood {0.0f};
	bool functional {true};
};
/// The workshop a craftsman supplies: the one wanting wood most for how near it is, the first of equals in the town's
/// order; none when none wants any or none works
[[nodiscard]] std::optional<size_t> BestWorkshop(std::span<const WorkshopFacts> workshops);

/// The wood a craftsman can still carry, as the game counts it (in a signed 16-bit count)
[[nodiscard]] int32_t CarryRoom(uint32_t maxCarried, uint32_t held);

} // namespace openblack::disciple_work
